// Ref.
// HMAC-SHA1
// https://techtutorialsx.com/2018/01/25/esp32-arduino-applying-the-hmac-sha-256-mechanism/
// https://m.blog.naver.com/PostView.naver?isHttpsRedirect=true&blogId=chandong83&logNo=222140414536

// Base64 encoding
// https://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"

#include "mbedtls/md.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"

#include "esp_http_client.h"

#include "ns_twitter.h"
#include "ns_heap.h"

#define MAX_PARAMS 16
#define MAX_KEY_LENGTH 31
#define MAX_VALUE_LENGTH 511

#define BUFFER_BLOCK_SIZE (1024)

// #define DEBUG

static const char *TAG = "twitter";

static char encoding_table[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'};
static int mod_table[] = {0, 2, 1};

int param_index;
char dict_key[MAX_PARAMS][MAX_KEY_LENGTH];
char dict_value[MAX_PARAMS][MAX_VALUE_LENGTH];
bool dict_param_flag[MAX_PARAMS];

char *base64_encode(unsigned char *data,
                    uint16_t input_length)
{

    uint16_t output_length = 4 * ((input_length + 2) / 3);
    char *encoded_data = fake_malloc(sizeof(char) * (output_length + 1));
    if (encoded_data == NULL)
        return NULL;

    for (int i = 0, j = 0; i < input_length;)
    {
        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (int i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[output_length - 1 - i] = '=';
    encoded_data[output_length] = '\0';

    return encoded_data;
}

char tohex(uint8_t c)
{
    return c > 9 ? 'A' + (c - 10) : '0' + c;
}

char *percent_encode(char *param)
{
    uint16_t param_length = strlen(param);
    char c;

    for (int i = 0; i < strlen(param); i++)
    {
        c = param[i];
        if (!(isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~'))
            param_length += 2;
    }

    char *encoded = fake_malloc(sizeof(char) * (param_length + 1));

    uint16_t length = 0;
    for (int i = 0; i < strlen(param); i++)
    {
        c = param[i];
        if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~')
        {
            encoded[length++] = c;
        }
        else
        {
            encoded[length++] = '%';
            encoded[length++] = tohex(c >> 4);
            encoded[length++] = tohex(c & 0x0F);
        }
    }
    encoded[length] = '\0';

    return encoded;
}

typedef struct content_struct
{
    int64_t size;
    char *body;
} content_struct;

esp_err_t
http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        // ESP_LOGI(
        //     TAG,
        //     "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
        //     evt->header_key,
        //     evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        // Can handle both pages having Content-Length and chunked pages.
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (evt->user_data == NULL)
            break;
        content_struct *content = evt->user_data;
        int64_t new_content_size = content->size + evt->data_len;
        if (content->body != NULL)
        {
            // check the allocated memory size and expand it if needed.
            int cur_block_size = (content->size + 1) / BUFFER_BLOCK_SIZE + 1;
            int new_block_size = (new_content_size + 1) / BUFFER_BLOCK_SIZE + 1;
            if (cur_block_size < new_block_size)
            {
                char *tmp = (char *)realloc(content->body, new_block_size * BUFFER_BLOCK_SIZE);
                if (tmp != NULL)
                {
                    // ESP_LOGI(TAG, "Expanded to %d blocks.", new_block_size);
                    content->body = tmp;
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to realloc to %d blocks.", new_block_size);
                    break;
                }
            }

            memcpy(content->body + content->size, evt->data, evt->data_len);
            content->body[new_content_size] = '\0';
        }
        content->size = new_content_size;

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
        if (err != 0)
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        break;
    default:
        ESP_LOGI(TAG, "Unknown event id: %d", evt->event_id);
    }
    return ESP_OK;
}

void twitter_append_oauth(char *key, char *value)
{
    strcpy(dict_key[param_index], key);
    strcpy(dict_value[param_index], value);
    dict_param_flag[param_index] = false;

    param_index++;
}

void twitter_set_api_param(char *key, char *value)
{
    strcpy(dict_key[param_index], key);
    strcpy(dict_value[param_index], value);
    dict_param_flag[param_index] = true;

    param_index++;
}

void twitter_init_api_params(void)
{
    char oauth_nonce[32];
    char oauth_timestamp[16];

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    sprintf(oauth_timestamp, "%ld", now);
    strftime(oauth_nonce, sizeof(oauth_nonce), "%FT%T", &timeinfo);
    for (int i = strlen(oauth_nonce); i < 32; i++)
    {
        oauth_nonce[i] = rand() % 0x100;
    }
    ESP_LOG_BUFFER_HEXDUMP(TAG, oauth_nonce, 32, ESP_LOG_INFO);

    param_index = 0;

    twitter_append_oauth("oauth_consumer_key", CONFIG_CONSUMER_TOKEN);
    twitter_append_oauth("oauth_token", CONFIG_OAUTH_TOKEN);

    char *encoded_nonce = base64_encode((unsigned char *)oauth_nonce, 32);
#ifndef DEBUG
    twitter_append_oauth("oauth_nonce", encoded_nonce);
#else
    twitter_append_oauth("oauth_nonce", "kYjzVBB8Y0ZFabxSWbWovY3uYSQ2pTgmZeNu2VS4cg");
#endif
    fake_free(encoded_nonce);

#ifndef DEBUG
    twitter_append_oauth("oauth_timestamp", oauth_timestamp);
#else
    twitter_append_oauth("oauth_timestamp", "1318622958");
#endif
    twitter_append_oauth("oauth_signature_method", "HMAC-SHA1");
    twitter_append_oauth("oauth_version", "1.0");
}

char *concat_params(void)
{
    char tmp[MAX_KEY_LENGTH > MAX_VALUE_LENGTH ? MAX_KEY_LENGTH : MAX_VALUE_LENGTH]; // use longer length
    bool tmp_flag;

    // sort by key and value
    for (int i = 0; i < (param_index - 1); i++)
    {
        int min_index = i;
        for (int j = i + 1; j < param_index; j++)
        {
            if (strcmp(dict_key[min_index], dict_key[j]) > 0)
                min_index = j;
        }

        if (i != min_index)
        {
            strcpy(tmp, dict_key[i]);
            strcpy(dict_key[i], dict_key[min_index]);
            strcpy(dict_key[min_index], tmp);

            strcpy(tmp, dict_value[i]);
            strcpy(dict_value[i], dict_value[min_index]);
            strcpy(dict_value[min_index], tmp);

            tmp_flag = dict_param_flag[i];
            dict_param_flag[i] = dict_param_flag[min_index];
            dict_param_flag[min_index] = tmp_flag;
        }
    }

    char *params = fake_malloc(sizeof(char));
    params[0] = '\0';
    for (int i = 0; i < param_index; i++)
    {
        uint16_t new_length = strlen(params) + 1;
        if (i > 0)
            new_length += 1;
        new_length += strlen(dict_key[i]);
        char *encoded_value = percent_encode(dict_value[i]);
        new_length += 1;
        new_length += strlen(encoded_value);

        char *realloced = fake_realloc(params, new_length);
        if (realloced != NULL)
        {
            params = realloced;
            // printf("key: %s\n", dict_key[i]);
            // printf("val: %s\n", encoded_value);
            if (i > 0)
                strcat(params, "&");
            strcat(params, dict_key[i]);
            strcat(params, "=");
            strcat(params, encoded_value);
        }
        fake_free(encoded_value);
    }

    return params;
}

void append_signature(char *_method, char *url)
{
    // make method name to upper cases.
    char method[strlen(_method) + 1];
    for (int i = 0; i < strlen(_method); i++)
    {
        method[i] = toupper(_method[i]);
    }
    method[strlen(_method)] = '\0';

    char *consumer_secret = percent_encode(CONFIG_CONSUMER_SECRET);
    char *oauth_secret = percent_encode(CONFIG_OAUTH_SECRET);

    char *key = fake_malloc(sizeof(char) * (strlen(consumer_secret) + 1 + strlen(oauth_secret) + 1));
    strcpy(key, consumer_secret);
    key[strlen(consumer_secret)] = '&';
    strcpy(key + strlen(consumer_secret) + 1, oauth_secret);
    fake_free(consumer_secret);
    fake_free(oauth_secret);

    char *params = concat_params();
    ESP_LOGI(TAG, "Params: %s", params);

    char *encoded_url = percent_encode(url);
    char *encoded_params = percent_encode(params);
    fake_free(params);
    char *sign_base = fake_malloc(sizeof(char) * (strlen(method) + 1 + strlen(encoded_url) + 1 + strlen(encoded_params) + 1));
    strcpy(sign_base, method);
    strcat(sign_base, "&");
    strcat(sign_base, encoded_url);
    fake_free(encoded_url);
    strcat(sign_base, "&");
    strcat(sign_base, encoded_params);
    fake_free(encoded_params);

    ESP_LOGI(TAG, "key: %s", key);
    ESP_LOGI(TAG, "Sign base: %s", sign_base);

    uint8_t hmac[20];
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA1;
    const uint16_t baseLength = strlen(sign_base);
    const uint16_t keyLength = strlen(key);

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)key, keyLength);
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)sign_base, baseLength);
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);
    fake_free(key);
    fake_free(sign_base);

    ESP_LOGI(TAG, "Dump signed param");
    ESP_LOG_BUFFER_HEXDUMP(TAG, hmac, 20, ESP_LOG_INFO);

    char *encode_signed_param = base64_encode(hmac, 20);
    ESP_LOGI(TAG, "Base64 encoded: %s", encode_signed_param);

    twitter_append_oauth("oauth_signature", encode_signed_param);
    fake_free(encode_signed_param);
}

