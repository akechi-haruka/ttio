#include <stdio.h>
#include <windows.h>
#include <stdbool.h>
#include "ttio.h"
#include "subprojects/aimelib/aime.h"
#include "subprojects/aimelib/util/dprintf.h"
#include "subprojects/segapi/api/api.h"

#define EXPORT __declspec(dllexport)
#define IsKeyDown(k) (GetAsyncKeyState(k) & 0x8000)
#define TTIO_KEY_NAME "ttio"
#define AIME_KEY_NAME "aime"
#define API_KEY_NAME "api"
#define CONFIG_NAME ".\\ttio.ini"
#define UNUSED __attribute__ ((unused))
#define MIN_API_VER 0x010101

static boolean game_is_reading = false;
static boolean scanned = false;
static uint16_t coin_counter = 0;

static struct config cfg;
static struct aime_config aime_cfg;
static struct api_config api_cfg;

static HRESULT aime_status = S_FALSE;
static uint8_t* api_card = NULL;
static bool api_is_polling_for_card = false;

boolean APIENTRY DllMain(UNUSED HMODULE hinstDLL, DWORD fdwReason, UNUSED LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        dprintf("TTIO 0.4, (c) 2024-2025 Haruka\n");

        cfg.vk_scan = GetPrivateProfileIntA(TTIO_KEY_NAME, "scan", VK_RETURN, CONFIG_NAME);
        GetPrivateProfileStringA(TTIO_KEY_NAME, "card_id", "0000000000000000", cfg.cardid, 17, CONFIG_NAME);
        for (int i = 0; i < 32; i++) {
            char key[16];
            sprintf(key, "key%d", i);
            cfg.vk_input[i] = GetPrivateProfileIntA(TTIO_KEY_NAME, key, 0, CONFIG_NAME);
        }

        aime_cfg.enable = GetPrivateProfileIntA(AIME_KEY_NAME, "enable", 0, CONFIG_NAME);
        if (aime_cfg.enable) {
            aime_cfg.port = GetPrivateProfileIntA(AIME_KEY_NAME, "port", 4, CONFIG_NAME);
            aime_cfg.high_baudrate = GetPrivateProfileIntA(AIME_KEY_NAME, "high_baudrate", 1, CONFIG_NAME);
            aime_cfg.custom_led_flash = GetPrivateProfileIntA(AIME_KEY_NAME, "custom_led_flash", 0, CONFIG_NAME);
            aime_cfg.read_delay = GetPrivateProfileIntA(AIME_KEY_NAME, "read_delay", 1000, CONFIG_NAME);
            aime_cfg.read_timeout = GetPrivateProfileIntA(AIME_KEY_NAME, "read_timeout", 0, CONFIG_NAME);
            aime_cfg.led_index_r = GetPrivateProfileIntA(AIME_KEY_NAME, "led_index_r", 9, CONFIG_NAME);
            aime_cfg.led_index_g = GetPrivateProfileIntA(AIME_KEY_NAME, "led_index_g", 10, CONFIG_NAME);
            aime_cfg.led_index_b = GetPrivateProfileIntA(AIME_KEY_NAME, "led_index_b", 11, CONFIG_NAME);
        }

        api_cfg.enable = GetPrivateProfileIntA(API_KEY_NAME, "enable", 0, CONFIG_NAME);
        if (api_cfg.enable) {
            api_cfg.service_index = GetPrivateProfileIntA(API_KEY_NAME, "serviceButtonIndex", 13, CONFIG_NAME);
            api_cfg.test_index = GetPrivateProfileIntA(API_KEY_NAME, "testButtonIndex", 12, CONFIG_NAME);
            api_cfg.coin_index = GetPrivateProfileIntA(API_KEY_NAME, "coinButtonIndex", 15, CONFIG_NAME);

            if (api_get_version() < MIN_API_VER) {
                dprintf("ttio: segapi dll is outdated! At least v.%x is required, DLL is v.%x", MIN_API_VER,
                        api_get_version());
                return FALSE;
            }

            HRESULT hr = api_init(L".\\ttio.ini");
            if (!SUCCEEDED(hr)) {
                dprintf("ttio: failed to initialize API\n");
                return FALSE;
            }

            dprintf("ttio: Loaded\n");

            api_send(PACKET_20_PING, 0, NULL);
            api_block_card_reader(false);
        }

        dprintf("ttio: TTIO is loaded\ncard id=%s\n", cfg.cardid);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        if (aime_cfg.enable) {
            aime_close();
        }
    }

    return TRUE;
}

