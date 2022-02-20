#include <string.h>
#include <fcntl.h>
#include <sys/param.h> // define MIN

#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"

#include "ns_wifi.h"

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

esp_err_t spiffs_file_get_handler(httpd_req_t *req)
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
    strlcpy(filepath_gzip, filepath, sizeof(filepath));
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

void percent_decode(char *query)
{
    char *decoded;
    decoded = query;

    while (*query != '\0')
    {
        if (*query == '%')
        {
            // fix to Upper
            if (query[1] >= 'a')
                query[1] -= 0x20;
            if (query[2] >= 'a')
                query[2] -= 0x20;

            // decode
            // 'A' = 65
            *decoded = ((query[1] > '@' ? query[1] - 55 : query[1] - '0') << 4) | (query[2] > '@' ? query[2] - 55 : query[2] - '0');
            query += 3;
        }
        else
        {
            *decoded = *query;
            query++;
        }
        decoded++;
    }
    *decoded = '\0';
}

esp_err_t set_ap_handler(httpd_req_t *req)
{
    /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */
    char content[100];

    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0)
    { /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }

    char ssid[64] = "";
    char password[64] = "";

    char content_type[34]; // fit "application/x-www-form-urlencoded";
    size_t content_type_len = httpd_req_get_hdr_value_len(req, "Content-Type");
    if (content_type_len > 0)
    {
        httpd_req_get_hdr_value_str(req, "Content-Type", content_type, 34);
        content_type[content_type_len] = '\0';
        ESP_LOGI(TAG, "Content-Type: %s", content_type);

        if (strcmp(content_type, "application/x-www-form-urlencoded") == 0)
        {
            if (strlen(content) > 0)
            {
                if (httpd_query_key_value(content, "ssid", ssid, 64) == ESP_OK)
                {
                    ssid[64 - 1] = '\0';
                    percent_decode(ssid);
                    ESP_LOGI(TAG, "SSID: %s", ssid);
                }

                if (httpd_query_key_value(content, "password", password, 64) == ESP_OK)
                {
                    password[64 - 1] = '\0';
                    percent_decode(password);
                    ESP_LOGI(TAG, "Password: %s", "XXXXXX");
                }
            }
        }
    }

    // ESP_LOGI(TAG, "posted: %s", content);
    ESP_LOGI(TAG, "Selected SSID: %s", ssid);
    if (strlen(ssid) > 0)
        wifi_set_ap(ssid, password);

    /* Send a simple response */
    const char resp[] = "Accepted Config";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t get_ap_handler(httpd_req_t *req)
{
    uint16_t ap_info_size = 0;
    // wifi_ap_record_t ap_info[CONFIG_SCAN_AP_LIST_SIZE];
    wifi_ap_record_t tmp_info;

    wifi_ap_record_t *ap_info = wifi_get_ap_info(&ap_info_size);

    cJSON *array = cJSON_CreateArray();

    for (int i = 0; i < ap_info_size; i++)
    {
        cJSON *info = cJSON_CreateObject();
        ESP_LOGI(TAG, "Return SSID \t%s", ap_info[i].ssid);
        cJSON_AddNumberToObject(info, "id", i);
        cJSON_AddStringToObject(info, "ssid", &ap_info[i].ssid);
        cJSON_AddNumberToObject(info, "rssi", ap_info[i].rssi);
        cJSON_AddItemToArray(array, info);
    }

    const char *json_data = cJSON_Print(array);
    httpd_resp_sendstr(req, json_data);

    free(json_data);
    cJSON_Delete(array);

    // list_files("/www", NULL);
    return ESP_OK;
}

httpd_handle_t start_ap_select_web(void)
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
        httpd_uri_t set_ap_chunk = {
            .uri = "/api/ap_config",
            .method = HTTP_POST,
            .handler = set_ap_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &set_ap_chunk);

        httpd_uri_t get_ap_chunk = {
            .uri = "/api/ap_list",
            .method = HTTP_GET,
            .handler = get_ap_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &get_ap_chunk);

        httpd_uri_t spiffs_file_get = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = spiffs_file_get_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &spiffs_file_get);
    }
    else
    {
        free(scratch);
    }
    return server;
}

void stop_ap_select_web(httpd_handle_t server)
{
    if (server)
    {
        httpd_stop(server);
        free(scratch);
    }
}
