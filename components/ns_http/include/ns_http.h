typedef struct
{
    int64_t size;
    char *body;
} HTTP_CONTENT;

esp_err_t ns_http_get(char *url, HTTP_CONTENT *content, unsigned int timeout_sec);
// content: if the returned content is not required, set NULL.
// timeout_sec: specify timeout in second.

