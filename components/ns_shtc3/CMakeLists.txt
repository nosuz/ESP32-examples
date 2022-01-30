idf_component_register(
    SRCS "ns_shtc3.c"
    INCLUDE_DIRS "include"
    REQUIRES ns_i2c_master ns_crc8
)
