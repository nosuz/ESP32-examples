idf_component_register(
        SRCS "i2c_read_sensor_main.c"
        INCLUDE_DIRS "."
        REQUIRES adt7410 aqm0802a
)
