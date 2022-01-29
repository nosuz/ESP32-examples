void init_fake_heap();
char *fake_malloc(uint16_t size);
void fake_free(char *pt);
char *fake_realloc(char *pt, uint16_t size);
