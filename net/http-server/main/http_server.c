#include "esp_log.h"
#include "esp_http_server.h"

esp_err_t root_dir_handler(httpd_req_t *req)
{
    const char resp[] = "URI GET Response";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t root_dir = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_dir_handler,
    .user_ctx = NULL};

httpd_uri_t say_hello = {
    .uri = "/hello",
    .method = HTTP_GET,
    // .handler = say_hello_handler,
    .handler = root_dir_handler,
    .user_ctx = NULL};

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &root_dir);
        httpd_register_uri_handler(server, &say_hello);
    }
    return server;
}

void stop_webserver(httpd_handle_t server)
{
    if (server)
    {
        httpd_stop(server);
    }
}
