/*

This code is based on the esp-idf example.
https://github.com/espressif/esp-idf/tree/master/examples/protocols/coap_client

*/

#include <sys/socket.h>
#include <netdb.h>

#include "esp_log.h"

#include "ns_wifi.h"

#include "ns_coap.h"

#define COAP_DEFAULT_TIME_SEC 60
#define COAP_LOG_LEVEL ESP_LOG_INFO

#define COAP_URI CONFIG_COAP_URI

const static char *TAG = "coap";

bool resp_wait = true;
coap_optlist_t *optlist = NULL;
int wait_ms;

coap_response_t message_handler(coap_session_t *session,
                                const coap_pdu_t *sent,
                                const coap_pdu_t *received,
                                const coap_mid_t mid)
{
    const unsigned char *data = NULL;
    size_t data_len;
    size_t offset;
    size_t total;
    coap_pdu_code_t rcvd_code = coap_pdu_get_code(received);

    if (COAP_RESPONSE_CLASS(rcvd_code) == 2)
    {
        if (coap_get_data_large(received, &data_len, &data, &offset, &total))
        {
            if (data_len != total)
            {
                ESP_LOGI(TAG, "Unexpected partial data received offset %u, length %u\n", offset, data_len);
            }
            ESP_LOGI(TAG, "Received: %.*s", (int)data_len, data);
            resp_wait = false;
        }
        return COAP_RESPONSE_OK;
    }
    ESP_LOGI(TAG, "%d.%02d", (rcvd_code >> 5), rcvd_code & 0x1F);
    if (coap_get_data_large(received, &data_len, &data, &offset, &total))
    {
        printf(": ");
        while (data_len--)
        {
            printf("%c", isprint(*data) ? *data : '.');
            data++;
        }
        printf("\n");
    }
    resp_wait = false;
    return COAP_RESPONSE_OK;
}

void coap_log_handler(coap_log_t level, const char *message)
{
    uint32_t esp_level = ESP_LOG_INFO;
    char *cp = strchr(message, '\n');

    if (cp)
        ESP_LOG_LEVEL(esp_level, TAG, "%.*s", (int)(cp - message), message);
    else
        ESP_LOG_LEVEL(esp_level, TAG, "%s", message);
}

