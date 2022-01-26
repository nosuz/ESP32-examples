// Ref.
// HMAC-SHA1
// https://techtutorialsx.com/2018/01/25/esp32-arduino-applying-the-hmac-sha-256-mechanism/
// https://m.blog.naver.com/PostView.naver?isHttpsRedirect=true&blogId=chandong83&logNo=222140414536

// Base64 encoding
// https://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c

#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

#include "mbedtls/md.h"
#include "esp_log.h"

#define MAX_PARAMS 16
#define MAX_KEY_LENGTH 64
#define MAX_VALUE_LENGTH 512

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

    char *encoded_data = malloc(output_length + 1);
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
    encoded_data[output_length] = 0;

    return encoded_data;
}

char *percent_encode(char *param)
{
    uint16_t len = strlen(param);
    char c;

    for (int i = 0; i < strlen(param); i++)
    {
        c = param[i];
        if (!isalnum(c) && c != '-' && c != '.' && c != '_' && c != '~')
            len += 2;
    }

    char *encoded = malloc(len + 1);
    encoded[len] = 0;

    len = 0;
    for (int i = 0; i < strlen(param); i++)
    {
        c = param[i];
        if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~')
        {
            encoded[len] = c;
        }
        else
        {
            encoded[len] = '%';
            sprintf(&encoded[len + 1], "%X", (c >> 4));
            sprintf(&encoded[len + 2], "%X", (c & 0x0F));
            len += 2;
        }
        len++;
    }

    return encoded;
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
    free(encoded_nonce);

#ifndef DEBUG
    twitter_append_oauth("oauth_timestamp", oauth_timestamp);
#else
    twitter_append_oauth("oauth_timestamp", "1318622958");
#endif
    twitter_append_oauth("oauth_signature_method", "HMAC-SHA1");
    twitter_append_oauth("oauth_version", "1.0");
}

void concat_params(char *params)
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

    params[0] = '\0';
    for (int i = 0; i < param_index; i++)
    {
        if (i > 0)
            strcat(params, "&");
        strcat(params, dict_key[i]);
        strcat(params, "=");
        char *encoded_value = percent_encode(dict_value[i]);
        strcat(params, encoded_value);
        free(encoded_value);
    }
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

    char *key = malloc(strlen(consumer_secret) + strlen(oauth_secret) + 1);
    strcpy(key, consumer_secret);
    key[strlen(consumer_secret)] = '&';
    strcpy(key + strlen(consumer_secret) + 1, oauth_secret);
    free(consumer_secret);
    free(oauth_secret);

    char *params = malloc(MAX_PARAMS * (MAX_KEY_LENGTH + MAX_VALUE_LENGTH + 2));
    concat_params(params);
    ESP_LOGI(TAG, "Params: %s", params);

    char *encoded_url = percent_encode(url);
    char *encoded_params = percent_encode(params);
    free(params);
    char *sign_base = malloc(strlen(method) + 1 + strlen(encoded_url) + 1 + strlen(encoded_params) + 1);
    strcpy(sign_base, method);
    strcat(sign_base, "&");
    strcat(sign_base, encoded_url);
    free(encoded_url);
    strcat(sign_base, "&");
    strcat(sign_base, encoded_params);
    free(encoded_params);

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
    free(key);
    free(sign_base);

    ESP_LOGI(TAG, "Dump signed param");
    ESP_LOG_BUFFER_HEXDUMP(TAG, hmac, 20, ESP_LOG_INFO);

    char *encode_signed_param = base64_encode(hmac, 20);
    ESP_LOGI(TAG, "Base64 encoded: %s", encode_signed_param);

    twitter_append_oauth("oauth_signature", encode_signed_param);
    free(encode_signed_param);
}

void make_oauth_header(char *header, char *method, char *url)
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

    header[0] = '\0';
    strcat(header, "OAuth ");
    bool first_item = true;
    for (int i = 0; i < param_index; i++)
    {
        if (regexec(&preg, dict_key[i], size, patternMatch, 0) == 0)
        {
            if (first_item)
            {
                first_item = false;
            }
            else
            {
                strcat(header, ",");
            }
            strcat(header, dict_key[i]);
            strcat(header, "=\"");
            strcat(header, dict_value[i]);
            strcat(header, "\"");
        }
    }

    regfree(&preg);
}

void make_post_data(char *post_data)
{
    post_data[0] = '\0';
    bool first_item = true;

    for (int i = 0; i < param_index; i++)
    {
        if (!dict_param_flag[i])
            continue;

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
        char *encoded_value = percent_encode(dict_value[i]);
        strcat(post_data, encoded_value);
        free(encoded_value);
    }
}

void twitter_update_status(void)
{
    const char *method = "post";
    const char *url = "https://api.twitter.com/1.1/statuses/update.json";

    char *header = malloc(MAX_PARAMS * (MAX_KEY_LENGTH * 2 + MAX_VALUE_LENGTH));
    make_oauth_header(header, method, url);
    ESP_LOGI(TAG, "OAuth Header: %s", header);

    char *post_data = malloc(MAX_PARAMS * (MAX_KEY_LENGTH * 2 + MAX_VALUE_LENGTH));
    make_post_data(post_data);
    ESP_LOGI(TAG, "Post data: %s", post_data);

    free(header);
    free(post_data);
}
