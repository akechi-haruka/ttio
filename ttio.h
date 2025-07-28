#pragma once
#include <stdbool.h>
#include <stdint.h>

struct config {
    unsigned int vk_scan;
    unsigned int vk_input[32];
    char cardid[17];
};

struct aime_config {
    bool enable;
    uint8_t port;
    bool high_baudrate;
    bool custom_led_flash;
    uint16_t read_delay;
    uint16_t read_timeout;
};

struct api_config {
    bool enable;
    uint8_t service_index;
    uint8_t test_index;
    uint8_t coin_index;
};

struct iodata {
    int buttons;
    short analog[8];
    short encoder[4];
    char unk2[32];
    short coin[2];
    short coin2[2];
    char unk3[4];
};

struct carddata {
    char id[16];
    char unk[28];
};

struct carddata_amic {
    char id[16];
    char accesscode[20];
    uint8_t is_amic;
};