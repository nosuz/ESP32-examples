# WPS

ESP32 を起動(WPS mode)にする前に AP(アクセスポイント)の WPS ボタンを押しておく。

先に ESP32 が WPS モードに入ると、一度タイムアウトを待つ必要があるので接続に２分近くかかる。

## Erase NVS

```
# erase NVS partition
parttool.py -p /dev/ttyUSB0 erase_partition --partition-name=nvs

# erase all contents
idf.py -p /dev/ttyUSB0 erase-flash
```
