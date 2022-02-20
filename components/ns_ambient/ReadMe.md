```
POST http://ambidata.io/api/v2/channels/{channelId}/dataarray

Content-Type: application/json
{"writeKey": writeKey, "data": [{"d1": value, "d2": value}]}
```

```bash
CHANNEL_ID=000000
WRITE_KEY=abcd1234
curl -X POST -H 'Content-Type: application/json' -d '{"writeKey": "'${WRITE_KEY}'", "data": [{"d1": 12.34}]}' http://ambidata.io/api/v2/channels/${CHANNEL_ID}/dataarray
```
