/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/*  File   : strappend.c
    Author : Monty
    Updated: 1987.02.07
    Defines: strappend()

    strappend(dest, len, fill) appends fill-characters to a string so that
    the result length == len. If the string is longer than len it's
    trunked. The des+len character is allways set to NULL.
*/

#include <global.h>
#include "m_string.h"


void strappend(s,len,fill)
register char *s;
pchar fill;
uint len;
{
  register char *endpos;

  endpos = s+len;
  while (*s++);
  s--;
  while (s<endpos) *(s++) = fill;
  *(endpos) = '\0';
} /* strappend */


