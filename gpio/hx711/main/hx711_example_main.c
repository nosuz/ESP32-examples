#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_sleep.h"

#define GPIO_OUTPUT_IO_0 4
#define GPIO_OUTPUT_IO_1 5
#define GPIO_OUTPUT_IO_4 33

#define GPIO_HX711_CLK 26

#define GPIO_OUTPUT_PIN_SEL ((1ULL << GPIO_OUTPUT_IO_0) | (1ULL << GPIO_OUTPUT_IO_1) | (1ULL << GPIO_OUTPUT_IO_4) | (1ULL << GPIO_HX711_CLK))

#define GPIO_OUTPUT_IO_3 25

#define GPIO_HX711_DATA 27

#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_OUTPUT_IO_3) | (1ULL << GPIO_HX711_DATA))

void app_main()
{
  gpio_config_t out_conf = {
      //disable interrupt
      .intr_type = GPIO_PIN_INTR_DISABLE,
      //set as output mode
      .mode = GPIO_MODE_OUTPUT,
      //bit mask of the pins that you want to set
      .pin_bit_mask = GPIO_OUTPUT_PIN_SEL,
      //disable pull-down mode
      .pull_down_en = 0,
      //disable pull-up mode
      .pull_up_en = 0};
  //configure GPIO with the given settings
  gpio_config(&out_conf);

  gpio_config_t in_conf = {
      //disable interrupt
      .intr_type = GPIO_PIN_INTR_DISABLE,
      //set as input mode
      .mode = GPIO_MODE_INPUT,
      //bit mask of the pins that you want to set
      .pin_bit_mask = GPIO_INPUT_PIN_SEL,
      //disable pull-down mode
      .pull_down_en = 0,
      //disable pull-up mode
      .pull_up_en = 0};
  //configure GPIO with the given settings
  gpio_config(&in_conf);

  // Sleep HX711
  gpio_set_level(GPIO_HX711_CLK, 1);
  ets_delay_us(100);

  long zero = 0;
  for (int i = 0; i < 10; i++)
  {
    // start device
    gpio_set_level(GPIO_HX711_CLK, 0);
    // wait device ready, or 0
    while (gpio_get_level(GPIO_HX711_DATA))
      ;
    ets_delay_us(1);
    //vTaskDelay(20 / portTICK_RATE_MS);

    long value = 0;
    int tick;
    for (tick = 0; tick < 24; tick++)
    {
      gpio_set_level(GPIO_HX711_CLK, 1);
      ets_delay_us(1);

      value = (value << 1);
      if (gpio_get_level(GPIO_HX711_DATA))
        value++;

      gpio_set_level(GPIO_HX711_CLK, 0);
      ets_delay_us(1);
    }
    // make 32 bit
    value = value ^ 0x800000;

    gpio_set_level(GPIO_HX711_CLK, 1);
    ets_delay_us(1);
    gpio_set_level(GPIO_HX711_CLK, 0);
    ets_delay_us(1);
    gpio_set_level(GPIO_HX711_CLK, 1);
    ets_delay_us(1);

    zero = zero + value;
    ets_delay_us(1000);
    //vTaskDelay(1 / portTICK_RATE_MS);
  }
  zero = zero / 10;

  // Sleep HX711
  gpio_set_level(GPIO_HX711_CLK, 1);
  ets_delay_us(100);

  int cnt = 0;
  //while(cnt < 10) {
  while (1)
  {
    printf("cnt: %d\n", cnt++);
    vTaskDelay(1000 / portTICK_RATE_MS);
    gpio_set_level(GPIO_OUTPUT_IO_0, cnt % 2);
    gpio_set_level(GPIO_OUTPUT_IO_1, cnt % 2);

    long avr = 0;
    for (int i = 0; i < 10; i++)
    {
      // start device
      gpio_set_level(GPIO_HX711_CLK, 0);
      // wait device ready, or 0
      while (gpio_get_level(GPIO_HX711_DATA))
        ;
      ets_delay_us(1);
      //vTaskDelay(20 / portTICK_RATE_MS);

      long value = 0;
      int tick;
      for (tick = 0; tick < 24; tick++)
      {
        gpio_set_level(GPIO_HX711_CLK, 1);
        ets_delay_us(1);

        value = (value << 1);
        if (gpio_get_level(GPIO_HX711_DATA))
          value++;

        gpio_set_level(GPIO_HX711_CLK, 0);
        ets_delay_us(1);
      }
      // make 32 bit
      value = value ^ 0x800000;

      avr = avr + value;

      gpio_set_level(GPIO_HX711_CLK, 1);
      ets_delay_us(1);
      gpio_set_level(GPIO_HX711_CLK, 0);
      ets_delay_us(1);
      gpio_set_level(GPIO_HX711_CLK, 1);
      ets_delay_us(1);

      ets_delay_us(1000);
      //vTaskDelay(1 / portTICK_RATE_MS);
    }
    avr = avr / 10;

    // Sleep HX711
    gpio_set_level(GPIO_HX711_CLK, 1);
    ets_delay_us(100);

    long delta = avr - zero;
    printf("raw dec: %ld\n", avr);
    printf("raw hex: %lx\n", avr);
    printf("delta: %ld\n", delta);

    //float weight = (float) delta / 2383.25;
    float weight = (float)delta / 2274.41;
    printf("weight: %0.2fkg\n", weight);
  }
  esp_deep_sleep_start();
}
