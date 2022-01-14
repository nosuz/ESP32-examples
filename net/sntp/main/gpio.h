void config_gpio(void);
int read_gpio(void);
extern volatile int intr_count;
extern volatile int pin_status;
void start_blink(void);
void stop_blink(void);
