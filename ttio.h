#pragma once
#include <stdbool.h>
#include <stdint.h>

#define COIN_SLOT_COUNT 2

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
    char unk[4];
    char output[24];
    char unk2[4];
    uint16_t coin[COIN_SLOT_COUNT];
    short coin_consume[COIN_SLOT_COUNT];
    char unk3[4];
};

struct carddata_v1 {
    char id[16];
    char accesscode[20];
};

struct carddata_v1_amic {
    char id[16];
    char accesscode[20];
    uint8_t is_amic;
};

struct carddata_v2 {
    char id[16];
    char nesica_uid[7];
    char accesscode[20];
    uint8_t is_amic;
};