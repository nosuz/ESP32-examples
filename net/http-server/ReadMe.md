## REST API

| endpoint        |                                               |
| --------------- | --------------------------------------------- |
| /api/led_on     | Trun ON LED                                   |
| /api/led_off    | Turn OFF LED                                  |
| /api/led_status | Return the current LED status in JSON format. |

test sample

```{bash}
$ avahi-browse -t -r _http._tcp
+ wlp3s0 IPv4 HTTP Server                                   Web Site             local
= wlp3s0 IPv4 HTTP Server                                   Web Site             local
   hostname = [esp32.local]
   address = [10.0.0.10]
   port = [80]
   txt = []

$ curl -v 'http://esp32.local/api/led_on'
$ curl -v 'http://esp32.local/api/led_off'
```

## SPIFFS Filesystem

- Specify flash memory size and the partition table file through `idf.py menuconfig`.
  - Flash memory size: Serial flasher config -> Flash size
  - Partition table: Partition Table -> Partition Table and Custom partition CSV file
- Create a filesystem through main/CMakeLists.txt.

## Error 1

```
dangerous relocation: call8:
```

`idf.py fullclean` followed by `idf.py build` solved the errors. I am not sure what's going on.

## Error 2

This error was shown when a web application acceses an ESP32 http server through npm proxy.

```
W (9885) httpd_parse: parse_block: request URI/header too long
W (9885) httpd_txrx: httpd_resp_send_err: 431 Request Header Fields Too Large - Header fields are too long for server to interpret
```

Chnage ESP32 configuration by `idf.py menuconfig`. The configuration filed is in `Component config > HTTP Server > Max HTTP Request Header Length change 512 to 1024.`.

## Web App

Follow www/ReadMe.md

## Button

Long press makes WPS starts. When WPS starts the LED begin to blink.

Short press toggles LED status.
