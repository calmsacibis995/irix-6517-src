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

/*
  Read a record with random-access. The position to the record must
  get by mrg_info(). The next record can be read with pos= -1 */


#include "mrgdef.h"

static MRG_TABLE *find_table(MRG_TABLE *start,MRG_TABLE *end,ulong pos);

/*
	Returns same as ni_rrnd:
	   0 = Ok.
	   1 = Record deleted.
	  -1 = EOF (or something, errno should be HA_ERR_END_OF_FILE)
*/

int mrg_rrnd(info,buf,filepos)
MRG_INFO *info;
byte *buf;
register ulong filepos;			/* If filepos == -1, read next */
{
  int error;
  N_INFO *isam_info;

  if (filepos == (ulong) -1)
  {
    if (!info->current_table)
    {
      if (info->open_tables == info->end_table)
      {						/* No tables */
	my_errno=HA_ERR_END_OF_FILE;
	return -1;
      }
      isam_info=(info->current_table=info->open_tables)->table;
      if (info->cache_in_use)
	ni_extra(isam_info,HA_EXTRA_CACHE);
      filepos=isam_info->s->pack.header_length;
      isam_info->lastinx= (uint) -1;	/* Can't forward or backward */
    }
    else
    {
      isam_info=info->current_table->table;
      filepos= isam_info->nextpos;
    }

    for (;;)
    {
      isam_info->update&= HA_STATE_CHANGED;
      if ((error=(*isam_info->s->read_rnd)(isam_info,(byte*) buf,
					filepos,1)) >= 0 ||
	  my_errno != HA_ERR_END_OF_FILE)
	return (error);
      if (info->cache_in_use)
	ni_extra(info->current_table->table,HA_EXTRA_NO_CACHE);
      if (info->current_table+1 == info->end_table)
	return(-1);
      info->current_table++;
      info->last_used_table=info->current_table;
      if (info->cache_in_use)
	ni_extra(info->current_table->table,HA_EXTRA_CACHE);
      info->current_table->file_offset=
	info->current_table[-1].file_offset+
	info->current_table[-1].table->s->state.data_file_length;

      isam_info=info->current_table->table;
      filepos=isam_info->s->pack.header_length;
      isam_info->lastinx= (uint) -1;
    }
  }
  info->current_table=find_table(info->open_tables,
				 info->last_used_table,filepos);
  isam_info=info->current_table->table;
  isam_info->update&= HA_STATE_CHANGED;
  return ((*isam_info->s->read_rnd)(isam_info,(byte*) buf,
				    (ha_rows) (filepos -
					       info->current_table->file_offset),
				    0));
}


	/* Find which table to use according to file-pos */

static MRG_TABLE *find_table(start,end,pos)
MRG_TABLE *start,*end;
ulong pos;
{
  MRG_TABLE *mid,*start_of_table;

  start_of_table=start;

  while (start != end)
  {
    mid=start+((uint) (end-start)+1)/2;
    if (mid->file_offset > pos)
      end=mid-1;
    else
      start=mid;
  }
  return start;
}