char *make_oauth_header(char *method, char *url)
{
    for (int i = 0; i < param_index; i++)
    {
        ESP_LOGI(TAG, "params(%d): %s -> %s", i, dict_key[i], dict_value[i]);
    }

    append_signature(method, url);

    regex_t preg;
    const char pattern[] = "^oauth_";
    regmatch_t patternMatch[1];
    regcomp(&preg, pattern, REG_EXTENDED | REG_NEWLINE);
    int size = sizeof(patternMatch) / sizeof(regmatch_t);

    char *oauth_header = fake_malloc(sizeof(char) * (strlen("OAuth ") + 1));
    strcpy(oauth_header, "OAuth ");

    bool first_item = true;
    for (int i = 0; i < param_index; i++)
    {
        if (regexec(&preg, dict_key[i], size, patternMatch, 0) == 0)
        {
            char *encoded_value = percent_encode(dict_value[i]);

            uint16_t new_length = strlen(oauth_header) + 1;
            if (!first_item)
                new_length += 1;                   // for ','
            new_length += strlen(dict_key[i]) + 2; // key + '="'
            new_length += strlen(encoded_value);   // for percent encoded value
            new_length += 1;                       // for '"'
            char *realloced = fake_realloc(oauth_header, sizeof(char) * new_length);
            if (realloced == NULL)
            {
                ESP_LOGE(TAG, "Failed realloc(oauth_header, %d): %s", new_length, dict_key[i]);
            }
            else
            {
                oauth_header = realloced;
                if (first_item)
                {
                    first_item = false;
                }
                else
                {
                    strcat(oauth_header, ",");
                }
                strcat(oauth_header, dict_key[i]);
                strcat(oauth_header, "=\"");
                strcat(oauth_header, encoded_value);
                strcat(oauth_header, "\"");
            }
            fake_free(encoded_value);
        }
    }

    regfree(&preg);
    return oauth_header;
}

