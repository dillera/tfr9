/**
 * @brief   strncat(), which CMOC's library (0.1.97) does not have
 * @license gpl v. 3, see LICENSE for details.
 */

#include <cmoc.h>

char *strncat(char *dest, const char *src, size_t n)
{
  char *d = dest + strlen(dest);

  while (n-- && *src)
    *d++ = *src++;
  *d = '\0';
  return dest;
}
