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