static coap_address_t *
coap_get_address(coap_uri_t *uri)
{
    static coap_address_t dst_addr;
    char *phostname = NULL;
    struct addrinfo hints;
    struct addrinfo *addrres;
    int error;
    char tmpbuf[INET6_ADDRSTRLEN];

    phostname = (char *)calloc(1, uri->host.length + 1);
    if (phostname == NULL)
    {
        ESP_LOGE(TAG, "calloc failed");
        return NULL;
    }
    memcpy(phostname, uri->host.s, uri->host.length);

    memset((char *)&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family = AF_UNSPEC;

    error = getaddrinfo(phostname, NULL, &hints, &addrres);
    if (error != 0)
    {
        ESP_LOGE(TAG, "DNS lookup failed for destination address %s. error: %d", phostname, error);
        free(phostname);
        return NULL;
    }
    if (addrres == NULL)
    {
        ESP_LOGE(TAG, "DNS lookup %s did not return any addresses", phostname);
        free(phostname);
        return NULL;
    }
    free(phostname);
    coap_address_init(&dst_addr);
    switch (addrres->ai_family)
    {
    case AF_INET:
        memcpy(&dst_addr.addr.sin, addrres->ai_addr, sizeof(dst_addr.addr.sin));
        dst_addr.addr.sin.sin_port = htons(uri->port);
        inet_ntop(AF_INET, &dst_addr.addr.sin.sin_addr, tmpbuf, sizeof(tmpbuf));
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", tmpbuf);
        break;
    case AF_INET6:
        memcpy(&dst_addr.addr.sin6, addrres->ai_addr, sizeof(dst_addr.addr.sin6));
        dst_addr.addr.sin6.sin6_port = htons(uri->port);
        inet_ntop(AF_INET6, &dst_addr.addr.sin6.sin6_addr, tmpbuf, sizeof(tmpbuf));
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", tmpbuf);
        break;
    default:
        ESP_LOGE(TAG, "DNS lookup response failed");
        return NULL;
    }
    freeaddrinfo(addrres);

    return &dst_addr;
}

static int
coap_build_optlist(coap_uri_t *uri)
{
#define BUFSIZE 40
    unsigned char _buf[BUFSIZE];
    unsigned char *buf;
    size_t buflen;
    int res;

    optlist = NULL;

    if (uri->scheme == COAP_URI_SCHEME_COAPS)
    {
        ESP_LOGE(TAG, "Secure CoAP is not supported");
        return 0;
    }
    if (uri->scheme == COAP_URI_SCHEME_COAPS_TCP)
    {
        ESP_LOGE(TAG, "CoAP over TSL is not supported");
        return 0;
    }

    if (uri->path.length)
    {
        buflen = BUFSIZE;
        buf = _buf;
        res = coap_split_path(uri->path.s, uri->path.length, buf, &buflen);

        while (res--)
        {
            coap_insert_optlist(&optlist,
                                coap_new_optlist(COAP_OPTION_URI_PATH,
                                                 coap_opt_length(buf),
                                                 coap_opt_value(buf)));

            buf += coap_opt_size(buf);
        }
    }

    if (uri->query.length)
    {
        buflen = BUFSIZE;
        buf = _buf;
        res = coap_split_query(uri->query.s, uri->query.length, buf, &buflen);

        while (res--)
        {
            coap_insert_optlist(&optlist,
                                coap_new_optlist(COAP_OPTION_URI_QUERY,
                                                 coap_opt_length(buf),
                                                 coap_opt_value(buf)));

            buf += coap_opt_size(buf);
        }
    }
    return 1;
}

esp_err_t coap_post_json(const char *server_uri, const char *json_data)
{
    coap_address_t *dst_addr;
    coap_uri_t uri;
    coap_context_t *ctx = NULL;
    coap_session_t *session = NULL;
    coap_pdu_t *request = NULL;
    unsigned char token[8];
    size_t tokenlength;

    /* Set up the CoAP logging */
    coap_set_log_handler(coap_log_handler);
    coap_set_log_level(COAP_LOG_LEVEL);

    /* Set up the CoAP context */
    ctx = coap_new_context(NULL);
    if (!ctx)
    {
        ESP_LOGE(TAG, "coap_new_context() failed");
        goto clean_up;
    }
    coap_context_set_block_mode(ctx,
                                COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);

    coap_register_response_handler(ctx, message_handler);

    if (coap_split_uri((const uint8_t *)server_uri, strlen(server_uri), &uri) == -1)
    {
        ESP_LOGE(TAG, "CoAP server uri error: %s", server_uri);
        goto clean_up;
    }
    if (!coap_build_optlist(&uri))
        goto clean_up;

    dst_addr = coap_get_address(&uri);
    if (!dst_addr)
        goto clean_up;

    /*
     * Note that if the URI starts with just coap:// (not coaps://) the
     * session will still be plain text.
     *
     * coaps+tcp:// is NOT yet supported by the libcoap->mbedtls interface
     * so COAP_URI_SCHEME_COAPS_TCP will have failed in a test above,
     * but the code is left in for completeness.
     */
    if (uri.scheme == COAP_URI_SCHEME_COAPS || uri.scheme == COAP_URI_SCHEME_COAPS_TCP)
    {
        ESP_LOGE(TAG, "MbedTLS (D)TLS Client Mode not configured");
        goto clean_up;
    }

    session = coap_new_client_session(ctx, NULL, dst_addr,
                                      uri.scheme == COAP_URI_SCHEME_COAP_TCP ? COAP_PROTO_TCP : COAP_PROTO_UDP);
    if (!session)
    {
        ESP_LOGE(TAG, "coap_new_client_session() failed");
        goto clean_up;
    }

    request = coap_new_pdu(coap_is_mcast(dst_addr) ? COAP_MESSAGE_NON : COAP_MESSAGE_CON,
                           //    COAP_REQUEST_CODE_GET, session);
                           COAP_REQUEST_CODE_POST, session);
    if (!request)
    {
        ESP_LOGE(TAG, "coap_new_pdu() failed");
        goto clean_up;
    }
    /* Add in an unique token */
    coap_session_new_token(session, &tokenlength, token);
    coap_add_token(request, tokenlength, token);

    /*
     * To make this a POST, you will need to do the following
     * Change COAP_REQUEST_CODE_GET to COAP_REQUEST_CODE_POST for coap_new_pdu()
     * Add in here a Content-Type Option based on the format of the POST text.  E.G. for JSON
     *   u_char buf[4];
     *   coap_insert_optlist(&optlist,
     *                       coap_new_optlist(COAP_OPTION_CONTENT_FORMAT,
     *                                        coap_encode_var_safe (buf, sizeof (buf),
     *                                                              COAP_MEDIATYPE_APPLICATION_JSON),
     *                                        buf));
     * Add in here the POST data of length length. E.G.
     *   coap_add_data_large_request(session, request length, data, NULL, NULL);
     */
    u_char buf[4];
    coap_insert_optlist(&optlist,
                        coap_new_optlist(COAP_OPTION_CONTENT_FORMAT,
                                         coap_encode_var_safe(buf, sizeof(buf),
                                                              COAP_MEDIATYPE_APPLICATION_JSON),
                                         buf));

    coap_add_data_large_request(session, request, strlen(json_data), (u_char *)json_data, NULL, NULL);

    coap_add_optlist_pdu(request, &optlist);

    resp_wait = true;
    coap_send(session, request);

    wait_ms = COAP_DEFAULT_TIME_SEC * 1000;

    while (resp_wait)
    {
        int result = coap_io_process(ctx, wait_ms > 1000 ? 1000 : wait_ms);
        if (result >= 0)
        {
            if (result >= wait_ms)
            {
                ESP_LOGE(TAG, "No response from server");
                break;
            }
            else
            {
                wait_ms -= result;
            }
        }
    }

clean_up:
    if (optlist)
    {
        coap_delete_optlist(optlist);
        optlist = NULL;
    }
    if (session)
    {
        coap_session_release(session);
    }
    if (ctx)
    {
        coap_free_context(ctx);
    }
    coap_cleanup();

    ESP_LOGI(TAG, "Finished");

    return ESP_OK;
}
