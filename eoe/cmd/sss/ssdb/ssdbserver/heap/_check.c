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

/* Check that heap-structure is ok */

#include "heapdef.h"

static int check_one_key(HP_KEYDEF *keydef,ulong records,ulong blength);

int heap_check_heap(info)
HP_INFO *info;
{
  int error;
  uint key;
  DBUG_ENTER("hp_check_keys");

  for (error=key=0 ; key < info->keys ; key++)
    error|=check_one_key(info->keydef+key,info->records,info->blength);

  DBUG_RETURN(error);
}


static int check_one_key(keydef,records,blength)
HP_KEYDEF *keydef;
ulong records,blength;
{
  int error;
  uint i,rec_link,found,max_links,seek,links;
  HASH_INFO *hash_info;

  error=0;
  for (i=found=max_links=seek=0 ; i < records ; i++)
  {
    hash_info=hp_find_hash(&keydef->block,i);
    if (_hp_mask(_hp_rec_hashnr(keydef,hash_info->ptr_to_rec),
		 blength,records) == i)
    {
      found++;
      seek++;
      links=1;
      while ((hash_info=hash_info->next_key) && found < records + 1)
      {
	seek+= ++links;
	if ((rec_link=_hp_mask(_hp_rec_hashnr(keydef,hash_info->ptr_to_rec),
			       blength,records))
	    != i)
	{
	  DBUG_PRINT("error",("Record in wrong link: Link %d  Record: %lx  Record-link %d", i,hash_info->ptr_to_rec,rec_link));
	  error=1;
	}
	else
	  found++;
      }
      if (links > max_links) max_links=links;
    }
  }
  if (found != records)
  {
    DBUG_PRINT("error",("Found %ld of %ld records"));
    error=1;
  }
  DBUG_PRINT("info",
	     ("records: %ld   seeks: %d   max links: %d   hitrate: %.2f",
	      records,seek,max_links,(float) seek / (float) records));
  return error;
}
