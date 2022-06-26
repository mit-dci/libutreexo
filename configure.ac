AC_INIT([utreexo], [0.0.1], [http://github.com/dergoegge/libutreexo])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PREREQ(2.60)
AC_CONFIG_SRCDIR([src/accumulator.cpp])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux/m4])
AM_INIT_AUTOMAKE([subdir-objects foreign])

LT_INIT
LT_LANG([C++])
AC_LANG([C++])

AC_PATH_PROG(CCACHE,ccache)

AC_ARG_ENABLE([ccache],
  [AS_HELP_STRING([--disable-ccache],
  [do not use ccache for building (default is to use if found)])],
  [use_ccache=$enableval],
  [use_ccache=auto])

AC_ARG_ENABLE(tests,
    AS_HELP_STRING([--enable-tests],[compile tests (default is yes)]),
    [use_tests=$enableval],
    [use_tests=yes])

AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
                    [use compiler flags and macros suited for debugging (default is no)])],
    [enable_debug=$enableval],
    [enable_debug=no])

AC_ARG_ENABLE(bench,
    AS_HELP_STRING([--disable-bench],[do not compile benchmarks (default is to compile)]),
    [use_bench=$enableval],
    [use_bench=yes])

AC_ARG_ENABLE(profiling-demo,
    AS_HELP_STRING([--enable-profiling-demo],[build profiling demo (default is no).]),
    [enable_profiling=$enableval],
    [enable_profiling=no])

AC_ARG_ENABLE([fuzz],
    AS_HELP_STRING([--enable-fuzz],
    [build for fuzzing (default no). enabling this will disable all other targets.]),
    [enable_fuzz=$enableval],
    [enable_fuzz=no])

AC_ARG_ENABLE([fuzz-cov],
    AS_HELP_STRING([--enable-fuzz-cov],
    [enable fuzz coverage reporting.]),
    [enable_fuzz_cov=$enableval],
    [enable_fuzz_cov=no])

AX_CHECK_COMPILE_FLAG([-Werror],[CXXFLAG_WERROR="-Werror"],[CXXFLAG_WERROR=""])

AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory], [nodefault])

AX_CHECK_COMPILE_FLAG([-Wall],[WARN_CXXFLAGS="$WARN_CXXFLAGS -Wall"],,[[$CXXFLAG_WERROR]])
## Some compilers (gcc) ignore unknown -Wno-* options, but warn about all
## unknown options if any other warning is produced. Test the -Wfoo case, and
## set the -Wno-foo case if it works.
AX_CHECK_COMPILE_FLAG([-Wshift-count-overflow],[NOWARN_CXXFLAGS="$NOWARN_CXXFLAGS -Wno-shift-count-overflow"],,[[$CXXFLAG_WERROR]])

dnl SUPPRESSED_CPPFLAGS=SUPPRESS_WARNINGS([$SOME_CPPFLAGS])
dnl Replace -I with -isystem in $SOME_CPPFLAGS to suppress warnings from
dnl headers from its include directories and return the result.
dnl See -isystem documentation:
dnl https://gcc.gnu.org/onlinedocs/gcc/Directory-Options.html
dnl https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-clang-isystem-directory
dnl Do not change "-I/usr/include" to "-isystem /usr/include" because that
dnl is not necessary (/usr/include is already a system directory) and because
dnl it would break GCC's #include_next.
AC_DEFUN([SUPPRESS_WARNINGS],
         [$(echo $1 |${SED} -E -e 's/(^| )-I/\1-isystem /g' -e 's;-isystem /usr/include([/ ]|$);-I/usr/include\1;g')])

if test x$enable_fuzz = xyes; then
  use_tests=no
  use_bench=no
  ## TODO make these configurable
  SANITIZER_LDFLAGS="-fsanitize=fuzzer,address,undefined"
  SANITIZER_CXXFLAGS="-fsanitize=fuzzer,address,undefined"
fi

if test x$enable_fuzz_cov = xyes; then
  use_tests=no
  use_bench=no
  CXXFLAGS="$CXXFLAGS -fprofile-instr-generate -fcoverage-mapping"
fi

