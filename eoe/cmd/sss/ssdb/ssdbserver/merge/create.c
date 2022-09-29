/* Copyright (C) 1979-1996 TcX AB & Monty Program KB & Detron HB

   This software is distributed with NO WARRANTY OF ANY KIND.  No author or
   distributor accepts any responsibility for the consequences of using it, or
   for whether it serves any particular purpose or works at all, unless he or
   she says so in writing.  Refer to the Free Public License (the "License")
   for full details.
   Every copy of this file must include a copy of the License, normally in a
   plain ASCII text file named PUBLIC.	The License grants you the right to
   copy, modify and redistribute this file, but only under certain conditions
   described in the License.  Among other things, the License requires that
   the copyright notice and this notice be preserved on all copies. */

/* Create a MERGE-file */

#include "mrgdef.h"

	/* create file named 'name' and save filenames in it
	   table_names should be NULL or a vector of string-pointers with
	   a NULL-pointer last
	   */

int mrg_create(name,table_names)
const char *name,**table_names;
{
  int save_errno;
  uint errpos;
  File file;
  char buff[FN_REFLEN],*end;
  DBUG_ENTER("mrg_create");

  errpos=0;
  if ((file = my_create(fn_format(buff,name,"",MRG_NAME_EXT,4),0,
       O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
    goto err;
  errpos=1;
  if (table_names)
    for ( ; *table_names ; table_names++)
    {
      strmov(buff,*table_names);
      fn_same(buff,name,4);
      *(end=strend(buff))='\n';
      if (my_write(file,*table_names,(uint) (end-buff+1),
		   MYF(MY_WME | MY_NABP)))
	goto err;
    }
  if (my_close(file,MYF(0)))
    goto err;
  DBUG_RETURN(0);

err:
  save_errno=my_errno;
  switch (errpos) {
  case 1:
    VOID(my_close(file,MYF(0)));
  }
  my_errno=save_errno;			/* Return right errocode */
  DBUG_RETURN(-1);
} /* mrg_create */
