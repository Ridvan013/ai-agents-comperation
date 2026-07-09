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
    if (width == 0 || height == 0) return false;
    if (channels != 3u && channels != 4u) return false;
    if (colorspace > 1u) return false;

    const uint64_t px_num = static_cast<uint64_t>(width) * height;
    const auto read_u8 = [](uint8_t &value) {
        char ch = 0;
        if (!std::cin.read(&ch, 1)) return false;
        value = static_cast<uint8_t>(static_cast<unsigned char>(ch));
        return true;
    };

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

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t px[4] = {0u, 0u, 0u, 255u};
    uint8_t prev[4] = {0u, 0u, 0u, 255u};

    for (uint64_t i = 0; i < px_num; ++i) {
        if (!read_u8(px[0]) || !read_u8(px[1]) || !read_u8(px[2])) return false;
        px[3] = 255u;
        if (channels == 4u && !read_u8(px[3])) return false;

        if (px[0] == prev[0] && px[1] == prev[1] && px[2] == prev[2] && px[3] == prev[3]) {
            ++run;
            if (run == 62 || i + 1 == px_num) {
                QoiWriteU8(static_cast<uint8_t>(QOI_OP_RUN_TAG | (run - 1)));
                run = 0;
            }
            continue;
        }

        if (run > 0) {
            QoiWriteU8(static_cast<uint8_t>(QOI_OP_RUN_TAG | (run - 1)));
            run = 0;
        }

        const int hash = QoiColorHash(px[0], px[1], px[2], px[3]);
        if (history[hash][0] == px[0] && history[hash][1] == px[1] &&
            history[hash][2] == px[2] && history[hash][3] == px[3]) {
            QoiWriteU8(static_cast<uint8_t>(QOI_OP_INDEX_TAG | hash));
        } else if (px[3] == prev[3]) {
            const int dr = static_cast<int>(px[0]) - static_cast<int>(prev[0]);
            const int dg = static_cast<int>(px[1]) - static_cast<int>(prev[1]);
            const int db = static_cast<int>(px[2]) - static_cast<int>(prev[2]);

            if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
                QoiWriteU8(static_cast<uint8_t>(
                    QOI_OP_DIFF_TAG | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2)));
            } else {
                const int dr_dg = dr - dg;
                const int db_dg = db - dg;
                if (dg >= -32 && dg <= 31 && dr_dg >= -8 && dr_dg <= 7 &&
                    db_dg >= -8 && db_dg <= 7) {
                    QoiWriteU8(static_cast<uint8_t>(QOI_OP_LUMA_TAG | (dg + 32)));
                    QoiWriteU8(static_cast<uint8_t>(((dr_dg + 8) << 4) | (db_dg + 8)));
                } else {
                    QoiWriteU8(QOI_OP_RGB_TAG);
                    QoiWriteU8(px[0]);
                    QoiWriteU8(px[1]);
                    QoiWriteU8(px[2]);
                }
            }
        } else {
            QoiWriteU8(QOI_OP_RGBA_TAG);
            QoiWriteU8(px[0]);
            QoiWriteU8(px[1]);
            QoiWriteU8(px[2]);
            QoiWriteU8(px[3]);
        }

        history[hash][0] = px[0];
        history[hash][1] = px[1];
        history[hash][2] = px[2];
        history[hash][3] = px[3];

        prev[0] = px[0];
        prev[1] = px[1];
        prev[2] = px[2];
        prev[3] = px[3];
    }

    // qoi-padding part
    for (size_t i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {
    const auto read_char = [](char &value) {
        return static_cast<bool>(std::cin.get(value));
    };
    const auto read_u8 = [](uint8_t &value) {
        char ch = 0;
        if (!std::cin.read(&ch, 1)) return false;
        value = static_cast<uint8_t>(static_cast<unsigned char>(ch));
        return true;
    };
    const auto read_u32 = [&read_u8](uint32_t &value) {
        uint8_t buf[4];
        for (int i = 0; i < 4; ++i) {
            if (!read_u8(buf[i])) return false;
        }
        value = static_cast<uint32_t>(buf[0]) << 24 | static_cast<uint32_t>(buf[1]) << 16 |
                static_cast<uint32_t>(buf[2]) << 8 | static_cast<uint32_t>(buf[3]);
        return true;
    };

    char c1 = 0, c2 = 0, c3 = 0, c4 = 0;
    if (!read_char(c1) || !read_char(c2) || !read_char(c3) || !read_char(c4)) return false;
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    // read image width
    if (!read_u32(width)) return false;
    // read image height
    if (!read_u32(height)) return false;
    // read channel number
    if (!read_u8(channels)) return false;
    // read color space specifier
    if (!read_u8(colorspace)) return false;

    if (width == 0 || height == 0) return false;
    if (channels != 3u && channels != 4u) return false;
    if (colorspace > 1u) return false;

    int run = 0;
    const uint64_t px_num = static_cast<uint64_t>(width) * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t px[4] = {0u, 0u, 0u, 255u};

    for (uint64_t i = 0; i < px_num; ++i) {
        if (run > 0) {
            --run;
        } else {
            uint8_t tag = 0;
            if (!read_u8(tag)) return false;

            if (tag == QOI_OP_RGB_TAG) {
                if (!read_u8(px[0]) || !read_u8(px[1]) || !read_u8(px[2])) return false;
            } else if (tag == QOI_OP_RGBA_TAG) {
                if (!read_u8(px[0]) || !read_u8(px[1]) || !read_u8(px[2]) || !read_u8(px[3])) {
                    return false;
                }
            } else if ((tag & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
                const int index = tag & 0x3f;
                px[0] = history[index][0];
                px[1] = history[index][1];
                px[2] = history[index][2];
                px[3] = history[index][3];
            } else if ((tag & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
                px[0] = static_cast<uint8_t>(px[0] + (((tag >> 4) & 0x03) - 2));
                px[1] = static_cast<uint8_t>(px[1] + (((tag >> 2) & 0x03) - 2));
                px[2] = static_cast<uint8_t>(px[2] + ((tag & 0x03) - 2));
            } else if ((tag & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
                uint8_t next = 0;
                if (!read_u8(next)) return false;
                const int dg = (tag & 0x3f) - 32;
                const int dr_dg = ((next >> 4) & 0x0f) - 8;
                const int db_dg = (next & 0x0f) - 8;
                px[0] = static_cast<uint8_t>(px[0] + dg + dr_dg);
                px[1] = static_cast<uint8_t>(px[1] + dg);
                px[2] = static_cast<uint8_t>(px[2] + dg + db_dg);
            } else {
                run = tag & 0x3f;
            }
        }

        const int hash = QoiColorHash(px[0], px[1], px[2], px[3]);
        history[hash][0] = px[0];
        history[hash][1] = px[1];
        history[hash][2] = px[2];
        history[hash][3] = px[3];

        QoiWriteU8(px[0]);
        QoiWriteU8(px[1]);
        QoiWriteU8(px[2]);
        if (channels == 4u) QoiWriteU8(px[3]);
    }

    bool valid = true;
    for (size_t i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        uint8_t byte = 0;
        if (!read_u8(byte)) return false;
        if (byte != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