if test "x$enable_debug" = xyes; then
  dnl Clear default -g -O2 flags
  ## TODO: dont reset CXXFLAGS when they are already set.
  CXXFLAGS=""

  dnl Disable all optimizations
  AX_CHECK_COMPILE_FLAG([-O0], [[DEBUG_CXXFLAGS="$DEBUG_CXXFLAGS -O0"]],,[[$CXXFLAG_WERROR]])

  dnl Prefer -g3, fall back to -g if that is unavailable.
  AX_CHECK_COMPILE_FLAG(
    [-g3],
    [[DEBUG_CXXFLAGS="$DEBUG_CXXFLAGS -g3"]],
    [AX_CHECK_COMPILE_FLAG([-g],[[DEBUG_CXXFLAGS="$DEBUG_CXXFLAGS -g"]],,[[$CXXFLAG_WERROR]])],
    [[$CXXFLAG_WERROR]])

  AX_CHECK_PREPROC_FLAG([-DDEBUG],[[DEBUG_CPPFLAGS="$DEBUG_CPPFLAGS -DDEBUG"]],,[[$CXXFLAG_WERROR]])
  AX_CHECK_PREPROC_FLAG([-DDEBUG_LOCKORDER],[[DEBUG_CPPFLAGS="$DEBUG_CPPFLAGS -DDEBUG_LOCKORDER"]],,[[$CXXFLAG_WERROR]])
  AX_CHECK_PREPROC_FLAG([-DABORT_ON_FAILED_ASSUME],[[DEBUG_CPPFLAGS="$DEBUG_CPPFLAGS -DABORT_ON_FAILED_ASSUME"]],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-ftrapv],[DEBUG_CXXFLAGS="$DEBUG_CXXFLAGS -ftrapv"],,[[$CXXFLAG_WERROR]])
fi

## Check for boost test framework if test are enabled.
if test x$use_tests = xyes; then

  dnl Minimum required Boost version
  define(MINIMUM_REQUIRED_BOOST, 1.58.0)

  dnl Check for Boost libs
  AX_BOOST_BASE([MINIMUM_REQUIRED_BOOST])

  if test x$suppress_external_warnings != xno; then
    BOOST_CPPFLAGS=SUPPRESS_WARNINGS($BOOST_CPPFLAGS)
  fi

  AX_BOOST_UNIT_TEST_FRAMEWORK

  dnl Determine if -DBOOST_TEST_DYN_LINK is needed
  AC_MSG_CHECKING([for dynamic linked boost test])
  TEMP_LIBS="$LIBS"
  LIBS="$LIBS $BOOST_LDFLAGS $BOOST_UNIT_TEST_FRAMEWORK_LIB"
  TEMP_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
  AC_LINK_IFELSE([AC_LANG_SOURCE([
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
       ])],
    [AC_MSG_RESULT(yes)]
    [TEST_DEFINES="$TEST_DEFINES -DBOOST_TEST_DYN_LINK"],
    [AC_MSG_RESULT(no)])
  LIBS="$TEMP_LIBS"
  CPPFLAGS="$TEMP_CPPFLAGS"

fi

if test "x$use_ccache" != "xno"; then
  AC_MSG_CHECKING(if ccache should be used)
  if test x$CCACHE = x; then
    if test "x$use_ccache" = "xyes"; then
      AC_MSG_ERROR([ccache not found.]);
    else
      use_ccache=no
    fi
  else
    use_ccache=yes
    CC="$ac_cv_path_CCACHE $CC"
    CXX="$ac_cv_path_CCACHE $CXX"
  fi
  AC_MSG_RESULT($use_ccache)
fi
if test "x$use_ccache" = "xyes"; then
    AX_CHECK_COMPILE_FLAG([-Qunused-arguments],[NOWARN_CXXFLAGS="$NOWARN_CXXFLAGS -Qunused-arguments"],,[[$CXXFLAG_WERROR]])
fi

VERIFY_DEFINES=-DUTREEXO_VERIFY
RELEASE_DEFINES=

AC_CONFIG_FILES([
    Makefile
])

AC_SUBST(DEBUG_CPPFLAGS)
AC_SUBST(DEBUG_CXXFLAGS)
AC_SUBST(WARN_CXXFLAGS)
AC_SUBST(NOWARN_CXXFLAGS)
AC_SUBST(VERIFY_DEFINES)
AC_SUBST(RELEASE_DEFINES)
AC_SUBST(SANITIZER_LDFLAGS)
AC_SUBST(SANITIZER_CXXFLAGS)
AM_CONDITIONAL([USE_TESTS], [test x"$use_tests" != x"no"])
AM_CONDITIONAL([ENABLE_BENCH], [test "$use_bench" = "yes"])
AM_CONDITIONAL([ENABLE_PROFILING], [test "$enable_profiling" = "yes"])
AM_CONDITIONAL([ENABLE_FUZZ], [test x"$enable_fuzz" != x"no"])
AC_OUTPUT

echo
echo "Options used to compile:"
if test $enable_fuzz = "no"; then
    echo "  with test       = $use_tests"
else
    echo "  with test       = not building test_verify because fuzzing is enabled"
fi
echo "  with bench      = $use_bench"
echo "  debug enabled   = $enable_debug"
echo "  profiling demo  = $enable_profiling"
echo