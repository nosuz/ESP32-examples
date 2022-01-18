void init_mqtt(void);
int mqtt_publish(char *topic, const char *data, int qos, int retain);
void mqtt_stop(void);
