/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* T|mmer en str{ng p{ slut_space */

#include "mysys_priv.h"

/*
	stripp_sp(my_string str)
	Strips end-space from string and returns new length.
*/

size_s stripp_sp(register my_string str)
{
  reg2 my_string found;
  reg3 my_string start;

  start=found=str;

  while (*str)
  {
    if (*str != ' ')
    {
      while (*++str && *str != ' ') {};
      if (!*str)
	return (size_s) (str-start);	/* Return stringlength */
    }
    found=str;
    while (*++str == ' ') {};
  }
  *found= '\0';				/* Stripp at first space */
  return (size_s) (found-start);
} /* stripp_sp */
