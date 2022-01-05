#include <string.h>
#include <fcntl.h>

#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_vfs.h"
#include "cJSON.h"

#include "gpio.h"

#define BASE_DIR CONFIG_WEB_MOUNT_POINT

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

static const char *TAG = "http";

char *scratch;

// From ESP32 example restfull_server
#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html"))
    {
        type = "text/html";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".js"))
    {
        type = "application/javascript";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".css"))
    {
        type = "text/css";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".png"))
    {
        type = "image/png";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".ico"))
    {
        type = "image/x-icon";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".svg"))
    {
        type = "text/xml";
    }
    ESP_LOGI(TAG, "Content-type: %s", type);
    return httpd_resp_set_type(req, type);
}

esp_err_t common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    char filepath_gzip[FILE_PATH_MAX];
    // ESP_LOGI(TAG, "scratch address: %x", scratch);

    ESP_LOGI(TAG, "Request URI: %s", req->uri);

    strlcpy(filepath, BASE_DIR, sizeof(BASE_DIR));
    if (req->uri[strlen(req->uri) - 1] == '/')
    {
        strlcat(filepath, "/index.html", sizeof(filepath));
    }
    else
    {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
    strlcat(filepath_gzip, filepath, sizeof(filepath));
    strlcat(filepath_gzip, ".gz", sizeof(filepath));

    int fd = -1;
    if (httpd_req_get_hdr_value_len(req, "Accept-Encoding") > 0)
    {
        httpd_req_get_hdr_value_str(req, "Accept-Encoding", scratch, SCRATCH_BUFSIZE);
        ESP_LOGI(TAG, "Accept-Encoding: %s", scratch);
        if (strstr(scratch, "gzip"))
        {
            ESP_LOGI(TAG, "Open GZIP file: %s", filepath_gzip);
            fd = open(filepath_gzip, O_RDONLY, 0);
            if (fd >= 0)
            {
                ESP_LOGI(TAG, "Content-Encoding: gzip");
                httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
            }
            else
            {
                ESP_LOGI(TAG, "Failed to open GZIP.");
            }
        }
    }

    if (fd == -1)
    {
        // Requested file has no gziped file. Opend requested file.
        ESP_LOGI(TAG, "Open file: %s", filepath);
        fd = open(filepath, O_RDONLY, 0);
        if (fd == -1)
        {
            ESP_LOGE(TAG, "Failed to open file : %s", filepath);
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File open error\n");
            return ESP_FAIL;
        }
    }

    set_content_type_from_file(req, filepath);

    ssize_t read_bytes;
    do
    {
        read_bytes = read(fd, scratch, SCRATCH_BUFSIZE);
        if (read_bytes == -1)
        {
            ESP_LOGE(TAG, "Read file error: %s", filepath);
        }
        else if (read_bytes > 0)
        {
            ESP_LOGI(TAG, "Read file: %d bytes", read_bytes);
            if (httpd_resp_send_chunk(req, scratch, read_bytes) != ESP_OK)
            {
                close(fd);
                ESP_LOGE(TAG, "Failed to sending chunk");
                httpd_resp_sendstr_chunk(req, NULL);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file\n");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    close(fd);
    ESP_LOGI(TAG, "File sending complete");
    httpd_resp_send_chunk(req, NULL, 0);
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

esp_err_t get_led_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Get LED status");
    read_gpio();

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    if (read_led())
    {
        cJSON_AddTrueToObject(root, "led_status");
    }
    else
    {
        cJSON_AddFalseToObject(root, "led_status");
    }
    const char *json = cJSON_Print(root);
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);
    return ESP_OK;
}

httpd_handle_t
start_webserver(void)
{
    scratch = calloc(1, SCRATCH_BUFSIZE);
    if (!scratch)
    {
        ESP_LOGE(TAG, "Failed to callloc");
        return NULL;
    }
    ESP_LOGI(TAG, "scratch area address: %p", scratch);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t say_hello = {
            .uri = "/hello",
            .method = HTTP_GET,
            .handler = say_hello_handler,
            .user_ctx = "Hello World!\n"};
        httpd_register_uri_handler(server, &say_hello);

        httpd_uri_t say_hello2 = {
            .uri = "/hello2",
            .method = HTTP_GET,
            .handler = say_hello2_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &say_hello2);

        httpd_uri_t say_hello_chunk = {
            .uri = "/hello3",
            .method = HTTP_GET,
            .handler = say_hello_chunk_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &say_hello_chunk);

        httpd_uri_t set_led_on = {
            .uri = "/api/led_on",
            .method = HTTP_GET,
            .handler = set_led_handler,
            .user_ctx = "on"};
        httpd_register_uri_handler(server, &set_led_on);

        httpd_uri_t set_led_off = {
            .uri = "/api/led_off",
            .method = HTTP_GET,
            .handler = set_led_handler,
            .user_ctx = "off"};
        httpd_register_uri_handler(server, &set_led_off);

        httpd_uri_t get_led = {
            .uri = "/api/led_status",
            .method = HTTP_GET,
            .handler = get_led_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &get_led);

        httpd_uri_t common_get_uri = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = common_get_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &common_get_uri);
    }
    else
    {
        free(scratch);
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
