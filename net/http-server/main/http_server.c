#include "esp_log.h"
#include "esp_http_server.h"

#include "gpio.h"

static const char *TAG = "http";

esp_err_t root_dir_handler(httpd_req_t *req)
{
    const char resp[] = "GET Response\n";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t say_hello_handler(httpd_req_t *req)
{
    const char *resp_str = (const char *)req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t say_hello2_handler(httpd_req_t *req)
{
    char resp[10 + 32 + 3];
    char *buf;
    size_t buf_len;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char name[32];
            bool has_name = false;
            char age[3];
            bool has_age = false;

            if (httpd_query_key_value(buf, "name", name, sizeof(name)) == ESP_OK)
            {
                has_name = true;
                ESP_LOGI(TAG, "Found URL query parameter => name=%s", name);
            }
            if (httpd_query_key_value(buf, "age", age, sizeof(age)) == ESP_OK)
            {
                has_age = true;
                ESP_LOGI(TAG, "Found URL query parameter => age=%s", age);
            }
            if (has_name)
            {
                if (has_age)
                {
                    snprintf(resp, sizeof(resp), "Hello %s (%s)\n", name, age);
                }
                else
                {
                    snprintf(resp, sizeof(resp), "Hello %s\n", name);
                }
            }
            else
            {
                snprintf(resp, sizeof(resp), "Tell me your name.\n");
            }
        }
        else
        {
            ESP_LOGE(TAG, "URL query error");
        }
        free(buf);
    }
    else
    {
        ESP_LOGI(TAG, "URL query = 0");
        snprintf(resp, sizeof(resp), "Hi there\n");
    }
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t say_hello_chunk_handler(httpd_req_t *req)
{
    const char msg1[] = "Hello, \n";
    const char msg2[] = "May I have your name?\n";

    httpd_resp_send_chunk(req, msg1, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, msg2, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "", 0);

    return ESP_OK;
}

esp_err_t set_led_handler(httpd_req_t *req)
{
    const char *status = (const char *)req->user_ctx;
    ESP_LOGI(TAG, "Set LED status %s", status);
    if (strcmp("on", status) == 0)
    {
        set_led(true);
    }
    else if (strcmp("off", status) == 0)
    {
        set_led(false);
    }
    else
    {
        ESP_LOGW(TAG, "Wrong LED status: %s", status);
    }
    httpd_resp_send(req, "OK\n", HTTPD_RESP_USE_STRLEN);

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
    .handler = say_hello_handler,
    .user_ctx = "Hello World!\n"};

httpd_uri_t say_hello2 = {
    .uri = "/hello2",
    .method = HTTP_GET,
    .handler = say_hello2_handler,
    .user_ctx = NULL};

httpd_uri_t say_hello_chunk = {
    .uri = "/hello3",
    .method = HTTP_GET,
    .handler = say_hello_chunk_handler,
    .user_ctx = NULL};

httpd_uri_t set_led_on = {
    .uri = "/led_on",
    .method = HTTP_GET,
    .handler = set_led_handler,
    .user_ctx = "on"};

httpd_uri_t set_led_off = {
    .uri = "/led_off",
    .method = HTTP_GET,
    .handler = set_led_handler,
    .user_ctx = "off"};

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &root_dir);
        httpd_register_uri_handler(server, &say_hello);
        httpd_register_uri_handler(server, &say_hello2);
        httpd_register_uri_handler(server, &say_hello_chunk);
        httpd_register_uri_handler(server, &set_led_on);
        httpd_register_uri_handler(server, &set_led_off);
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
