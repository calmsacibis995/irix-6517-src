/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#include "mysys_priv.h"
#include <m_string.h>

	/* Pack a filename for output on screen */
	/* Changes long paths to .../ */
	/* Removes pathname and extension */
	/* If not possibly to pack returns '?' in to and returns 1*/

int pack_filename(my_string to, my_string name, size_s max_length)
					/* to may be name */

{
  int i;
  char buff[FN_REFLEN];

  if (strlen(fn_format(to,name,"","",0)) <= max_length)
    return 0;
  if (strlen(fn_format(to,name,"","",8)) <= max_length)
    return 0;
  if (strlen(fn_format(buff,name,".../","",1)) <= max_length)
  {
    VOID(strmov(to,buff));
    return 0;
  }
  for (i= 0 ; i < 3 ; i++)
  {
    if (strlen(fn_format(buff,to,"","", i == 0 ? 2 : i == 1 ? 1 : 3 ))
	<= max_length)
    {
      VOID(strmov(to,buff));
      return 0;
    }
  }
  to[0]='?'; to[1]=0;				/* Can't pack filename */
  return 1;
} /* pack_filename */
