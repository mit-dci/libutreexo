ACLOCAL_AMFLAGS = -I build-aux/m4
AM_CXXFLAGS = $(WARN_CXXFLAGS) $(NOWARN_CXXFLAGS) $(DEBUG_CXXFLAGS) $(SANITIZER_CXXFLAGS)
AM_CPPFLAGS = $(DEBUG_CPPFLAGS)
AM_LDFLAGS = $(SANITIZER_LDFLAGS)

include sources.mk

bin_PROGRAMS =

#include_HEADERS = $(UTREEXO_DIST_HEADER_INT)
#include_HEADERS = $(UTREEXO_DIST_HEADER_INT) $(UTREEXO_LIB_HEADERS_INT)

LIBUTREEXO = libutreexo.la
if USE_TESTS
LIBUTREEXO_VERIFY = libutreexo_verify.la
endif

lib_LTLIBRARIES =
lib_LTLIBRARIES += $(LIBUTREEXO)

noinst_LTLIBRARIES =
noinst_LTLIBRARIES += $(LIBUTREEXO_VERIFY)

libutreexo_la_SOURCES = $(UTREEXO_LIB_SOURCES_INT)
libutreexo_la_CPPFLAGS = -I$(srcdir)/src $(AM_CPPFLAGS) $(RELEASE_DEFINES)
libutreexo_la_CXXFLAGS = $(AM_CXXFLAGS)
libutreexo_la_LDFLAGS = $(AM_LDFLAGS)

libutreexo_verify_la_SOURCES = $(UTREEXO_LIB_SOURCES_INT)
libutreexo_verify_la_CPPFLAGS = -I$(srcdir)/src $(AM_CPPFLAGS) $(VERIFY_DEFINES)
libutreexo_verify_la_CXXFLAGS = $(AM_CXXFLAGS)
libutreexo_verify_la_LDFLAGS = $(AM_LDFLAGS)

if USE_TESTS
noinst_PROGRAMS = test-verify
TESTS = test-verify
endif

test_verify_SOURCES = $(UTREEXO_TEST_SOURCES_INT) 
test_verify_CPPFLAGS = -I$(srcdir)/src $(AM_CPPFLAGS) $(VERIFY_DEFINES)
test_verify_LDADD = $(LIBUTREEXO_VERIFY)
test_verify_LDFLAGS = $(AM_LDFLAGS)

if ENABLE_FUZZ
noinst_PROGRAMS = fuzz
TESTS = fuzz
endif

fuzz_SOURCES = $(UTREEXO_FUZZ_SOURCES_INT) 
fuzz_CPPFLAGS = -I$(srcdir)/src -I$(srcdir)/fuzz $(AM_CPPFLAGS) $(RELEASE_DEFINES)
fuzz_CXXFLAGS = $(AM_CXXFLAGS)
fuzz_LDADD = $(LIBUTREEXO)
fuzz_LDFLAGS = $(AM_LDFLAGS)

if ENABLE_BENCH
bin_PROGRAMS += bench_utreexo
bench_utreexo_SOURCES = ${UTREEXO_BENCH_SOURCES_INT}
bench_utreexo_CPPFLAGS = -I$(srcdir)/src $(AM_CPPFLAGS)
bench_utreexo_CXXFLAGS = $(AM_CXXFLAGS)
bench_utreexo_LDADD = $(LIBUTREEXO)
bench_utreexo_LDFLAGS = $(AM_LDFLAGS)
endif

if ENABLE_PROFILING
bin_PROGRAMS += profiling_demo
profiling_demo_SOURCES = ${UTREEXO_PROFILING_DEMO_SOURCES_INT}
profiling_demo_CPPFLAGS = -I$(srcdir)/src $(AM_CPPFLAGS)
profiling_demo_CXXFLAGS = $(AM_CXXFLAGS)
profiling_demo_LDADD = $(LIBUTREEXO)
profiling_demo_LDFLGS = $(AM_LDFLAGS)
endif