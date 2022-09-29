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

/* Read a record with random-access. The position to the record must
   get by N_INFO. The next record can be read with pos= -1 */


#include "isamdef.h"

/*
	   Funktionen ger som resultat:
	   0 = Ok.
	   1 = Posten har raderats sedan senaste l{sning.
	  -1 = EOF (eller motsvarande: se errno)
*/

int ni_rrnd(N_INFO *info, byte *buf, register ulong filepos)


					/* Obs om filepos == -1 l{ses n{sta */
{
  int skipp_deleted_blocks;
  DBUG_ENTER("ni_rrnd");

  skipp_deleted_blocks=0;

  if (filepos == NI_POS_ERROR)
  {
    skipp_deleted_blocks=1;
    if (info->lastpos == NI_POS_ERROR)	/* First read ? */
      filepos= info->s->pack.header_length;	/* Read first record */
    else
      filepos= info->nextpos;
  }

  info->lastinx= -1;				/* Can't forward or backward */
  info->update&= HA_STATE_CHANGED;		/* Init all but update-flag */

  if (info->opt_flag & WRITE_CACHE_USED && flush_io_cache(&info->rec_cache))
    DBUG_RETURN(my_errno);

  DBUG_RETURN ((*info->s->read_rnd)(info,buf,filepos,skipp_deleted_blocks));
}
