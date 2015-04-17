#include "pactest.c"
#include "tap.c"
#include <alpm.h>
#include <stdio.h>

#define ASSERT(x) \
    if(!(x)) { \
        tap_bail("ASSERT FAILED %s line %d: '%s'", __FILE__, __LINE__, #x); \
        exit(1); \
    }
#define ASSERTC(x) \
    if(!(x)) { \
        tap_bail("ASSERT FAILED %s line %d: '%s'", __FILE__, __LINE__, #x); \
        cleanup(); \
        exit(1); \
    }
