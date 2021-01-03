// Taken and slightly modified from minisketch/util.h

#ifdef UTREEXO_VERIFY
#include <stdio.h>
#endif

/* Assertion macros */

/**
 * Unconditional failure on condition failure.
 * Primarily used in testing harnesses.
 */
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, "Check condition failed: " #cond); \
        abort(); \
    } \
} while(0)

/**
 * Check macro that does nothing in normal non-verify builds but crashes in verify builds.
 * This is used to test conditions at runtime that should always be true, but are either
 * expensive to test or in locations where returning on failure would be messy.
 */
#ifdef UTREEXO_VERIFY
#define CHECK_SAFE(cond) CHECK(cond)
#else
#define CHECK_SAFE(cond)
#endif
