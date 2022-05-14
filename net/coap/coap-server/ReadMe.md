For testing this client, I used `coap-server` on Ubuntu.

The `coap-server` has limited end points. Refer `coap-client` manual. One of it is `/time`. This returns current date and time string.

```{bash}
$ sudo apt install libcoap2-bin
$ coap-server
```

```{bash}
$ coap-client -m get  coap://esp32.local/test
Hello World!
$ echo -n 1000|coap-client -m put coap://esp32.local/test -f -
$ coap-client -m get  coap://esp32.local/test
1000

```
