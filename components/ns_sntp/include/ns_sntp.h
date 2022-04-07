#ifndef NS_SNTP_H
#define NS_SNTP_H

void set_local_timezone(void);
void init_sntp(uint16_t sntp_interval);
void sync_sntp(void);
void start_sntp(void);
void stop_sntp(void);
#endif