EXPORT int NESiCAReaderCancelRead(void) {
    dprintf("NESiCAReaderCancelRead\n");
    game_is_reading = false;

    if (aime_cfg.enable) {
        HRESULT hr;

        hr = aime_led_set(0, 0, 0);
        if (!SUCCEEDED(hr)) {
            return 0;
        }
        hr = aime_set_polling(false);
        if (!SUCCEEDED(hr)) {
            return 0;
        }
    }
    api_is_polling_for_card = false;
    api_block_card_reader(false);

    return 1;
}

void tohex(const unsigned char* in, const size_t insz, char* out, const size_t outsz) {
    const unsigned char* pin = in;
    const char* hex = "0123456789ABCDEF";
    char* pout = out;
    for (; pin < in + insz; pout += 2, pin++) {
        pout[0] = hex[*pin >> 4 & 0xF];
        pout[1] = hex[*pin & 0xF];
        if ((size_t)(pout + 2 - out) > outsz) {
            /* Better to truncate output string than overflow buffer */
            /* it would be still better to either return a status */
            /* or ensure the target buffer is large enough and it never happen */
            break;
        }
    }
}

EXPORT int NESiCAReaderGetID(struct carddata_v2* data) {
    dprintf("NESiCAReaderGetID\n");
    api_block_card_reader(false);
    ZeroMemory(data, sizeof(struct carddata_v2));
    if (scanned) {
        if (api_cfg.enable && api_card != NULL) {
            tohex(api_card, 8, data->id, 16);
            return 1;
        }
        if (aime_cfg.enable) {
            if (aime_get_card_type() == CARD_TYPE_FELICA) {
                tohex((const unsigned char *) aime_get_card_id(), aime_get_card_len(), data->id, 16);
                return 1;
            }
        } else {
            memcpy(data->id, cfg.cardid, 16);
            return 1;
        }
    }
    return 0;
}

EXPORT int NESiCAReaderGetResult(void) {
    //dprintf("NESiCAReaderGetResult\n");
    // OK = 0, error = 2, no reading = 3
    if (scanned) return 0;
    if (!game_is_reading || (aime_cfg.enable && !aime_is_polling())) return 3;

    if (api_card != NULL) {
        return 0;
    }
    if (aime_cfg.enable) {
        switch (aime_get_card_type()) {
            case CARD_TYPE_NONE: return 3;
            case CARD_TYPE_MIFARE:
            case CARD_TYPE_FELICA: return 0;
            default: return 2;
        }
    }

    return 2;
}

EXPORT int NESiCAReaderGetStatus(void) {
    // is reading?
    if (aime_cfg.enable) {
        if (!aime_is_polling()) {
            return 0;
        }
    }
    return game_is_reading ? 1 : 0;
}

EXPORT int NESiCAReaderIsError(void) {
    if (aime_cfg.enable) {
        return !SUCCEEDED(aime_status);
    }
    return 0;
}

EXPORT int NESiCAReaderRead(void) {
    dprintf("NESiCAReaderRead\n");
    // start read
    api_card = NULL;
    api_is_polling_for_card = false;
    game_is_reading = true;
    scanned = false;

    api_block_card_reader(true);

    if (aime_cfg.enable) {
        aime_set_polling(false); // force reader restart
        aime_set_polling(true);
    }

    return 1;
}

EXPORT int ttioGetDeviceVersion(void) {
    return 1;
}

EXPORT int ttioClose(void) {
    dprintf("ttioClose\n");

    if (aime_cfg.enable) {
        aime_close();
    }

    return 1;
}

EXPORT int ttioGetStatus(void) {
    return 0x200; // ???
}

