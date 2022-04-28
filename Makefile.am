ACLOCAL_AMFLAGS = -I build-aux/m4
AM_CXXFLAGS = $(WARN_CXXFLAGS) $(NOWARN_CXXFLAGS) $(DEBUG_CXXFLAGS) $(SANITIZER_CXXFLAGS)
AM_CPPFLAGS = $(DEBUG_CPPFLAGS)
AM_LDFLAGS = $(SANITIZER_LDFLAGS)

include sources.mk

#include_HEADERS = $(UTREEXO_DIST_HEADER_INT)
#include_HEADERS = $(UTREEXO_DIST_HEADER_INT) $(UTREEXO_LIB_HEADERS_INT)

LIBUTREEXO_COMMON = libutreexo_common.la
LIBUTREEXO_V1 = libutreexo_v1.la
LIBUTREEXO = libutreexo.la

lib_LTLIBRARIES =
lib_LTLIBRARIES += $(LIBUTREEXO)

noinst_LTLIBRARIES =
noinst_LTLIBRARIES += $(LIBUTREEXO_V1) $(LIBUTREEXO_COMMON)

BUILD_DEFINES = $(RELEASE_DEFINES)
if USE_TESTS
BUILD_DEFINES = $(VERIFY_DEFINES)
endif

libutreexo_common_la_SOURCES = $(UTREEXO_LIB_COMMON_INT)
libutreexo_common_la_CPPFLAGS = -I$(srcdir)/src $(AM_CPPFLAGS) $(BUILD_DEFINES)
libutreexo_common_la_CXXFLAGS = $(AM_CXXFLAGS)
libutreexo_common_la_LDFLAGS = $(AM_LDFLAGS)

libutreexo_v1_la_SOURCES = $(UTREEXO_LIB_V1_SOURCES_INT)
libutreexo_v1_la_CPPFLAGS = -I$(srcdir)/include -I$(srcdir)/src -I$(srcdir)/src/v1 $(AM_CPPFLAGS) $(BUILD_DEFINES)
libutreexo_v1_la_CXXFLAGS = $(AM_CXXFLAGS)
libutreexo_v1_la_LIBADD = $(LIBUTREEXO_COMMON)
libutreexo_v1_la_LDFLAGS = $(AM_LDFLAGS)

libutreexo_la_SOURCES = $(UTREEXO_LIB_SOURCES)
libutreexo_la_CPPFLAGS = -I$(srcdir)/include -I$(srcdir)/src $(AM_CPPFLAGS) $(BUILD_DEFINES)
libutreexo_la_CXXFLAGS = $(AM_CXXFLAGS)
libutreexo_la_LIBADD = $(LIBUTREEXO_V1)
libutreexo_la_LDFLAGS = $(AM_LDFLAGS)

if USE_TESTS
noinst_PROGRAMS = test-verify
TESTS = test-verify
endif

test_verify_SOURCES = $(UTREEXO_TEST_SOURCES_INT) 
test_verify_CPPFLAGS = -I$(srcdir)/include -I$(srcdir)/src -I$(srcdir)/src/test $(AM_CPPFLAGS) $(BUILD_DEFINES)
test_verify_LDADD = $(LIBUTREEXO)
test_verify_LDFLAGS = $(AM_LDFLAGS)

if ENABLE_FUZZ
noinst_PROGRAMS = fuzz
TESTS = fuzz
endif

fuzz_SOURCES = $(UTREEXO_FUZZ_SOURCES_INT) 
fuzz_CPPFLAGS = -I$(srcdir)/src -I$(srcdir)/fuzz $(AM_CPPFLAGS) $(BUILD_DEFINES)
fuzz_CXXFLAGS = $(AM_CXXFLAGS)
fuzz_LDADD = $(LIBUTREEXO)
fuzz_LDFLAGS = $(AM_LDFLAGS)
