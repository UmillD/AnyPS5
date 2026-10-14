#include "SceTypes.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include <array>
#include <cstdarg>
#include <cstdio>
#include <exception>
#include <source_location>
#include <cstdlib>
#include <cstring>

extern "C" int APS5_VABI sscanf_nid_postfix(const char*, const char*, ...);
extern "C" int APS5_VABI vsscanf_nid_postfix(const char*, const char*, VaList*);
extern "C" void APS5_VABI free_nid_postfix(void*);

static int ScanList(const char* input, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    int result;
    try { result = vsscanf_nid_postfix(input, format, reinterpret_cast<VaList*>(args)); }
    catch (...) { va_end(args); throw; }
    va_end(args);
    return result;
}

static void* APS5_VABI Allocate(std::size_t bytes) { return GuestHeap::GuestHeapAllocate_nid_postfix(bytes); }
static void APS5_VABI Release(void* pointer) { GuestHeap::GuestHeapFree_nid_postfix(pointer); }
static void* APS5_VABI Unused() { std::abort(); }

int main() {
    const std::array<void*, 10> api{reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&Unused),
        reinterpret_cast<void*>(&Unused), reinterpret_cast<void*>(&Unused), reinterpret_cast<void*>(&Unused), reinterpret_cast<void*>(&Unused)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    int failures = 0;
    const auto fail = [&](std::source_location location = std::source_location::current()) {
        std::fprintf(stderr, "wide scan check failed at line %u\n", location.line());
        ++failures;
    };
    for (const bool list : {false, true}) {
        for (const char* format : {"%2lc", "%2ls", "%2l[a-z]"}) {
            std::array<char16_t, 4> output{u'!', u'!', u'!', u'!'};
            try {
                const int result = list ? ScanList("abc", format, output.data()) : sscanf_nid_postfix("abc", format, output.data());
                const bool character = format[3] == 'c';
                if (result != 1 || output[0] != u'a' || output[1] != u'b' ||
                    output[2] != (character ? u'!' : u'\0') || output[3] != u'!') {
                    std::fprintf(stderr, "%s %s: incorrect result or overwritten destination guard\n", list ? "vsscanf" : "sscanf", format);
                    fail();
                }
            } catch (const std::exception& error) {
                std::fprintf(stderr, "%s %s: %s\n", list ? "vsscanf" : "sscanf", format, error.what());
                fail();
            }
        }
        auto scan = list ? ScanList : sscanf_nid_postfix;
        std::array<char16_t, 4> output{u'!', u'!', u'!', u'!'};
        int count = -1;
        int number = -1;
        if (scan("  ab 42", "%2ls%n %d", output.data(), &count, &number) != 2 ||
            output != std::array<char16_t, 4>{u'a', u'b', u'\0', u'!'} || count != 4 || number != 42) fail();
        output.fill(u'!');
        if (scan("abZ", "%*2lc%lc", output.data()) != 1 || output[0] != u'Z' || output[1] != u'!') fail();
        output.fill(u'!');
        if (scan("", "%lc", output.data()) != EOF || output[0] != u'!') fail();
        if (scan("12", "%2l[a-z]", output.data()) != 0 || output[0] != u'!') fail();
        if (scan("\xe9", "%lc", output.data()) != 1 || output[0] != 0xe9 || output[1] != u'!') fail();
        output.fill(u'!');
        if (scan("]x", "%2l[]x]", output.data()) != 1 ||
            output != std::array<char16_t, 4>{u']', u'x', u'\0', u'!'}) fail();
        output.fill(u'!');
        if (scan("ab1", "%2l[^0-9]", output.data()) != 1 ||
            output != std::array<char16_t, 4>{u'a', u'b', u'\0', u'!'}) fail();
        wchar_t native[4] = {L'!', L'!', L'!', L'!'};
        output.fill(u'!');
        const int expected = std::sscanf("a", "%2lc", native);
        const int actual = scan("a", "%2lc", output.data());
        if (actual != expected) fail();
        char* allocated = nullptr;
        char* nativeAllocated = nullptr;
        const int nativeMixed = std::sscanf("a word", "%lc %ms", native, &nativeAllocated);
        const int guestMixed = scan("a word", "%lc %ms", output.data(), &allocated);
        if (guestMixed != nativeMixed || !allocated || std::strcmp(allocated, "word") != 0) fail();
        std::free(nativeAllocated);
        free_nid_postfix(allocated);
        allocated = nullptr;
        if (scan("a xy", "%lc %2mc", output.data(), &allocated) != 2 || !allocated || std::memcmp(allocated, "xy", 2) != 0) fail();
        free_nid_postfix(allocated);
        allocated = nullptr;
        if (scan("a abc1", "%lc %m[a-z]", output.data(), &allocated) != 2 || !allocated || std::strcmp(allocated, "abc") != 0) fail();
        free_nid_postfix(allocated);
        char16_t* allocatedWide = nullptr;
        if (scan("abc", "%2mls", &allocatedWide) != 1 || !allocatedWide || allocatedWide[0] != u'a' || allocatedWide[1] != u'b' || allocatedWide[2] != 0) fail();
        free_nid_postfix(allocatedWide);
    }
    std::printf("wide scan failures: %d\n", failures);
    return failures != 0;
}
