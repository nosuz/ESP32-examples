# WiFi exsample

If NVS has no SSID or the triger button is pressed on boot, ESP32 will be access point and start HTTP server to select a access point.

Set AP by the next command.

```
curl -X POST --data-urlencode 'ssid=AP_NAME' --data-urlencode 'passwd=AP_PASSWORD' http://esp32.local/ap
```

## Prtition table

```
idf.py menuconfig
```

Set to use custom partition table in `Partition Table` menu.

Set correct Flash size in `Serial flasher config` menu.

Copy partitions.csv into the project top directory.

## Erase NVS

```
parttool.py --port /dev/ttyUSB0 erase_partition --partition-name=nvs
```
