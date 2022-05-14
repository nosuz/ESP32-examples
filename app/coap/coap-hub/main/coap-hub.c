/*

This code is based on the esp-idf example.
https://github.com/espressif/esp-idf/tree/master/examples/protocols/coap_server

*/

// #include <string.h>
// #include <sys/socket.h>
// #include <netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "cJSON.h"
#include "esp_log.h"

#include "ns_wifi.h"
#include "ns_coap.h"

#define COAP_LOG_LEVEL ESP_LOG_DEBUG

const static char *TAG = "coap";

static RingbufHandle_t rb_handle = NULL;

/*
 * The resource handler
 */
static void
hnd_coap_post(coap_resource_t *resource,
              coap_session_t *session,
              const coap_pdu_t *request,
              const coap_string_t *query,
              coap_pdu_t *response)
{
    size_t size;
    size_t offset;
    size_t total;
    const unsigned char *data;
    char *payload;

    ESP_LOGI(TAG, "POST");
    // coap_resource_notify_observers(resource, NULL);

    /* coap_get_data_large() sets size to 0 on error */
    (void)coap_get_data_large(request, &size, &data, &offset, &total);

    if (size > 0)
    {
        // esp-idf/components/coap/libcoap/include/coap3/pdu.h
        // COAP_RESPONSE_CODE(201)
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);

        // ESP_LOGI(TAG, "size: %d, offset: %d, total: %d", size, offset, total);
        // ESP_LOG_BUFFER_HEXDUMP(TAG, data, size, ESP_LOG_INFO);
        payload = malloc(size + 1);
        if (payload != NULL)
        {
            // Added \0 for string termination. data is not terminated by \0.
            memcpy(payload, data, size);
            payload[size] = '\0';

            UBaseType_t res = xRingbufferSend(rb_handle, payload, size + 1, pdMS_TO_TICKS(1000));
            if (res != pdTRUE)
            {
                ESP_LOGE(TAG, "Failed to send item");
            }
            free(payload);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to malloc for payload");
        }
    }
    else
    {
        ESP_LOGW(TAG, "No payload");
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
    }
}

static void coap_hub_task(void *p)
{
    coap_context_t *ctx = NULL;
    coap_address_t serv_addr;
    coap_resource_t *resource = NULL;

    coap_set_log_handler(coap_log_handler);
    coap_set_log_level(COAP_LOG_LEVEL);

    while (1)
    {
        coap_endpoint_t *ep = NULL;
        unsigned wait_ms;

        /* Prepare the CoAP server socket */
        coap_address_init(&serv_addr);
        serv_addr.addr.sin6.sin6_family = AF_INET6;
        serv_addr.addr.sin6.sin6_port = htons(COAP_DEFAULT_PORT);

        ctx = coap_new_context(NULL);
        if (!ctx)
        {
            ESP_LOGE(TAG, "coap_new_context() failed");
            continue;
        }
        coap_context_set_block_mode(ctx,
                                    COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);

        ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_UDP);
        if (!ep)
        {
            ESP_LOGE(TAG, "udp: coap_new_endpoint() failed");
            goto clean_up;
        }
        ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_TCP);
        if (!ep)
        {
            ESP_LOGE(TAG, "tcp: coap_new_endpoint() failed");
            goto clean_up;
        }

        // coap://esp32.loacl/env
        resource = coap_resource_init(coap_make_str_const("env"), 0);
        if (!resource)
        {
            ESP_LOGE(TAG, "coap_resource_init() failed");
            goto clean_up;
        }
        coap_register_handler(resource, COAP_REQUEST_POST, hnd_coap_post);
        // /* We possibly want to Observe the GETs */
        // coap_resource_set_get_observable(resource, 1);
        coap_add_resource(ctx, resource);

        wait_ms = COAP_RESOURCE_CHECK_TIME * 1000;

        ESP_LOGI(TAG, "Start CoAP service");
        ns_start_mdns();
        ns_add_mdns("_coap", "_udp", 5683);
        ns_add_mdns("_coap", "_tcp", 5683);

        while (1)
        {
            int result = coap_io_process(ctx, wait_ms);
            if (result < 0)
            {
                break;
            }
            else if (result && (unsigned)result < wait_ms)
            {
                /* decrement if there is a result wait time returned */
                wait_ms -= result;
            }
            if (result)
            {
                /* result must have been >= wait_ms, so reset wait_ms */
                wait_ms = COAP_RESOURCE_CHECK_TIME * 1000;
            }
        }
    }
clean_up:
    coap_free_context(ctx);
    coap_cleanup();

    vTaskDelete(NULL);
}

void app_main(void)
{
    // https://docs.espressif.com/projects/esp-idf/en/v4.4.1/esp32/api-reference/system/freertos_additions.html#ring-buffers
    rb_handle = xRingbufferCreate(1028, RINGBUF_TYPE_NOSPLIT);
    if (rb_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create ring buffer");
        return;
    }

    ESP_LOGI(TAG, "Init Wifi");
    wifi_init();
    ESP_LOGI(TAG, "Try to connect");
    wifi_connect();

    ESP_LOGI(TAG, "Wait connection");
    if (wifi_wait_connection())
    {
        ESP_LOGI(TAG, "Connected");
        // How do we handle WiFi connection is failed? reboot?
        xTaskCreate(coap_hub_task, "coap", 8 * 1024, NULL, 5, NULL);

        while (1)
        {
            // block by ring buffer
            // wait data and append to Google spreadsheet
            size_t item_size;
            char *item = (char *)xRingbufferReceive(rb_handle, &item_size, portMAX_DELAY);
            if (item)
            {
                // Payload sample
                // {"id" : "34:ab:95:5a:9b:d0", "bat" : 0.42599999904632568, "t_ad" : 23.25, "t_sh" : 0, "h_sh" : 0, "cnt" : 1, "fin" : true}
                ESP_LOGI(TAG, "Payload: %s", item);
                cJSON *json_data = cJSON_Parse(item);
                if (json_data == NULL)
                {
                    const char *error_ptr = cJSON_GetErrorPtr();
                    if (error_ptr != NULL)
                    {
                        ESP_LOGE(TAG, "%s", error_ptr);
                    }
                    goto end;
                }

                // Google Spreadsheet
            end:
                cJSON_Delete(json_data);
            }
            vRingbufferReturnItem(rb_handle, (void *)item);
        }
    }
    else
    {
        ESP_LOGW(TAG, "Failed to connect");
    }
}
