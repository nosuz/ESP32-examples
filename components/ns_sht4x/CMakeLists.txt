idf_component_register(
    SRCS "ns_sht4x.c"
    INCLUDE_DIRS "include"
    REQUIRES ns_i2c_master ns_crc8
)
