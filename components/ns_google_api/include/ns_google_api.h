esp_err_t google_api_init(void);
void google_api_params_init(void);
void google_api_param(const char *key, const char *value);
void google_shreadsheet_append(const char *spreadsheet_id, const char *sheet_range, const char *values);
void gmaile_formatdate(char *strftime_buf, size_t length, struct tm *timeinfo);
void gmail_create_draft(const char *message);
void gmail_send(const char *message);
