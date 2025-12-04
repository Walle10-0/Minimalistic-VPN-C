#include "VPNtools.h"
// shared code between client and server

#include <stdlib.h>
#include <errno.h>

// I should use this more often ... or not
void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}