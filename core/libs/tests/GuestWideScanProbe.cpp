#include "SceTypes.hpp"
#include <array>
#include <cstdarg>
#include <cstdio>
#include <exception>
#include <cstdlib>

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
        auto scan = list ? ScanList : sscanf_nid_postfix;
        std::array<char16_t, 4> output{u'!', u'!', u'!', u'!'};
        int count = -1;
        int number = -1;
        if (scan("  ab 42", "%2ls%n %d", output.data(), &count, &number) != 2 ||
            output != std::array<char16_t, 4>{u'a', u'b', u'\0', u'!'} || count != 4 || number != 42) ++failures;
        output.fill(u'!');
        if (scan("abZ", "%*2lc%lc", output.data()) != 1 || output[0] != u'Z' || output[1] != u'!') ++failures;
        output.fill(u'!');
        if (scan("", "%lc", output.data()) != EOF || output[0] != u'!') ++failures;
        if (scan("12", "%2l[a-z]", output.data()) != 0 || output[0] != u'!') ++failures;
        if (scan("\xe9", "%lc", output.data()) != 1 || output[0] != 0xe9 || output[1] != u'!') ++failures;
        output.fill(u'!');
        if (scan("]x", "%2l[]x]", output.data()) != 1 ||
            output != std::array<char16_t, 4>{u']', u'x', u'\0', u'!'}) ++failures;
        output.fill(u'!');
        if (scan("ab1", "%2l[^0-9]", output.data()) != 1 ||
            output != std::array<char16_t, 4>{u'a', u'b', u'\0', u'!'}) ++failures;
        wchar_t native[4] = {L'!', L'!', L'!', L'!'};
        output.fill(u'!');
        const int expected = std::sscanf("a", "%2lc", native);
        const int actual = scan("a", "%2lc", output.data());
        std::printf("partial lc: host=%d guest=%d host[0]=%u guest[0]=%u\n", expected, actual,
            static_cast<unsigned>(native[0]), static_cast<unsigned>(output[0]));
        if (actual != expected) ++failures;
        char* allocated = nullptr;
        char* nativeAllocated = nullptr;
        const int nativeMixed = std::sscanf("a word", "%lc %ms", native, &nativeAllocated);
        const int guestMixed = scan("a word", "%lc %ms", output.data(), &allocated);
        std::printf("allocated mixed: host=%d guest=%d\n", nativeMixed, guestMixed);
        if (guestMixed != nativeMixed) ++failures;
        std::free(nativeAllocated);
        std::free(allocated);
    }
    std::printf("wide scan failures: %d\n", failures);
    return failures != 0;
}
