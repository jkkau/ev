#ifndef EV_UTILITIES_HH_
#define EV_UTILITIES_HH_

#include <stdio.h>
#include <stdlib.h>       //exit()
#include <errno.h>
#include <string.h>       //strerror()

namespace ev {

#define check_return(r) { \
    if (r < 0) { \
        fprintf(stderr, "%s:%d -- ret:%d, error msg %s\n", __FILE__, __LINE__, r, strerror(errno)); \
        exit(0); \
    } \
}

}

#endif