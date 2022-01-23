#include <stdio.h>

#include "twitter.h"
#include "esp_log.h"

static const char *TAG = "twitter-client";

void app_main(void)
{
    twitter_init_api_params();
    twitter_set_api_param("status", "Hello Ladies + Gentlemen, a signed OAuth request!");
    twitter_set_api_param("include_entities", "true");

    twitter_update_status();
}