char *make_post_data(void)
{
    char *post_data = fake_malloc(sizeof(char) * 1);
    post_data[0] = '\0';

    bool first_item = true;
    for (int i = 0; i < param_index; i++)
    {
        if (!dict_param_flag[i])
            continue;

        uint16_t new_length = strlen(post_data) + 1;
        if (!first_item)
            new_length += 1;                   // for '&'
        new_length += strlen(dict_key[i]) + 1; // key + '='
        char *encoded_value = percent_encode(dict_value[i]);
        new_length += strlen(encoded_value);
        char *realloced = fake_realloc(post_data, sizeof(char) * new_length); // expand memory
        if (realloced == NULL)
        {
            ESP_LOGE(TAG, "Failed realloc(post_data, %d): %s", new_length, dict_key[i]);
        }
        else
        {
            post_data = realloced;
            if (first_item)
            {
                first_item = false;
            }
            else
            {
                strcat(post_data, "&");
            }
            strcat(post_data, dict_key[i]);
            strcat(post_data, "=");
            strcat(post_data, encoded_value);
        }
        fake_free(encoded_value);
    }

    return post_data;
}

void twitter_update_status(void)
{
    const char *method = "post";
    const char *url = "https://api.twitter.com/1.1/statuses/update.json";

    char *oauth_header = make_oauth_header(method, url);
    ESP_LOGI(TAG, "OAuth Header: %s", oauth_header);

    char *post_data = make_post_data();
    ESP_LOGI(TAG, "Post data: %s", post_data);

    content_struct content = {
        .size = 0,
        .body = malloc(sizeof(char) * BUFFER_BLOCK_SIZE),
    };

    esp_http_client_config_t config = {
        .url = url,
        // .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = &content,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // POST
    esp_http_client_set_header(client, "Authorization", oauth_header);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
        printf("%s\n", content.body);
    }
    else
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(content.body);

    fake_free(oauth_header);
    fake_free(post_data);
    init_fake_heap();
}
