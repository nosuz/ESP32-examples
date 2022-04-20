typedef struct
{
    rmt_item32_t *items;
    size_t length;
} RMT_ITEMS;

RMT_ITEMS *decode_ir_pulses(const char *params);
void delete_rmt_items(RMT_ITEMS *rmt_items);
