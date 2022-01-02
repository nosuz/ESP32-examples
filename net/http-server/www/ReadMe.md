## Web app

### install packages

```{bash}
$ npm init vite@latest www -- --template vue
$ cd www
$ npm install
```

bootstrap and axios are also installed by `npm install` command.

### development the application

```
$ npm run dev
```

In development mode, API access is redirected to ESP32 server.

app.py is API mock.

### make dist files

```
$ npm run build
```

Files in dist folder are copied into SPIFFS.

### LED control Web application

[LED control](http://eap32.local/)
