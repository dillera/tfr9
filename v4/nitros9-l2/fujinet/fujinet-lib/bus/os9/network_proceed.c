#include "fujinet-network.h"

/**********************************************************************
 Under NitrOS-9 the FujiNet is on a becker port, which has no proceed
 (carrier detect) line to say that network data is waiting: programs
 poll network_status() instead.
**********************************************************************/

bool network_has_proceed(void)
{
  return false;
}

bool network_proceed(void)
{
  return false;
}
