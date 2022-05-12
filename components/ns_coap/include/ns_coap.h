#include "coap3/coap.h"

void coap_log_handler(coap_log_t level, const char *message);
esp_err_t coap_post_json(const char *server_uri, const char *json_data);
