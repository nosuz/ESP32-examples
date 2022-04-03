## Spreadsheet

Enable SpreadSheet API

Require one of the next scopes.

- https://www.googleapis.com/auth/drive
- https://www.googleapis.com/auth/drive.file
- https://www.googleapis.com/auth/spreadsheets

### append values

```
POST https://sheets.googleapis.com/v4/spreadsheets/{spreadsheetId}/values/{range}:append

range is like sheetname!A1

query params
ValueInputOption: USER_ENTERED
insertDataOption: INSERT_ROWS

values are passed by JSON format.

Exapmle.

curl -X POST 'https://sheets.googleapis.com/v4/spreadsheets/{spreadsheetId}/values/Sheet1!A1:append?insertDataOption=INSERT_ROWS&valueInputOption=USER_ENTERED' -H "Authorization: Bearer {ACCESS_TOKEN}" -H "Content-type: application/json" -d '{"values": [[ "2022/03/22 09:33", 21.5 ]]}'

```

## Read Google spreadsheet from RStudio

1. Run once `gs4_auth()` on RStudio console.
2. Select 0
3. Check Spreadsheet scope and accept.
4. Authentification data is cached.

```{r}
> gs4_auth()
→ The googlesheets4 package is requesting access to your Google account
  Select a pre-authorised account or enter '0' to obtain a new token
  Press Esc/Ctrl + C to cancel

1: MY_GOOGLE_MAIL_ADDRESS

Selection: 0
Waiting for authentication in browser...
Press Esc/Ctrl + C to abort
Authentication complete.
```

## Panic Stack overflow

HTTP access to Google API consumes a lot of memory and sometimes raises stack overflow panic. To avoid this error, increase `Main task stack size` for example 5120 bytes in `Component config > ESP System Settings`.

Component config > ESP System Settings

- Panic handler behaviour
- Main task stack size

### Heap and stack memory

Get remaining heap and stack memory size.

```
ESP_LOGI(TAG, "free heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
ESP_LOGI(TAG, "stack level: %d", uxTaskGetStackHighWaterMark(NULL));
```

## Reference

- [Web サーバーアプリケーションに OAuth2.0 を使用する](https://developers.google.com/identity/protocols/oauth2/web-server)
- [OAuth 2.0 を使用して Google API にアクセスする方法](https://blog.shinonome.io/google-api/)
- [Sheets API](https://developers.google.com/sheets/api)
