#include "SceTypes.hpp"
#include <array>
#include <cstdarg>
#include <cstdio>
#include <exception>

extern "C" int APS5_VABI sscanf_nid_postfix(const char*, const char*, ...);
extern "C" int APS5_VABI vsscanf_nid_postfix(const char*, const char*, VaList*);

static int ScanList(const char* input, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    int result;
    try { result = vsscanf_nid_postfix(input, format, reinterpret_cast<VaList*>(args)); }
    catch (...) { va_end(args); throw; }
    va_end(args);
    return result;
}

int main() {
    int failures = 0;
    for (const bool list : {false, true}) {
        for (const char* format : {"%2lc", "%2ls", "%2l[a-z]"}) {
            std::array<char16_t, 4> output{u'!', u'!', u'!', u'!'};
            try {
                const int result = list ? ScanList("abc", format, output.data()) : sscanf_nid_postfix("abc", format, output.data());
                const bool character = format[3] == 'c';
                if (result != 1 || output[0] != u'a' || output[1] != u'b' ||
                    output[2] != (character ? u'!' : u'\0') || output[3] != u'!') {
                    std::fprintf(stderr, "%s %s: incorrect result or overwritten destination guard\n", list ? "vsscanf" : "sscanf", format);
                    ++failures;
                }
            } catch (const std::exception& error) {
                std::fprintf(stderr, "%s %s: %s\n", list ? "vsscanf" : "sscanf", format, error.what());
                ++failures;
            }
        }
    }
    std::printf("wide scan failures: %d/6\n", failures);
    return failures != 0;
}
