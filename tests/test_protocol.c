#include "protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static FpProfile sample(void) {
    FpProfile p = {.id = {1, 2, 3, 4, 5, 6, 7, 8}, .avatar = 3};
    strcpy(p.nickname, "ABCDEFGHIJKL");
    strcpy(p.status, "123456789012345678901234567890");
    return p;
}
static void fix_crc(uint8_t* frame) {
    uint16_t crc = fp_crc(frame, 58);
    frame[58] = crc & 255;
    frame[59] = crc >> 8;
}
static size_t feed(FpParser* parser, const uint8_t* bytes, size_t size, FpProfile* out) {
    size_t received = 0;
    for(size_t i = 0; i < size; i++)
        received += fp_feed(parser, bytes[i], out);
    return received;
}
int main(void) {
    assert(fp_crc((const uint8_t*)"123456789", 9) == 0x29b1);
    FpProfile p = sample(), out;
    uint8_t frame[FP_FRAME_SIZE], broken[FP_FRAME_SIZE];
    fp_encode(&p, frame);
    assert(fp_decode(frame, &out));
    assert(!memcmp(&p, &out, sizeof(p)));
    for(size_t i = 0; i < FP_FRAME_SIZE; i++) {
        for(unsigned bit = 0; bit < 8; bit++) {
            memcpy(broken, frame, sizeof(frame));
            broken[i] ^= 1 << bit;
            assert(!fp_decode(broken, &out));
        }
    }
    // Semantic rejection even with a recomputed valid CRC.
    memcpy(broken, frame, sizeof(frame));
    broken[4] = 2;
    fix_crc(broken);
    assert(!fp_decode(broken, &out));
    memcpy(broken, frame, sizeof(frame));
    broken[13] = FP_AVATARS;
    fix_crc(broken);
    assert(!fp_decode(broken, &out));
    memcpy(broken, frame, sizeof(frame));
    memset(broken + 14, 'X', FP_NICK_SIZE);
    fix_crc(broken);
    assert(!fp_decode(broken, &out));
    memcpy(broken, frame, sizeof(frame));
    memset(broken + 27, 'X', FP_STATUS_SIZE);
    fix_crc(broken);
    assert(!fp_decode(broken, &out));
    memcpy(broken, frame, sizeof(frame));
    broken[14] = 0;
    fix_crc(broken);
    assert(!fp_decode(broken, &out));
    memcpy(broken, frame, sizeof(frame));
    broken[27] = 27;
    fix_crc(broken);
    assert(!fp_decode(broken, &out));
    // Byte stream boundaries, truncation and noise must not prevent recovery.
    for(size_t split = 0; split <= FP_FRAME_SIZE; split++) {
        FpParser parser = {0};
        size_t n = feed(&parser, frame, split, &out);
        n += feed(&parser, frame + split, FP_FRAME_SIZE - split, &out);
        assert(n == 1 && !memcmp(&p, &out, sizeof(p)));
        assert(feed(&parser, frame, sizeof(frame), &out) == 1);
    }
    for(size_t lost = 0; lost < FP_FRAME_SIZE; lost++) {
        FpParser parser = {0};
        assert(feed(&parser, frame, lost, &out) == 0);
        assert(feed(&parser, frame, sizeof(frame), &out) == 1);
    }
    FpParser parser = {0};
    uint32_t seed = 123;
    for(unsigned i = 0; i < 1000000; i++) {
        seed = seed * 1664525u + 1013904223u;
        fp_feed(&parser, seed >> 24, &out);
        assert(parser.used < FP_FRAME_SIZE);
    }
    assert(feed(&parser, frame, sizeof(frame), &out) == 1);
    p.status[0] = 0;
    fp_encode(&p, frame);
    assert(fp_decode(frame, &out) && out.status[0] == 0);
    assert(fp_text_valid("DEF CON", 25, false));
    assert(!fp_text_valid("", 25, false));
    assert(!fp_text_valid("bad\nlabel", 25, false));
    puts(
        "Protocol tests passed (CRC, bounds, malformed frames, stream recovery, 1M noise bytes).");
    return 0;
}
