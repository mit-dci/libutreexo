UTREEXO_LIB_SOURCES = 
UTREEXO_LIB_SOURCES += %reldir%/src/accumulator.cpp
UTREEXO_LIB_SOURCES += %reldir%/src/prove.cpp
UTREEXO_LIB_SOURCES += %reldir%/src/verify.cpp
UTREEXO_LIB_SOURCES += %reldir%/src/modify.cpp
UTREEXO_LIB_SOURCES += %reldir%/src/undo.cpp

UTREEXO_LIB_COMMON_INT =
UTREEXO_LIB_COMMON_INT += %reldir%/src/state.cpp
UTREEXO_LIB_COMMON_INT += %reldir%/src/crypto/sha512.cpp

UTREEXO_LIB_V1_SOURCES_INT =
UTREEXO_LIB_V1_SOURCES_INT += %reldir%/src/v1/interface.cpp
UTREEXO_LIB_V1_SOURCES_INT += %reldir%/src/v1/pollard.cpp
UTREEXO_LIB_V1_SOURCES_INT += %reldir%/src/v1/ram_forest.cpp

UTREEXO_TEST_SOURCES_INT = 
UTREEXO_TEST_SOURCES_INT += %reldir%/src/test/tests.cpp 
UTREEXO_TEST_SOURCES_INT += %reldir%/src/test/utils.cpp 
UTREEXO_TEST_SOURCES_INT += %reldir%/src/test/accumulator_tests.cpp
UTREEXO_TEST_SOURCES_INT += %reldir%/src/test/swapless_tests.cpp
UTREEXO_TEST_SOURCES_INT += %reldir%/src/test/state_tests.cpp

UTREEXO_FUZZ_SOURCES_INT =
UTREEXO_FUZZ_SOURCES_INT += %reldir%/src/fuzz/fuzz.cpp
UTREEXO_FUZZ_SOURCES_INT += %reldir%/src/fuzz/forest_state.cpp
UTREEXO_FUZZ_SOURCES_INT += %reldir%/src/fuzz/batchproof.cpp
UTREEXO_FUZZ_SOURCES_INT += %reldir%/src/fuzz/pollard.cpp
