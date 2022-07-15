
UTREEXO_LIB_CRYPTO_INT =
UTREEXO_LIB_CRYPTO_INT += %reldir%/src/crypto/sha512.cpp

UTREEXO_TEST_SOURCES_INT = 
UTREEXO_TEST_SOURCES_INT += %reldir%/src/test/tests.cpp 
UTREEXO_TEST_SOURCES_INT += %reldir%/src/test/utils.cpp 
UTREEXO_TEST_SOURCES_INT += %reldir%/src/test/undo_tests.cpp
UTREEXO_TEST_SOURCES_INT += %reldir%/src/test/accumulator_tests.cpp
UTREEXO_TEST_SOURCES_INT += %reldir%/src/test/state_tests.cpp

UTREEXO_FUZZ_SOURCES_INT =
UTREEXO_FUZZ_SOURCES_INT += %reldir%/src/fuzz/fuzz.cpp
UTREEXO_FUZZ_SOURCES_INT += %reldir%/src/fuzz/forest_state.cpp
UTREEXO_FUZZ_SOURCES_INT += %reldir%/src/fuzz/batchproof.cpp
