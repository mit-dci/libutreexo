#ifndef LIBUTREEXO_FUZZ
#define LIBUTREEXO_FUZZ

#include <functional>
#include <map>
#include <string_view>
#include <cstring>

using FuzzFunc = std::function<void(const uint8_t*, size_t size)>;

static std::map<std::string_view, FuzzFunc> g_targets;
void RegisterFuzzTarget(std::string_view name, FuzzFunc func);

#define FUZZ(name)                                         \
    void name##_fuzz_target(const uint8_t*, size_t);       \
    struct name##_register {                               \
        name##_register()                                  \
        {                                                  \
            RegisterFuzzTarget(#name, name##_fuzz_target); \
        }                                                  \
    } const static g_##name##_register;                    \
    void name##_fuzz_target(const uint8_t* data, size_t size)

#define FUZZ_CONSUME_UNCHECKED(type, name)  \
    type name;                              \
    std::memcpy(&name, data, sizeof(type)); \
    size -= sizeof(type);                   \
    data += sizeof(type);

#define FUZZ_CONSUME(type, name) \
    if (sizeof(type) > size) {   \
        return;                  \
    }                            \
    FUZZ_CONSUME_UNCHECKED(type, name)

#define FUZZ_CONSUME_VEC(type, name, len)      \
    if ((size_t)(sizeof(type) * len) > size) { \
        return;                                \
    }                                          \
    std::vector<type> name;                    \
    for (int i = 0; i < len; i++) {            \
        FUZZ_CONSUME_UNCHECKED(type, j);       \
        name.push_back(j);                     \
    }

#define FUZZ_CONSUME_BOOL(name)       \
    FUZZ_CONSUME(uint8_t, name_uint8) \
    bool name = 1 & name_uint8;

#endif

