#include "protocol.h"
#include <string.h>

uint16_t fp_crc(const uint8_t* data, size_t size) {
    uint16_t crc = 0xffff;
    for(size_t i = 0; i < size; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for(unsigned b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

bool fp_text_valid(const char* text, size_t capacity, bool allow_empty) {
    if(!allow_empty && !text[0]) return false;
    for(size_t i = 0; i < capacity; i++) {
        if(!text[i]) return true;
        if((uint8_t)text[i] < 32 || (uint8_t)text[i] > 126) return false;
    }
    return false;
}

void fp_encode(const FpProfile* p, uint8_t out[FP_FRAME_SIZE]) {
    memset(out, 0, FP_FRAME_SIZE);
    memcpy(out, "FLPS", 4);
    out[4] = 1;
    memcpy(out + 5, p->id, 8);
    out[13] = p->avatar;
    memcpy(out + 14, p->nickname, FP_NICK_SIZE);
    memcpy(out + 27, p->status, FP_STATUS_SIZE);
    uint16_t crc = fp_crc(out, 58);
    out[58] = crc & 0xff;
    out[59] = crc >> 8;
}

bool fp_decode(const uint8_t in[FP_FRAME_SIZE], FpProfile* p) {
    if(memcmp(in, "FLPS", 4) || in[4] != 1 || in[13] >= FP_AVATARS ||
       fp_crc(in, 58) != ((uint16_t)in[58] | ((uint16_t)in[59] << 8)) ||
       !fp_text_valid((const char*)in + 14, FP_NICK_SIZE, false) ||
       !fp_text_valid((const char*)in + 27, FP_STATUS_SIZE, true))
        return false;
    memcpy(p->id, in + 5, 8);
    p->avatar = in[13];
    memcpy(p->nickname, in + 14, FP_NICK_SIZE);
    memcpy(p->status, in + 27, FP_STATUS_SIZE);
    return true;
}

bool fp_feed(FpParser* parser, uint8_t byte, FpProfile* p) {
    parser->bytes[parser->used++] = byte;
    if(parser->used < FP_FRAME_SIZE) return false;
    if(fp_decode(parser->bytes, p)) {
        parser->used = 0;
        return true;
    }
    memmove(parser->bytes, parser->bytes + 1, FP_FRAME_SIZE - 1);
    parser->used--;
    return false;
}
