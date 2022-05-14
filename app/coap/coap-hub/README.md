Test

```{bash}
$ date '+{"time": %N}' | coap-client -m post coap://coap.local/env -f -

$ echo -n '{"id" : "34:ab:95:5a:9b:d0", "bat" : 0.42599999904632568, "t_ad" : 23.25, "t_sh" : 0, "h_sh" : 0, "cnt" : 1, "fin" : true}' | coap-client -m post coap://coap.local/env -f -
```
