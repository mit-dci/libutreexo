ACLOCAL_AMFLAGS = -I build-aux/m4
AM_CXXFLAGS = $(WARN_CXXFLAGS) $(NOWARN_CXXFLAGS) $(DEBUG_CXXFLAGS) $(SANITIZER_CXXFLAGS)
AM_CPPFLAGS = $(DEBUG_CPPFLAGS)
AM_LDFLAGS = $(SANITIZER_LDFLAGS)

include sources.mk

LIBUTREEXO_CRYPTO = libutreexo_crypto.la

noinst_LTLIBRARIES =
noinst_LTLIBRARIES += $(LIBUTREEXO_CRYPTO)

BUILD_DEFINES = $(RELEASE_DEFINES)
if USE_TESTS
BUILD_DEFINES = $(VERIFY_DEFINES)
endif

libutreexo_crypto_la_SOURCES = $(UTREEXO_LIB_CRYPTO_INT)
libutreexo_crypto_la_CPPFLAGS = -I$(srcdir)/src $(AM_CPPFLAGS) $(BUILD_DEFINES)
libutreexo_crypto_la_CXXFLAGS = $(AM_CXXFLAGS)
libutreexo_crypto_la_LDFLAGS = $(AM_LDFLAGS)

if USE_TESTS
noinst_PROGRAMS = test-verify
TESTS = test-verify
endif

test_verify_SOURCES = $(UTREEXO_TEST_SOURCES_INT) 
test_verify_CPPFLAGS = -I$(srcdir)/src -I$(srcdir)/include -I$(srcdir)/src/test $(AM_CPPFLAGS) $(BUILD_DEFINES)
test_verify_LDADD = $(LIBUTREEXO_CRYPTO)
test_verify_LDFLAGS = $(AM_LDFLAGS)

if ENABLE_FUZZ
noinst_PROGRAMS = fuzz
TESTS = fuzz
endif

fuzz_SOURCES = $(UTREEXO_FUZZ_SOURCES_INT) 
fuzz_CPPFLAGS = -I$(srcdir)/include -I$(srcdir)/fuzz $(AM_CPPFLAGS) $(BUILD_DEFINES)
fuzz_CXXFLAGS = $(AM_CXXFLAGS)
fuzz_LDADD = $(LIBUTREEXO_CRYPTO)
fuzz_LDFLAGS = $(AM_LDFLAGS)
