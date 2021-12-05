# ESP32-examples

ESP32とその派生ESP32-C3とESP32-S3に習熟するためのサンプルコードたちと、俺々ライブラリー。

## 環境

ESP-IDFを使用。

Arduinoを使用している方が多いようですが、もう少し底辺からプログラムを書いてみたくてESP32のIDFを使用しています。

## 使用方法

ESP-IDFのダウントードとインストールは終了しているとします。

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

idf.py build
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
