esp_err_t google_api_init(void);
void google_api_params_init(void);
void google_api_param(const char *key, const char *value);
void google_shreadsheet_append(const char *spreadsheet_id, const char *sheet_range, const char *values);
