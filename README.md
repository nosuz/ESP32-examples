# ESP32-examples

ESP32 とその派生 ESP32-C3 と ESP32-S3 に習熟するためのサンプルコードたちと、俺々ライブラリー。

**実際には動作しないコードが含まれている可能性があります。また、API が変更になることも頻繁にありえます。**

## 環境

ESP-IDF を使用。

Arduino を使用している方が多いようですが、もう少し底辺からプログラムを書いてみたくて ESP32 の IDF を使用しています。

## サンプルリスト

| タイトル                           | 説明                                                                                               | path                                   |
| ---------------------------------- | -------------------------------------------------------------------------------------------------- | -------------------------------------- |
| L チカ                             | GPIO に接続された LED を点滅させる。最も簡単な GPIO 操作のサンプル。                               | [gpio/blink](gpio/blink)               |
| I2C 温度センサーと液晶ディスプレイ | I2C バスに接続された温度センサー ADT741 を読み取り、液晶ディスプレイ AQM0802A に表示するサンプル。 | [i2c/i2c_read_temp](i2c/i2c_read_temp) |
| ロードセルセンサー HX711           | ロードセルの値を HX711 で読み込むサンプル。                                                        | [gpio/hx711](gpio/hx711)               |
| WiFi                               | AP に接続するサンプル。接続と切断を 5 秒毎に繰り返す。                                             | [net/wifi](net/wifi)                   |

## 使用方法

ESP-IDF のダウントードとインストールは終了しているとします。

### ダウンロード

```
wget https://github.com/nosuz/ESP32-examples/archive/refs/heads/main.zip
unzip $DOWNLOAD_FOLDER/main.zip

# or

git clone https://github.com/nosuz/ESP32-examples.git
```

### コンパイル

```
. $HOME/esp/esp-idf/export.sh

# それぞれのサンプルがあるディレクトリーに移動する。
cd EXSAMPLE_DIR

idf.py set-target esp32 # or esp32c3 esp32s3

idf.py menuconfig # 必要ならば

idf.py build # 初回は色々コンパイルするので時間がかかる。
```

### 書き込み

```
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM0 monitor # コンソールログ等を確認する場合
```

## trouble shooting

### 次のようなエラーが表示されてコンパイルが止まる。解消方法は？

```
[610/972] Generating x509_crt_bundle
<SNIP>
gen_crt_bundle.py: Invalid certificate in ~/esp/esp-idf/components/
mbedtls/esp_crt_bundle/cacrt_all.pem
Invalid certificate
ninja: build stopped: subcommand failed.
ninja failed with exit code 1
```

`sdkconfig`を編集して次のようにします。

```
# CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_CMN=y
```

参照[FAILED: esp-idf/mbedtls/x509_crt_bundle (Invalid certificate) (IDFGH-3345)](https://github.com/espressif/esp-idf/issues/5322)

### これまで動いていたサンプルが、再コンパイルしたら動かなくなった。解決方法は？

`esp-idf`のモジュールをアップデートしてみる。

```
git submodule update --init
```
