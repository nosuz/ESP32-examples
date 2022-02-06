#include <esp_http_server.h>

httpd_handle_t start_ap_select_web(void);
void stop_ap_select_web(httpd_handle_t server);
