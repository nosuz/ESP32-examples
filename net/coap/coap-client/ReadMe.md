For testing this client, I used `coap-server` on Ubuntu.

The `coap-server` has limited end points. Refer `coap-client` manual. One of it is `/time`. This returns current date and time string.

```{bash}
$ sudo apt install libcoap2-bin
$ coap-server
```
