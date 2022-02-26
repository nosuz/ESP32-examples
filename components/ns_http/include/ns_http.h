typedef struct
{
    int64_t size;
    char *body;
} HTTP_CONTENT;

esp_err_t ns_http_get(char *url, unsigned int timeout_sec, HTTP_CONTENT *content);
// timeout_sec: specify timeout in second.
// content: if the returned content is not required, set NULL.

esp_err_t ns_http_post(char *url, unsigned int timeout_sec, char *post_content_type, char *post_data, HTTP_CONTENT *content);
esp_err_t ns_http_auth_basic_post(char *url, char *user_name, char *password, unsigned int timeout_sec, char *post_content_type, char *post_data, HTTP_CONTENT *content);
esp_err_t ns_http_auth_bearer_post(char *url, char *bearer_token, unsigned int timeout_sec, char *post_content_type, char *post_data, HTTP_CONTENT *content);