EXPORT int ttioOpen(void) {
    dprintf("ttioOpen\n");

    if (aime_cfg.enable) {
        aime_set_poll_delay(aime_cfg.read_delay);
        aime_set_timeout(aime_cfg.read_timeout);
        aime_status = aime_connect(aime_cfg.port, aime_cfg.high_baudrate ? 115200 : 38400, aime_cfg.custom_led_flash);
        if (SUCCEEDED(aime_status)) {
            aime_debug_print_versions();
        }
    }

    return 1;
}

EXPORT int NESiCAReaderUpdate(void) {
    if ((game_is_reading && IsKeyDown(cfg.vk_scan)) || (aime_cfg.enable && aime_get_card_type() != CARD_TYPE_NONE) || (
        api_cfg.enable && api_card != NULL)) {
        scanned = true;
        game_is_reading = false;
    }

    if (api_cfg.enable && aime_cfg.enable) {
        if (!game_is_reading) {
            uint8_t* rgb = api_get_aime_rgb_and_clear();
            if (rgb != NULL) {
                aime_led_set(rgb[0], rgb[1], rgb[2]);
            }

            if (api_get_card_switch_state()) {
                api_is_polling_for_card = api_get_card_reading_state_and_clear_switch_state();
                dprintf("ttio: set polling by API (%d)\n", api_is_polling_for_card);
                aime_set_polling(api_is_polling_for_card);
            } else if (api_is_polling_for_card) {
                uint8_t card = aime_get_card_type();
                if (card != CARD_TYPE_NONE) {
                    dprintf("ttio: read card of type %d\n", card);
                    if (card == CARD_TYPE_FELICA) {
                        api_send(PACKET_25_CARD_FELICA, aime_get_card_len(), (const uint8_t*)aime_get_card_id());
                    } else if (card == CARD_TYPE_MIFARE) {
                        api_send(PACKET_26_CARD_AIME, aime_get_card_len(), (const uint8_t*)aime_get_card_id());
                    } else {
                        struct api_error_t error;
                        api_send(PACKET_36_ERROR, sizeof(error), (const uint8_t*)&error);
                    }
                    aime_set_polling(false);
                }
            }
        }
    }
    return 0;
}

EXPORT int ttioUpdate(struct iodata* data) {

    for (int i = 0; i < 32; i++) {
        if (IsKeyDown(cfg.vk_input[i])) {
            data->buttons |= 1 << i;
        }
    }

    if (api_get_and_clear_service()) {
        data->buttons |= 1 << api_cfg.service_index;
    }
    if (api_get_and_clear_test()) {
        data->buttons |= 1 << api_cfg.test_index;
    }
    const int c = api_get_and_clear_credits();
    if (c > 0) {
        data->buttons |= 1 << api_cfg.coin_index;
        data->coin[0] += (short)c;
    }

    if (aime_cfg.enable && !aime_cfg.custom_led_flash) {
        aime_led_set(aime_cfg.led_index_r, aime_cfg.led_index_g, aime_cfg.led_index_b);
    }

    NESiCAReaderUpdate();

    return 1;
}

EXPORT int NESiCAReaderGetIDAndAmic(struct carddata_v1_amic* data) {
    ZeroMemory(data, sizeof(struct carddata_v1_amic));
    if (scanned) {
        if (api_cfg.enable && api_card != NULL) {
            tohex(api_card, 8, data->id, 16);
            return 1;
        }
        if (aime_cfg.enable) {
            if (aime_get_card_type() == CARD_TYPE_FELICA) {
                tohex((const unsigned char *) aime_get_card_id(), aime_get_card_len(), data->id, 16);
                return 1;
            }
            if (aime_get_card_type() == CARD_TYPE_MIFARE) {
                tohex((const unsigned char *) aime_get_card_id(), aime_get_card_len(), data->accesscode, 20);
                data->is_amic = true;
                return 1;
            }
        } else {
            memcpy(data->id, cfg.cardid, 16);
            return 1;
        }
    }
    return 0;
}

EXPORT int NESiCAReaderGetXioStatus(void) {
    if (aime_cfg.enable) {
        return !SUCCEEDED(aime_status) ? -3 : 1;
    }
    return 1;
}