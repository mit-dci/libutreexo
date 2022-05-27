UTREEXO_DIST_HEADERS_INT = 
UTREEXO_DIST_HEADERS_INT += %reldir%/include/utreexo.h

UTREEXO_LIB_HEADERS_INT = 
UTREEXO_LIB_HEADERS_INT += %reldir%/src/accumulator.h
UTREEXO_LIB_HEADERS_INT += %reldir%/src/pollard.h
UTREEXO_LIB_HEADERS_INT += %reldir%/src/ram_forest.h
UTREEXO_LIB_HEADERS_INT += %reldir%/src/attributes.h 
UTREEXO_LIB_HEADERS_INT += %reldir%/src/check.h
UTREEXO_LIB_HEADERS_INT += %reldir%/src/batchproof.h
UTREEXO_LIB_HEADERS_INT += %reldir%/src/state.h
UTREEXO_LIB_HEADERS_INT += %reldir%/src/crypto/common.h
UTREEXO_LIB_HEADERS_INT += %reldir%/src/crypto/sha512.h
UTREEXO_LIB_HEADERS_INT += %reldir%/src/compat/byteswap.h
UTREEXO_LIB_HEADERS_INT += %reldir%/src/compat/endian.h
UTREEXO_LIB_HEADERS_INT += %reldir%/src/compat/cpuid.h

UTREEXO_LIB_SOURCES_INT = 
UTREEXO_LIB_SOURCES_INT += %reldir%/src/accumulator.cpp
UTREEXO_LIB_SOURCES_INT += %reldir%/src/pollard.cpp
UTREEXO_LIB_SOURCES_INT += %reldir%/src/ram_forest.cpp
UTREEXO_LIB_SOURCES_INT += %reldir%/src/batchproof.cpp
UTREEXO_LIB_SOURCES_INT += %reldir%/src/state.cpp
UTREEXO_LIB_SOURCES_INT += %reldir%/src/crypto/sha512.cpp

UTREEXO_TEST_SOURCES_INT = 
UTREEXO_TEST_SOURCES_INT += %reldir%/src/test/tests.cpp 
UTREEXO_TEST_SOURCES_INT += %reldir%/src/test/accumulator_tests.cpp
UTREEXO_TEST_SOURCES_INT += %reldir%/src/test/state_tests.cpp

UTREEXO_FUZZ_SOURCES_INT =
UTREEXO_FUZZ_SOURCES_INT += %reldir%/src/fuzz/fuzz.cpp
UTREEXO_FUZZ_SOURCES_INT += %reldir%/src/fuzz/forest_state.cpp
UTREEXO_FUZZ_SOURCES_INT += %reldir%/src/fuzz/batchproof.cpp
UTREEXO_FUZZ_SOURCES_INT += %reldir%/src/fuzz/pollard.cpp

UTREEXO_BENCH_SOURCES_INT = 
UTREEXO_BENCH_SOURCES_INT += %reldir%/src/bench/pollard.cpp
UTREEXO_BENCH_SOURCES_INT += %reldir%/src/bench/ram_forest.cpp
UTREEXO_BENCH_SOURCES_INT += %reldir%/src/bench/bench_utreexo.cpp
UTREEXO_BENCH_SOURCES_INT += %reldir%/src/bench/bench.cpp
UTREEXO_BENCH_SOURCES_INT += %reldir%/src/bench/bench.h
UTREEXO_BENCH_SOURCES_INT += %reldir%/src/bench/nanobench.cpp
UTREEXO_BENCH_SOURCES_INT += %reldir%/src/bench/nanobench.h
UTREEXO_BENCH_SOURCES_INT += %reldir%/src/bench/util/args.h
UTREEXO_BENCH_SOURCES_INT += %reldir%/src/bench/util/args.cpp
UTREEXO_BENCH_SOURCES_INT += %reldir%/src/bench/util/leaves.h
