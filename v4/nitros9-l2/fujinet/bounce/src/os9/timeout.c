#include <cmoc.h>
#include <coco.h>

// connection.c sets fujinet-lib's fn_default_timeout, which the Atari's
// fujinet-lib has; the CoCo's, and so NitrOS-9's, has no timeout to set.
uint8_t fn_default_timeout;
