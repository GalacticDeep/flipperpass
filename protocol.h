#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define FP_FRAME_SIZE    60
#define FP_NICK_SIZE     13
#define FP_STATUS_SIZE   31
#define FP_LOCATION_SIZE 25
#define FP_AVATARS       4

typedef struct {
    uint8_t id[8];
    uint8_t avatar;
    char nickname[FP_NICK_SIZE];
    char status[FP_STATUS_SIZE];
} FpProfile;

typedef struct {
    uint8_t bytes[FP_FRAME_SIZE];
    size_t used;
} FpParser;

uint16_t fp_crc(const uint8_t* data, size_t size);
void fp_encode(const FpProfile* profile, uint8_t out[FP_FRAME_SIZE]);
bool fp_decode(const uint8_t in[FP_FRAME_SIZE], FpProfile* profile);
bool fp_feed(FpParser* parser, uint8_t byte, FpProfile* profile);
bool fp_text_valid(const char* text, size_t capacity, bool allow_empty);
