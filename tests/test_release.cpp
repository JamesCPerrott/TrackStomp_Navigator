#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#ifndef TUSB_CONFIG_PATH
#error "TUSB_CONFIG_PATH is not defined"
#endif
#ifndef CMAKE_LISTS_PATH
#error "CMAKE_LISTS_PATH is not defined"
#endif

namespace {

std::string slurp(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "failed to open " << path << '\n';
        return {};
    }
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

} // namespace

int main() {
    const std::string tusb = slurp(TUSB_CONFIG_PATH);
    if (tusb.empty()) {
        return 1;
    }
    if (!contains(tusb, "#define CFG_TUD_CDC    0") ||
        !contains(tusb, "#define CFG_TUD_MSC    0") ||
        !contains(tusb, "#define CFG_TUD_VENDOR 0") ||
        !contains(tusb, "#define CFG_TUSB_DEBUG 0")) {
        std::cerr << "tusb_config.h must disable CDC, MSC, vendor, and TinyUSB debug\n";
        return 1;
    }

    const std::string cmake = slurp(CMAKE_LISTS_PATH);
    if (cmake.empty()) {
        return 1;
    }
    if (!contains(cmake, "option(DEBUG_UART \"Enable UART-only stdio on GP0/GP1\" OFF)") ||
        !contains(cmake, "pico_enable_stdio_usb(trackstomp_navigator 0)")) {
        std::cerr << "CMakeLists.txt must default DEBUG_UART OFF and disable USB stdio\n";
        return 1;
    }

    return 0;
}
