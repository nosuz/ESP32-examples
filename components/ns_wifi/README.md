# WiFi exsample

If NVS has no SSID or the triger button is pressed on boot, ESP32 will be access point and start HTTP server to select a access point.

## Erase NVS

```
parttool.py --port /dev/ttyUSB0 erase_partition --partition-name=nvs
```
