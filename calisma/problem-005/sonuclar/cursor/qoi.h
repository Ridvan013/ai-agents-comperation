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

namespace qoi_detail {
// The helpers in utils.h transfer one byte at a time through std::cin/std::cout.
// Decoupling the C++ streams from C stdio makes that byte-wise I/O fast enough
// for large images without changing any observable behavior.
inline bool EnableFastIo() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    return true;
}
const bool kFastIoEnabled = EnableFastIo();
}  // namespace qoi_detail

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

    // Only 3 (RGB) and 4 (RGBA) channel images are valid.
    if (channels != 3u && channels != 4u) {
        return false;
    }

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
    const uint64_t px_num = static_cast<uint64_t>(width) * height;

    // Running index of the 64 most recently seen colors, all zeroed.
    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    // Current pixel; alpha defaults to opaque for RGB input.
    uint8_t r = 0u, g = 0u, b = 0u, a = 255u;
    // Previous pixel, initialized per the QOI specification.
    uint8_t pre_r = 0u, pre_g = 0u, pre_b = 0u, pre_a = 255u;

    for (uint64_t i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        g = QoiReadU8();
        b = QoiReadU8();
        if (channels == 4) a = QoiReadU8();

        if (r == pre_r && g == pre_g && b == pre_b && a == pre_a) {
            // QOI_OP_RUN: identical pixel, extend the current run.
            ++run;
            // Runs are stored biased by -1, so the max encodable length is 62.
            if (run == 62 || i + 1 == px_num) {
                QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
                run = 0;
            }
        } else {
            // A different pixel ends any pending run first.
            if (run > 0) {
                QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
                run = 0;
            }

            const int index_pos = QoiColorHash(r, g, b, a);
            if (history[index_pos][0] == r && history[index_pos][1] == g &&
                history[index_pos][2] == b && history[index_pos][3] == a) {
                // QOI_OP_INDEX: pixel already present in the history table.
                QoiWriteU8(QOI_OP_INDEX_TAG | static_cast<uint8_t>(index_pos));
            } else {
                history[index_pos][0] = r;
                history[index_pos][1] = g;
                history[index_pos][2] = b;
                history[index_pos][3] = a;

                if (a == pre_a) {
                    // Channel differences use 8-bit wraparound arithmetic.
                    const int8_t vr = static_cast<int8_t>(r - pre_r);
                    const int8_t vg = static_cast<int8_t>(g - pre_g);
                    const int8_t vb = static_cast<int8_t>(b - pre_b);
                    const int8_t vg_r = static_cast<int8_t>(vr - vg);
                    const int8_t vg_b = static_cast<int8_t>(vb - vg);

                    if (vr >= -2 && vr <= 1 && vg >= -2 && vg <= 1 &&
                        vb >= -2 && vb <= 1) {
                        // QOI_OP_DIFF: small per-channel difference.
                        QoiWriteU8(QOI_OP_DIFF_TAG |
                                   static_cast<uint8_t>((vr + 2) << 4) |
                                   static_cast<uint8_t>((vg + 2) << 2) |
                                   static_cast<uint8_t>(vb + 2));
                    } else if (vg >= -32 && vg <= 31 && vg_r >= -8 && vg_r <= 7 &&
                               vg_b >= -8 && vg_b <= 7) {
                        // QOI_OP_LUMA: difference relative to the green channel.
                        QoiWriteU8(QOI_OP_LUMA_TAG | static_cast<uint8_t>(vg + 32));
                        QoiWriteU8(static_cast<uint8_t>((vg_r + 8) << 4) |
                                   static_cast<uint8_t>(vg_b + 8));
                    } else {
                        // QOI_OP_RGB: full RGB triple, alpha unchanged.
                        QoiWriteU8(QOI_OP_RGB_TAG);
                        QoiWriteU8(r);
                        QoiWriteU8(g);
                        QoiWriteU8(b);
                    }
                } else {
                    // QOI_OP_RGBA: alpha changed, store all four channels.
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
    for (size_t i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
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

    // Only 3 (RGB) and 4 (RGBA) channel images are valid.
    if (channels != 3u && channels != 4u) {
        return false;
    }

    int run = 0;
    const uint64_t px_num = static_cast<uint64_t>(width) * height;

    // Running index of the 64 most recently seen colors, all zeroed.
    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    // Current pixel, initialized per the QOI specification.
    uint8_t r = 0u, g = 0u, b = 0u, a = 255u;

    for (uint64_t i = 0; i < px_num; ++i) {
        if (run > 0) {
            // Still inside a run: repeat the current pixel.
            --run;
        } else {
            const uint8_t tag = QoiReadU8();
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
                const uint8_t index_pos = tag & 0x3fu;
                r = history[index_pos][0];
                g = history[index_pos][1];
                b = history[index_pos][2];
                a = history[index_pos][3];
            } else if ((tag & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
                // Each channel carries a 2-bit value biased by -2; uint8_t wraps.
                r = static_cast<uint8_t>(r + ((tag >> 4) & 0x03) - 2);
                g = static_cast<uint8_t>(g + ((tag >> 2) & 0x03) - 2);
                b = static_cast<uint8_t>(b + (tag & 0x03) - 2);
            } else if ((tag & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
                const uint8_t tag2 = QoiReadU8();
                const int vg = (tag & 0x3f) - 32;              // green diff, bias -32
                const int vg_r = ((tag2 >> 4) & 0x0f) - 8;     // (red - green) diff
                const int vg_b = (tag2 & 0x0f) - 8;            // (blue - green) diff
                r = static_cast<uint8_t>(r + vg + vg_r);
                g = static_cast<uint8_t>(g + vg);
                b = static_cast<uint8_t>(b + vg + vg_b);
            } else {  // (tag & QOI_MASK_2) == QOI_OP_RUN_TAG
                run = tag & 0x3fu;  // remaining repeats after this pixel
            }

            const int index_pos = QoiColorHash(r, g, b, a);
            history[index_pos][0] = r;
            history[index_pos][1] = g;
            history[index_pos][2] = b;
            history[index_pos][3] = a;
        }

        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);
    }

    bool valid = true;
    for (size_t i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
