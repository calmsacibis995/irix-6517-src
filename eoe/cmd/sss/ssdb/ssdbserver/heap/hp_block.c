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

/* functions on blocks; Keys and records are saved in blocks */

#include "heapdef.h"

	/* Find record according to record-position */

byte *_hp_find_block(block,pos)
HP_BLOCK *block;
ulong pos;
{
  reg1 int i;
  reg3 HP_PTRS *ptr;

  for (i=block->levels-1, ptr=block->root ; i > 0 ; i--)
  {
    ptr=(HP_PTRS*)ptr->blocks[pos/block->level_info[i].records_under_level];
    pos%=block->level_info[i].records_under_level;
  }
  return (byte*) ptr+ pos*block->recbuffer;
}


	/* get one new block-of-records. Alloc ptr to block if neaded */
	/* Interrupts are stopped to allow ha_panic in interrupts */

int _hp_get_new_block(block)
HP_BLOCK *block;
{
  reg1 int i,j;
  HP_PTRS *root;

  for (i=0 ; (uint) i < block->levels ; i++)
    if (block->level_info[i].free_ptrs_in_block)
      break;

  if (!(root=(HP_PTRS*) my_malloc(sizeof(HP_PTRS)*i+block->records_in_block*
				  block->recbuffer,MYF(0))))
    return 1;

  if (i == 0)
  {
    block->levels=1;
    block->root=block->level_info[0].last_blocks=root;
  }
  else
  {
    dont_break();		/* Dont allow SIGHUP or SIGINT */
    if ((uint) i == block->levels)
    {
      block->levels=i+1;
      block->level_info[i].free_ptrs_in_block=HP_PTRS_IN_NOD-1;
      ((HP_PTRS**) root)[0]= block->root;
      block->root=block->level_info[i].last_blocks= root++;
    }
    block->level_info[i].last_blocks->
      blocks[HP_PTRS_IN_NOD - block->level_info[i].free_ptrs_in_block--]=
	(byte*) root;

    for (j=i-1 ; j >0 ; j--)
    {
      block->level_info[j].last_blocks= root++;
      block->level_info[j].last_blocks->blocks[0]=(byte*) root;
      block->level_info[j].free_ptrs_in_block=HP_PTRS_IN_NOD-1;
    }
    block->level_info[0].last_blocks= root;
    allow_break();		/* Allow SIGHUP & SIGINT */
  }
  return 0;
}


	/* free all blocks under level */

byte *_hp_free_level(block,level,pos,last_pos)
HP_BLOCK *block;
uint level;
HP_PTRS *pos;
byte *last_pos;
{
  int i,max_pos;
  byte *next_ptr;

  if (level == 1)
    next_ptr=(byte*) pos+block->recbuffer;
  else
  {
    max_pos= (block->level_info[level-1].last_blocks == pos) ?
      HP_PTRS_IN_NOD - block->level_info[level-1].free_ptrs_in_block :
    HP_PTRS_IN_NOD;

    next_ptr=(byte*) (pos+1);
    for (i=0 ; i < max_pos ; i++)
      next_ptr=_hp_free_level(block,level-1,
			      (HP_PTRS*) pos->blocks[i],next_ptr);
  }
  if ((byte*) pos != last_pos)
  {
    my_free((gptr) pos,MYF(0));
    return last_pos;
  }
  return next_ptr;			/* next memory position */
}
