ACLOCAL_AMFLAGS = -I build-aux/m4
AM_CXXFLAGS = $(WARN_CXXFLAGS) $(NOWARN_CXXFLAGS)

include sources.mk

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

libutreexo_verify_la_SOURCES = $(UTREEXO_LIB_SOURCES_INT)
libutreexo_verify_la_CPPFLAGS = -I$(srcdir)/src $(AM_CPPFLAGS) $(VERIFY_DEFINES)
libutreexo_verify_la_CXXFLAGS = $(AM_CXXFLAGS)

if USE_TESTS
noinst_PROGRAMS = test-verify
TESTS = test-verify
endif

test_verify_SOURCES = $(UTREEXO_TEST_SOURCES_INT) 
test_verify_CPPFLAGS = -I$(srcdir)/src $(AM_CPPFLAGS) $(VERIFY_DEFINES)
test_verify_LDADD = $(LIBUTREEXO_VERIFY)
test_verify_LDFLAGS = $(AM_LDFLAGS) -static

