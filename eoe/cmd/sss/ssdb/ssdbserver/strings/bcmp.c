
/*
  bcmp(s1, s2, len) returns 0 if the "len" bytes starting at "s1" are
  identical to the "len" bytes starting at "s2", non-zero if they are
  different.
  Now only used with purify.
*/

#include <global.h>
#include "m_string.h"

#if !defined(bcmp) && !defined(HAVE_BCMP)

#if defined(MC68000) && defined(DS90)

int bcmp(s1,s2, len)
const char *s1;
const char *s2;
uint len;					/* 0 <= len <= 65535 */
{
  asm("		movl	12(a7),d0	");
  asm("		subqw	#1,d0		");
  asm("		blt	.L5		");
  asm("		movl	4(a7),a1	");
  asm("		movl	8(a7),a0	");
  asm(".L4:	cmpmb	(a0)+,(a1)+	");
  asm("		dbne	d0,.L4		");
  asm(".L5:	addqw	#1,d0		");
}

#else

#ifdef HAVE_purify
int my_bcmp(s1, s2, len)
#else
int bcmp(s1, s2, len)
#endif
    register const char *s1;
    register const char *s2;
    register uint len;
{
  while (len-- != 0 && *s1++ == *s2++) ;
  return len+1;
}

#endif
#endif /* BSD_FUNCS */
