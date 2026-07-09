#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0;
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);

bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {
    QoiWriteChar('q'); QoiWriteChar('o'); QoiWriteChar('i'); QoiWriteChar('f');
    QoiWriteU32(width); QoiWriteU32(height);
    QoiWriteU8(channels); QoiWriteU8(colorspace);

    uint8_t hist[64][4]; memset(hist, 0, sizeof(hist));
    uint8_t pr = 0, pg = 0, pb = 0, pa = 255;
    int run = 0;
    long px = (long)width * height;

    for (long i = 0; i < px; ++i) {
        uint8_t r = QoiReadU8(), g = QoiReadU8(), b = QoiReadU8(), a = pa;
        if (channels == 4) a = QoiReadU8();

        if (r == pr && g == pg && b == pb && a == pa) {
            run++;
            if (run == 62 || i == px - 1) { QoiWriteU8(QOI_OP_RUN_TAG | (run - 1)); run = 0; }
        } else {
            if (run > 0) { QoiWriteU8(QOI_OP_RUN_TAG | (run - 1)); run = 0; }
            int idx = QoiColorHash(r, g, b, a);
            if (hist[idx][0] == r && hist[idx][1] == g && hist[idx][2] == b && hist[idx][3] == a) {
                QoiWriteU8(QOI_OP_INDEX_TAG | idx);
            } else {
                hist[idx][0] = r; hist[idx][1] = g; hist[idx][2] = b; hist[idx][3] = a;
                if (a == pa) {
                    int8_t vr = (int8_t)(r - pr), vg = (int8_t)(g - pg), vb = (int8_t)(b - pb);
                    int8_t vgr = (int8_t)(vr - vg), vgb = (int8_t)(vb - vg);
                    if (vr >= -2 && vr <= 1 && vg >= -2 && vg <= 1 && vb >= -2 && vb <= 1) {
                        QoiWriteU8(QOI_OP_DIFF_TAG | ((vr + 2) << 4) | ((vg + 2) << 2) | (vb + 2));
                    } else if (vg >= -32 && vg <= 31 && vgr >= -8 && vgr <= 7 && vgb >= -8 && vgb <= 7) {
                        QoiWriteU8(QOI_OP_LUMA_TAG | (vg + 32));
                        QoiWriteU8(((vgr + 8) << 4) | (vgb + 8));
                    } else {
                        QoiWriteU8(QOI_OP_RGB_TAG); QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
                    }
                } else {
                    QoiWriteU8(QOI_OP_RGBA_TAG); QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b); QoiWriteU8(a);
                }
            }
        }
        pr = r; pg = g; pb = b; pa = a;
    }
    for (int i = 0; i < 8; ++i) QoiWriteU8(QOI_PADDING[i]);
    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {
    char m0 = QoiReadChar(), m1 = QoiReadChar(), m2 = QoiReadChar(), m3 = QoiReadChar();
    if (m0 != 'q' || m1 != 'o' || m2 != 'i' || m3 != 'f') return false;
    width = QoiReadU32(); height = QoiReadU32();
    channels = QoiReadU8(); colorspace = QoiReadU8();

    uint8_t hist[64][4]; memset(hist, 0, sizeof(hist));
    uint8_t r = 0, g = 0, b = 0, a = 255;
    long px = (long)width * height;
    int run = 0;

    for (long i = 0; i < px; ++i) {
        if (run > 0) {
            run--;
        } else {
            uint8_t tag = QoiReadU8();
            if (tag == QOI_OP_RGB_TAG) {
                r = QoiReadU8(); g = QoiReadU8(); b = QoiReadU8();
            } else if (tag == QOI_OP_RGBA_TAG) {
                r = QoiReadU8(); g = QoiReadU8(); b = QoiReadU8(); a = QoiReadU8();
            } else if ((tag & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
                int idx = tag & 0x3f;
                r = hist[idx][0]; g = hist[idx][1]; b = hist[idx][2]; a = hist[idx][3];
            } else if ((tag & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
                r += ((tag >> 4) & 3) - 2; g += ((tag >> 2) & 3) - 2; b += (tag & 3) - 2;
            } else if ((tag & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
                uint8_t t2 = QoiReadU8();
                int vg = (tag & 0x3f) - 32;
                r += vg + ((t2 >> 4) & 0xf) - 8;
                g += vg;
                b += vg + (t2 & 0xf) - 8;
            } else if ((tag & QOI_MASK_2) == QOI_OP_RUN_TAG) {
                run = tag & 0x3f;
            }
            int idx = QoiColorHash(r, g, b, a);
            hist[idx][0] = r; hist[idx][1] = g; hist[idx][2] = b; hist[idx][3] = a;
        }
        QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);
    }
    return true;
}

#endif  // QOI_FORMAT_CODEC_QOI_H_
