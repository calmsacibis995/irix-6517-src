/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/*  File   : strmake.c
    Author : Michael Widenius
    Updated: 20 Jul 1984
    Defines: strmake()

    strmake(dst,src,length) moves length characters, or until end, of src to
    dst and appends a closing NUL to dst.
    strmake() returns pointer to closing null;
*/

#include <global.h>
#include "m_string.h"

#ifdef BAD_STRING_COMPILER

char *strmake(dst, src, length)
char *dst;
const char *src;
uint	length;
{
  reg1 char *res;

  if ((res=memccpy(dst,src,0,length)))
    return res-1;
  dst[length]=0;
  return dst+length;
}
#define strmake strmake_overlapp	/* Use orginal for overlapping str */
#endif


char *strmake(dst, src, length)
reg1 char *dst;
reg2 const char *src;
reg3 uint  length;
{
  while (length--)
    if (! (*dst++ = *src++))
      return dst-1;
  *dst=0;
  return dst;
}
