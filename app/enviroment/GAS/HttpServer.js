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

function getTimestamp() {
  const date = new Date()
  return Math.floor(date.getTime()/1000)
}

function refreshToken() {
  const url = "https://api.twitter.com/2/oauth2/token"

  const props = PropertiesService.getScriptProperties()

  if (props.getProperty("expire_at") && parseInt(props.getProperty("expire_at")) > getTimestamp()) {
    // access token is valid
    console.log("access token is valid")
    return
  }

  // private client requires BASIC auth header
  const options = {
    method : "post",
    followRedirects: false,
    contentType: "application/x-www-form-urlencoded",
    payload : {
      grant_type: "refresh_token",
      refresh_token: props.getProperty("refresh_token"),
      client_id: props.getProperty("client_id")
    }
  }
  // insert BASIC auth if stored client_secret
  if(props.getProperty("client_secret")) {
    const encoded_auth = Utilities.base64Encode(props.getProperty("client_id") + ":" + props.getProperty("client_secret"))
    options.headers = {
      Authorization : "Basic " + encoded_auth
    }
  }

  const response = UrlFetchApp.fetch(url, options);
  const json = JSON.parse(response);

  props.setProperties({
    "access_token": json.access_token,
    "refresh_token": json.refresh_token,
    "expire_at": (getTimestamp() + json.expires_in - 60).toString() // stored as String
  })
  // console.log(json)

  console.log("updated access token")
}

function updateStatus(message) {
  const url = "https://api.twitter.com/2/tweets"

  const api_params = {
    text: message
  }

  refreshToken()

  const props = PropertiesService.getScriptProperties()

  const options = {
    method : "post",
    followRedirects: false,
    headers: {
      Authorization : "Bearer " + props.getProperty("access_token")
    },
    contentType: "application/json",
    payload : JSON.stringify(api_params)
  }

  const response = UrlFetchApp.fetch(url, options);
  const json = JSON.parse(response);
  console.log(json)
}

function extractTodayRedords() {
  const sheet = SpreadsheetApp.openById(SpreadsheetApp.getActiveSpreadsheet().getId())
  const env = sheet.getSheetByName("env")

  const data = env.getDataRange().getValues()
  // extract today's records
  const today = new Date(Utilities.formatDate(new Date(), 'Asia/Tokyo', 'yyyy-MM-dd') + 'T00:00:00+09:00')
  const records = data.filter((row) => row[0] > today)
  // console.log(records)
  return records
}

function highTempDays() {
  const sheet = SpreadsheetApp.openById(SpreadsheetApp.getActiveSpreadsheet().getId())
  const env = sheet.getSheetByName("env")

  const data = env.getDataRange().getValues()
  // extract cool days
  const records = data.filter((row) => (row[2] < 25.0) || (row[3] < 25.0)).map((row) => row[0])

  const lastCoolDay = Math.max.apply(null, records)
  const now = new Date()
  return Math.floor((now - lastCoolDay) / (1000 * 3600 * 24))
}

function makeStatusLine(records) {
  const statuses = []
  // Make uniq
  const ids = new Set(records.map((row) => row[5]))
  ids.forEach((id) => {
    const rows = records.filter((row) => row[5] === id)
    const last_row = rows[rows.length - 1]
    statuses.push("" + last_row[1] + "回->" + last_row[6].toFixed(2) + "V")
  })

  return statuses.join()
}

function minTemp() {
  const records = extractTodayRedords()
  const rows = records.flatMap((row) => [row[2], row[3]])
  // const temp = Math.min(...rows)
  const temp = Math.min.apply(null, rows)
  // console.log(temp.toFixed(2))

  // get status

  const today = new Date()
  const date = Utilities.formatDate(today, 'Asia/Tokyo', 'M月d日')
  let  message = "今朝(" + date + ")の最低室温は、" + temp.toFixed(1) + "℃でした。"
  const high_temp_days = highTempDays()
  if (high_temp_days > 2) {
    // ignore 2 days over 25C
    message += "連続" + high_temp_days + "日の熱帯夜です。"
  }
  message += "\n" + makeStatusLine(records) + "\n#ESP32 #GAS"
  console.log(message)
  updateStatus(message)
}

function maxTemp() {
  const records = extractTodayRedords()
  const rows = records.flatMap((row) => [row[2], row[3]])
  // const temp = Math.max(...rows)
  const temp = Math.max.apply(null, rows)
  // console.log(temp.toFixed(2))

  const date = Utilities.formatDate(new Date(), 'Asia/Tokyo', 'M月d日')
  let message = "今日(" + date + ")の最高室温は、" + temp.toFixed(1) + "℃でした。\n"
  message += makeStatusLine(records) + "\n#ESP32 #GAS"
  console.log(message)
  updateStatus(message)
}
