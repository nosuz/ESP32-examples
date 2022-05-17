// Script Properties, configured in Project Setting
// secret: The key for accpect transaction.
// access_token: The key to access API. Valid only for 2 hours.
// refresh_token: The key to get a new access_token.
// client_id: This id and refresh token is required to get a new access token.
// client_secret: This string is required only when the client is registered as a private client.
// debug: set 1 will records posted JSON data on the log sheet.


function doPost(e) {
  const sheet = SpreadsheetApp.openById(SpreadsheetApp.getActiveSpreadsheet().getId())

  // const token = e.parameter.token  // query param

  const json = e.postData.getDataAsString()
  const payload = JSON.parse(json)

  if (! payload.secret) {
    // no secret key in POSTed data
    return
  }

  const props = PropertiesService.getScriptProperties()
  // If no secret prop., save the first secret in POSTed as the secret prop.
  if (! props.getProperty("secret")){
    props.setProperty("secret", payload.secret)
  } else if (payload.secret != props.getProperty("secret")) {
    return
  }

  // const date_time = Utilities.formatDate(new Date(), 'Asia/Tokyo', 'yyyy/MM/dd HH:mm:ss')
  const date_time = new Date()

  // For debug POSTed params.
  if (props.getProperty("debug") && props.getProperty("debug") === "1") {
    const log = sheet.getSheetByName("log")
    // Save log
    log.appendRow([date_time, json])
  }

  const env = sheet.getSheetByName("env")
  // {"id":"34:ab:95:5a:9b:d0","bat":0.42599999904632568,"t_ad":22.625,"t_sh":0,"h_sh":0,"cnt":1,"fin":true}
  const data = payload.data
  // DATE_TIME	BOOT_COUNT	ADT7410_TEMP	SHTC30_TEMP	SHTC30_HUMI	DEV_ID BATTERY_VOLT
  env.appendRow([date_time, data.cnt, data.t_ad, data.t_sh, data.h_sh, data.id, data.bat, data.up, data.fin])

  // const output = ContentService.createTextOutput()
  // output.setMimeType(ContentService.MimeType.JSON)
  // output.setContent(JSON.stringify({status: "OK"}))

  // return output
}
