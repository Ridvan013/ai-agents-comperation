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

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {

    // qoi-header part

    // write magic bytes "qoif"
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    // write image width
    QoiWriteU32(width);
    // write image height
    QoiWriteU32(height);
    // write channel number
    QoiWriteU8(channels);
    // write color space specifier
    QoiWriteU8(colorspace);

    /* qoi-data part */
    int run = 0;
    long long px_num = 1ll * width * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r, g, b, a;
    a = 255u;
    uint8_t pre_r, pre_g, pre_b, pre_a;
    pre_r = 0u;
    pre_g = 0u;
    pre_b = 0u;
    pre_a = 255u;

    for (long long i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        g = QoiReadU8();
        b = QoiReadU8();
        if (channels == 4) a = QoiReadU8();

        if (r == pre_r && g == pre_g && b == pre_b && a == pre_a) {
            // identical pixel: extend the current run
            ++run;
            // a run stores at most 62 pixels (bias of -1); also flush the
            // pending run once the last pixel of the image is reached
            if (run == 62 || i == px_num - 1) {
                QoiWriteU8(QOI_OP_RUN_TAG | (run - 1));
                run = 0;
            }
        } else {
            // the pixel differs: first flush any pending run
            if (run > 0) {
                QoiWriteU8(QOI_OP_RUN_TAG | (run - 1));
                run = 0;
            }

            int hash = QoiColorHash(r, g, b, a);
            if (history[hash][0] == r && history[hash][1] == g &&
                history[hash][2] == b && history[hash][3] == a) {
                // the pixel is already in the running color table
                QoiWriteU8(QOI_OP_INDEX_TAG | hash);
            } else {
                history[hash][0] = r;
                history[hash][1] = g;
                history[hash][2] = b;
                history[hash][3] = a;

                if (a == pre_a) {
                    // signed, wrap-around channel differences
                    int8_t vr = static_cast<int8_t>(r - pre_r);
                    int8_t vg = static_cast<int8_t>(g - pre_g);
                    int8_t vb = static_cast<int8_t>(b - pre_b);
                    int8_t vg_r = static_cast<int8_t>(vr - vg);
                    int8_t vg_b = static_cast<int8_t>(vb - vg);

                    if (vr >= -2 && vr <= 1 && vg >= -2 && vg <= 1 &&
                        vb >= -2 && vb <= 1) {
                        QoiWriteU8(QOI_OP_DIFF_TAG | ((vr + 2) << 4) |
                                   ((vg + 2) << 2) | (vb + 2));
                    } else if (vg >= -32 && vg <= 31 && vg_r >= -8 &&
                               vg_r <= 7 && vg_b >= -8 && vg_b <= 7) {
                        QoiWriteU8(QOI_OP_LUMA_TAG | (vg + 32));
                        QoiWriteU8(((vg_r + 8) << 4) | (vg_b + 8));
                    } else {
                        QoiWriteU8(QOI_OP_RGB_TAG);
                        QoiWriteU8(r);
                        QoiWriteU8(g);
                        QoiWriteU8(b);
                    }
                } else {
                    QoiWriteU8(QOI_OP_RGBA_TAG);
                    QoiWriteU8(r);
                    QoiWriteU8(g);
                    QoiWriteU8(b);
                    QoiWriteU8(a);
                }
            }
        }

        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
    }

    // qoi-padding part
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {

    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    // read image width
    width = QoiReadU32();
    // read image height
    height = QoiReadU32();
    // read channel number
    channels = QoiReadU8();
    // read color space specifier
    colorspace = QoiReadU8();

    int run = 0;
    long long px_num = 1ll * width * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r, g, b, a;
    r = 0u;
    g = 0u;
    b = 0u;
    a = 255u;

    for (long long i = 0; i < px_num; ++i) {

        if (run > 0) {
            // still emitting pixels of the current run
            --run;
        } else {
            uint8_t tag = QoiReadU8();

            if (tag == QOI_OP_RGB_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
            } else if (tag == QOI_OP_RGBA_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
                a = QoiReadU8();
            } else if ((tag & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
                int hash = tag & 0x3f;
                r = history[hash][0];
                g = history[hash][1];
                b = history[hash][2];
                a = history[hash][3];
            } else if ((tag & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
                r += ((tag >> 4) & 0x03) - 2;
                g += ((tag >> 2) & 0x03) - 2;
                b += (tag & 0x03) - 2;
            } else if ((tag & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
                uint8_t extra = QoiReadU8();
                int8_t vg = static_cast<int8_t>((tag & 0x3f) - 32);
                r += vg + ((extra >> 4) & 0x0f) - 8;
                g += vg;
                b += vg + (extra & 0x0f) - 8;
            } else {  // (tag & QOI_MASK_2) == QOI_OP_RUN_TAG
                // repeat the previous pixel for the remaining run length
                run = tag & 0x3f;
            }

            int hash = QoiColorHash(r, g, b, a);
            history[hash][0] = r;
            history[hash][1] = g;
            history[hash][2] = b;
            history[hash][3] = a;
        }

        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);
    }

    bool valid = true;
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
