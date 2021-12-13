#include "fuzz.h"
#include <cstdlib>
#include <map>
#include <cassert>

void RegisterFuzzTarget(std::string_view name, FuzzFunc func)
{
    const auto it = g_targets.try_emplace(name, func);
    assert(it.second);
}

static FuzzFunc* g_fuzz_func{nullptr};

extern "C" int LLVMFuzzerTestOneInput(unsigned char* data, size_t size)
{
    assert(g_fuzz_func);
    (*g_fuzz_func)(data, size);
    return 0;
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv)
{
    // Get target name from "FUZZ" env variable and initialize it.
    std::string_view target{std::getenv("FUZZ")};

    const auto it = g_targets.find(target);
    assert(it != g_targets.end());

    g_fuzz_func = &it->second;
    return 0;
}
