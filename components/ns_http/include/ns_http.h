#include "esp_http_client.h"

typedef struct
{
    int64_t size;
    char *body;
} HTTP_CONTENT;

typedef struct http_header
{
    char *key;
    char *value;
    struct http_header *next;
} HTTP_HEADER;

typedef struct
{
    esp_http_client_config_t config;
    HTTP_HEADER *header;
    HTTP_HEADER *last_header;
    char *post_data;
    unsigned int data_size;
} HTTP_STRUCT;

HTTP_STRUCT *ns_http_get(const char *url);
HTTP_STRUCT *ns_http_post(const char *url, const char *content_type, const char *data, unsigned int data_size);
void ns_http_set_timeout(HTTP_STRUCT *http, unsigned int timeout_sec);
void ns_http_auth_basic(HTTP_STRUCT *http, const char *user_name, const char *password);
void ns_http_auth_bearer(HTTP_STRUCT *http, const char *access_token);
void ns_http_set_header(HTTP_STRUCT *http, const char *key, const char *value);
esp_err_t ns_http_send(HTTP_STRUCT *http, HTTP_CONTENT *content);
void ns_http_free(HTTP_STRUCT *http);

HTTP_CONTENT ns_http_default_content(void);
void ns_http_content_cleanup(HTTP_CONTENT *content);
