// Local-only test driver: identical to main.cpp but forces binary stdio so
// Windows does not translate 0x0A <-> 0x0D0A during binary QOI I/O.
// This file is NOT part of the submission (only qoi.h is submitted).
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>

#include <fcntl.h>
#include <io.h>

#include "conv.h"
#include "qoi.h"

void QoiToPnm(std::string fType, bool omitted) {
    auto fName = "temp." + fType;

    std::fstream temp;
    temp.open(fName, std::ios::out | std::ios::binary | std::ios::trunc);

    auto backup = std::cout.rdbuf();
    auto target = temp.rdbuf();

    uint32_t width, height;
    uint8_t channels, colorspace;

    std::cout.rdbuf(target);
    QoiDecode(width, height, channels, colorspace);
    std::cout.rdbuf(backup);

    if (fType == "rgb" && channels != 3) {
        std::cerr << "image type doesnt match the channel number" << std::endl;
        return;
    }
    if (fType == "rgba" && channels != 4) {
        std::cerr << "image type doesnt match the channel number" << std::endl;
        return;
    }

    temp.close();
    temp.open(fName, std::ios::in | std::ios::binary);
    try {
        if (fType == "rgb") {
            RgbToPpm(temp, std::cout, width, height);
        } else {
            RgbaToPam(temp, std::cout, width, height);
        }
    } catch (const char *msg) {
        std::cerr << msg << std::endl;
        return;
    } catch (...) {
        std::cerr << "an error is raised during conversion" << std::endl;
    }
    temp.close();

    if (omitted) std::remove(fName.c_str());
}

void PnmToQoi(std::string fType, bool omitted) {
    auto fName = "temp." + fType;

    std::fstream temp;
    temp.open(fName, std::ios::out | std::ios::binary | std::ios::trunc);

    uint32_t width, height;
    uint8_t channels = 3u, colorspace;

    try {
        if (fType == "rgb") {
            PpmToRgb(std::cin, temp, width, height);
            channels = 3u;
        } else {
            PamToRgba(std::cin, temp, width, height);
            channels = 4u;
        }
    } catch (const char *msg) {
        std::cerr << msg << std::endl;
        return;
    } catch (...) {
        std::cerr << "an error is raised during conversion" << std::endl;
    }

    temp.close();
    temp.open(fName, std::ios::in | std::ios::binary);

    auto backup = std::cin.rdbuf();
    auto target = temp.rdbuf();

    std::cin.rdbuf(target);
    QoiEncode(width, height, channels, 0u);
    std::cin.rdbuf(backup);

    temp.close();

    if (omitted) std::remove(fName.c_str());
}

int main(int argc, char *argv[]) {
    _setmode(0, _O_BINARY);  // stdin
    _setmode(1, _O_BINARY);  // stdout

    if (argc <= 1) {
        std::cerr << "too few arguments" << std::endl;
        return 0;
    }
    std::map<std::string, bool> args;
    for (int i = 1; i < argc; ++i) args[argv[i]] = true;

    if (args["-e"] && args["-d"]) {
        std::cerr << "-d and -e options conflict" << std::endl;
        return 0;
    }

    std::string type = args["-4"] ? "rgba" : "rgb";
    if (args["-e"]) PnmToQoi(type, args["-o"]);
    if (args["-d"]) QoiToPnm(type, args["-o"]);
    return 0;
}
