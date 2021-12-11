# WiFi exsample

Connect the AP and disconnect. After 5 sec. reconnect the AP.

Set SSID and password from Smartconfig or ESP touch.

## Erase NVS

```
parttool.py --port /dev/ttyUSB0 erase_partition --partition-name=nvs
```
