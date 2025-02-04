#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "util.h"

void error_exit(const char *message) {
    fprintf(stderr, "Error: %s\n", message);
    perror("System error");
    exit(EXIT_FAILURE);
}
