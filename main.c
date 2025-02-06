#include <stdio.h>
#include <windows.h>
#include <stdbool.h>
#define EXPORT __declspec(dllexport)
#define IsKeyDown(k) (GetAsyncKeyState(k) & 0x8000)
#define KEY_NAME "ttio"
#define CONFIG_NAME ".\\ttio.ini"
#define UNUSED __attribute__ ((unused))

struct config {
    unsigned int vk_scan;
    unsigned int vk_input[32];
    char cardid[32];
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

static boolean reading = false;
static boolean scanned = false;
static struct config cfg;

int hex2bin(char *source_str, char *dest_buffer) {
    char *line = source_str;
    char *data = line;
    int offset;
    int read_byte;
    int data_len = 0;

    while (sscanf(data, " %02x%n", &read_byte, &offset) == 1) {
        dest_buffer[data_len++] = read_byte;
        data += offset;
    }
    return data_len;
}

boolean APIENTRY DllMain(UNUSED HMODULE hinstDLL, DWORD fdwReason, UNUSED LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        OutputDebugString("TTIO 0.1, (c) 2024 Haruka\n");

        cfg.vk_scan = GetPrivateProfileIntA(KEY_NAME, "scan", VK_RETURN, CONFIG_NAME);
        GetPrivateProfileStringA(KEY_NAME, "card_id", "00000000000000000000000000000000", cfg.cardid, 32, CONFIG_NAME);
        for (int i = 0; i < 32; i++) {
            char key[16];
            sprintf(key, "key%d", i);
            cfg.vk_input[i] = GetPrivateProfileIntA(KEY_NAME, key, 0, CONFIG_NAME);
        }

        OutputDebugString("TTIO is loaded\ncard id=");
        OutputDebugString(cfg.cardid);
        OutputDebugString("\n");
    }

    return TRUE;
}

EXPORT int NESiCAReaderCancelRead() {
    OutputDebugString("NESiCAReaderCancelRead\n");
    reading = false;
    return 1;
}

EXPORT int NESiCAReaderGetID(struct carddata *data) {
    OutputDebugString("NESiCAReaderGetID\n");
    ZeroMemory(data, sizeof(struct carddata));
    if (scanned) {
        hex2bin(cfg.cardid, data->id);
        return 1;
    }
    return 0;
}

EXPORT int NESiCAReaderGetResult() {
    OutputDebugString("NESiCAReaderGetResult\n");
    // OK = 0, error = 2, no reading = 3
    if (scanned) return 0;
    if (!reading) return 3;
    return 2;
}

EXPORT int NESiCAReaderGetStatus() {
    // is reading?
    return reading ? 1 : 0;
}

EXPORT int NESiCAReaderIsError() {
    return 0;
}

EXPORT int NESiCAReaderRead() {
    OutputDebugString("NESiCAReaderRead\n");
    // start read
    reading = true;
    scanned = false;
    return 1;
}

EXPORT int ttioGetDeviceVersion() {
    return 1;
}

EXPORT int ttioClose() {
    OutputDebugString("ttioClose\n");
    return 1;
}

EXPORT int ttioGetStatus() {
    return 0x200; // ???
}

EXPORT int ttioOpen() {
    OutputDebugString("ttioOpen\n");
    return 1;
}

EXPORT int ttioUpdate(struct iodata *data) {
    ZeroMemory(data, sizeof(struct iodata));
    if (reading && IsKeyDown(cfg.vk_scan)) {
        scanned = true;
        reading = false;
    }
    for (int i = 0; i < 32; i++) {
        if (IsKeyDown(cfg.vk_input[i])) {
            data->buttons |= 1 << i;
        }
    }

    return 1;
}
