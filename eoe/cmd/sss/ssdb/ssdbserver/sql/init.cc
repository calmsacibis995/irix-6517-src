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

/* Init and dummy functions for interface with unireg */

#include "mysql_priv.h"
#include <m_ctype.h>

void unireg_init(ulong options)
{
  uint i;
  double nr;
  DBUG_ENTER("unireg_init");

  MYSYS_PROGRAM_DONT_USE_CURSES();
  abort_loop=0;

  my_disable_async_io=1;		/* aioread is only in shared library */
  wild_many='%'; wild_one='_'; wild_prefix='\\'; /* Change to sql syntax */

  current_pid=(uint) getpid();		/* Save for later ref */
  init_time();				/* Init time-functions (read zone) */
#ifdef USE_MY_ATOF
  init_my_atof();			/* use our atof */
#endif
  my_abort_hook=unireg_abort;		/* Abort with close of databases */
  f_fyllchar=' ';			/* Input fill char */
  bfill(last_ref,MAX_REFLENGTH,(uchar) 255);  /* This is indexfile-last-ref */

  VOID(strmov(reg_ext,".frm"));
  for (i=0 ; i < 6 ; i++)		// YYMMDDHHMMSS
    dayord.pos[i]=i;
  specialflag=SPECIAL_SAME_DB_NAME;
  blob_newline='^';			/* Convert newline in blobs to this */
  /* Make a tab of powers of 10 */
  for (i=0,nr=1.0; i < array_elements(log_10) ; i++)
  {					/* It's used by filesort... */
    log_10[i]= nr ; nr*= 10.0;
  }
  specialflag|=options;			/* Set options from argv */

  // The following is needed because of like optimization in select.cc

  uchar max_char=my_sort_order[(uchar) max_sort_char];
  for (i = 0; i < 256; i++)
  {
    if ((uchar) my_sort_order[i] > max_char)
    {
      max_char=(uchar) my_sort_order[i];
	max_sort_char= (char) i;
    }
  }
  DBUG_VOID_RETURN;
}
