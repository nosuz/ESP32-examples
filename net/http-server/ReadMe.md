## REST API

| endpoint |              |
| -------- | ------------ |
| /led_on  | Trun ON LED  |
| /led_off | Turn OFF LED |

test sample

```{bash}
$ avahi-browse -t -r _http._tcp
+ wlp3s0 IPv4 HTTP Server                                   Web Site             local
= wlp3s0 IPv4 HTTP Server                                   Web Site             local
   hostname = [esp32.local]
   address = [10.0.0.10]
   port = [80]
   txt = []

$ curl -v 'http://esp32.local/led_on'
$ curl -v 'http://esp32.local/led_off'
```

## SPIFFS Filesystem

- Specify flash memory size and the partition table file through `idf.py menuconfig`.
  - Flash memory size: Serial flasher config -> Flash size
  - Partition table: Partition Table -> Partition Table and Custom partition CSV file
- Create a filesystem through main/CMakeLists.txt.

## Error

`dangerous relocation: call8:`

`idf.py fullclean` followed by `idf.py build` solved the errors. I am not sure what's going on.
