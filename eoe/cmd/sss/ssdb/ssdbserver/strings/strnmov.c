/*
    strnmov(dst,src,length) moves length characters, or until end, of src to
    dst and appends a closing NUL to dst if src is shorter than length.
    The result is a pointer to the first NUL in dst, or is dst+n if dst was
    truncated.
*/

#include <global.h>
#include "m_string.h"

char *strnmov(dst, src, n)
     reg1 char *dst;
     reg2 const char *src;
     reg3 uint n;
{
  while (n-- != 0) {
    if (!(*dst++ = *src++)) {
      return (char*) dst-1;
    }
  }
  return dst;
}
