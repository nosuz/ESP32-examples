## Refresh token

```bash
curl -X POST 'https://api.twitter.com/2/oauth2/token' \
--basic -u "$CLIENT_ID:$CLIENT_SECRET" \
--header 'Content-Type: application/x-www-form-urlencoded' \
--data-urlencode "refresh_token=$REFRESH_TOKEN" \
--data-urlencode 'grant_type=refresh_token' \
--data-urlencode "client_id=$CLIENT_ID"
```

```json
{
  "token_type": "bearer",
  "expires_in": 7200,
  "access_token": "XXXXXXXXXXXXXXXXXXXXXXXXXXXX",
  "scope": "tweet.write users.read tweet.read offline.access",
  "refresh_token": "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
}
```

## Update status

```bash
curl -X POST https://api.twitter.com/2/tweets \
-H "Authorization: Bearer $ACCESS_TOKEN" \
-H "Content-type: application/json" \
-d '{"text": "Hello World!"}'
```

## Reference

- [Twitter OAuth2.0 の設定や動作まとめ](https://zenn.dev/kg0r0/articles/8b1cfe654a1cee#developer-portal)
- Twitter Developer Platform
  - [Manage Tweets](https://developer.twitter.com/en/docs/twitter-api/tweets/manage-tweets/api-reference/post-tweets)
  - [Authorization](https://developer.twitter.com/en/docs/authentication/oauth-2-0/authorization-code)
