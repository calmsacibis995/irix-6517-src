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

/* Functions to read, write and lock records */

#include "mysql_priv.h"

static int rr_quick(READ_RECORD *info);
static int rr_sequental(READ_RECORD *info);
static int rr_from_tempfile(READ_RECORD *info);
static int rr_from_pointers(READ_RECORD *info);
static int rr_from_cache(READ_RECORD *info);
static int init_rr_cache(READ_RECORD *info);
static int rr_cmp(uchar *a,uchar *b);

	/* init struct for read with info->read_record */

void init_read_record(READ_RECORD *info,TABLE *table,SQL_SELECT *select,
		      bool use_record_cache)
{
  IO_CACHE *tempfile;
  DBUG_ENTER("init_read_record");

  bzero((char*) info,sizeof(*info));
  info->form=table;
  info->forms= &info->form;		/* Only one form */
  info->record=table->record[0];
  info->ref_length=table->keyfile_info.ref_length;
  info->select=select;
  table->status=0;			/* And it's allways found */

  if (select && my_b_inited(&select->file))
    tempfile= &select->file;
  else
    tempfile= table->io_cache;
  if (select && select->quick && (! tempfile || !tempfile->buffer))
  {
    DBUG_PRINT("info",("using rr_quick"));
    info->read_record=rr_quick;
  }
  else if (tempfile && tempfile->buffer) /* Test if ref-records was used */
  {
    DBUG_PRINT("info",("using rr_from_tempfile"));
    info->read_record=rr_from_tempfile;
    info->io_cache=tempfile;
    info->io_cache->end_of_file=my_seek(tempfile->file,0L,MY_SEEK_END,MYF(0));
    reinit_io_cache(info->io_cache,READ_CACHE,0L,0);
    info->ref_pos=table->keyfile_info.ref.refpos;

#ifndef _MSC_VER
    if (! (specialflag & SPECIAL_SAFE_MODE) &&
	my_default_record_cache_size &&
	(table->db_stat & HA_READ_ONLY ||
	 table->reginfo.lock_type == TL_READ) &&
	table->reclength*(table->keyfile_info.records+
			     table->keyfile_info.deleted) >
	(ulong) RECORD_CACHE_SIZE * 16 &&
	info->io_cache->end_of_file/info->ref_length*table->reclength >
	(ulong) my_default_record_cache_size*2 &&
	!table->blob_fields)
      if (! init_rr_cache(info))
      {
	DBUG_PRINT("info",("using rr_from_cache"));
	info->read_record=rr_from_cache;
      }
#endif
  }
  else if (table->record_pointers)
  {
    info->cache_pos=table->record_pointers;
    info->cache_end=info->cache_pos+ table->found_records*info->ref_length;
    info->read_record= rr_from_pointers;
  }
  else
  {
    DBUG_PRINT("info",("using rr_sequential"));
    info->read_record=rr_sequental;
    ha_reset(table);
    /* We can use record cache if we don't update dynamic length tables */
    if (use_record_cache ||
	(int) table->reginfo.lock_type <= (int) TL_READ_HIGH_PRIORITY ||
	!(table->db_create_options & HA_OPTION_PACK_RECORD))
      VOID(ha_extra(table,HA_EXTRA_CACHE));	// Cache reads
  }
  DBUG_VOID_RETURN;
} /* init_read_record */


void end_read_record(READ_RECORD *info)
{					/* free cache if used */
  if (info->cache)
  {
    my_free_lock((char*) info->cache,MYF(0));
    info->cache=0;
  }
}

	/* Read a record from head-database */

static int rr_quick(READ_RECORD *info)
{
  return info->select->quick->get_next();
}

static int rr_sequental(READ_RECORD *info)
{
  return (ha_r_rnd(info->form,info->record,info->ref_pos));
}

static int rr_from_tempfile(READ_RECORD *info)
{
  if (my_b_read(info->io_cache,info->ref_pos,info->ref_length))
    return HA_ERR_END_OF_FILE;			/* End of file */
  if (TEST_IF_LASTREF(info->ref_pos,info->ref_length))
    return(HA_ERR_END_OF_FILE);			/* File ends with this */
  return (ha_r_rnd(info->form,info->record,info->ref_pos));
} /* rr_from_tempfile */


static int rr_from_pointers(READ_RECORD *info)
{
  if (info->cache_pos == info->cache_end)
    return HA_ERR_END_OF_FILE;			/* End of file */
  byte *tmp=info->cache_pos;
  info->cache_pos+=info->ref_length;
  return (ha_r_rnd(info->form,info->record,tmp));
}

	/* cacheing of records from a database */

#if !defined(_MSC_VER) || MAX_REFLENGTH != 4

static int init_rr_cache(READ_RECORD *info)
{
  uint rec_cache_size;
  DBUG_ENTER("init_rr_cache");

  info->struct_length=3+info->ref_length;
  info->reclength=ALIGN_SIZE(info->form->reclength+1);
  if (info->reclength < info->struct_length)
    info->reclength=ALIGN_SIZE(info->struct_length);

  info->error_offset=info->form->reclength;
  info->cache_records=my_default_record_cache_size/
    (info->reclength+info->struct_length);
  rec_cache_size=info->cache_records*info->reclength;
  info->rec_cache_size=info->cache_records*info->ref_length;

  if (info->cache_records <= 2 ||
      !(info->cache=my_malloc_lock(rec_cache_size+info->cache_records*
				   info->struct_length,
				   MYF(0))))
    DBUG_RETURN(1);
  DBUG_PRINT("info",("Allocated buffert for %d records",info->cache_records));
  info->read_positions=info->cache+rec_cache_size;
  info->cache_pos=info->cache_end=info->cache;
  DBUG_RETURN(0);
} /* init_rr_cache */


static int rr_from_cache(READ_RECORD *info)
{
  reg1 uint i;
  uint length;
  ulong rest_of_file;
  int16 error;
  byte *position,*ref_position,*record_pos;
  ulong record;

  for (;;)
  {
    if (info->cache_pos != info->cache_end)
    {
      if (info->cache_pos[info->error_offset])
      {
	shortget(error,info->cache_pos);
      }
      else
      {
	error=0;
	memcpy(info->record,info->cache_pos,(size_t) info->form->reclength);
      }
      info->cache_pos+=info->reclength;
      return ((int) error);
    }
    length=info->rec_cache_size;
    rest_of_file=info->io_cache->end_of_file - my_b_tell(info->io_cache);
    if ((ulong) length > rest_of_file)
      length=(uint) rest_of_file;
    if (!length || my_b_read(info->io_cache,info->cache,length))
      return HA_ERR_END_OF_FILE;		/* End of file */

    length/=info->ref_length;
    position=info->cache;
    ref_position=info->read_positions;
    for (i=0 ; i < length ; i++,position+=info->ref_length)
    {
      if (memcmp(position,last_ref,(size_s) info->ref_length) == 0)
      {					/* End of file */
	if (! (length=i))
	  return HA_ERR_END_OF_FILE;	/* Last record and no in buffert */
	break;
      }
      memcpy(ref_position,position,(size_s) info->ref_length);
      ref_position+=info->ref_length;
      int3store(ref_position,(long) i);
      ref_position+=3;
    }
    qsort(info->read_positions,length,info->struct_length,(qsort_cmp) rr_cmp);

    position=info->read_positions;
    for (i=0 ; i < length ; i++)
    {
      memcpy(info->ref_pos,position,(size_s) info->ref_length);
      position+=info->ref_length;
      record=uint3korr(position);
      position+=3;
      record_pos=info->cache+record*info->reclength;
      if ((error=(int16) ha_r_rnd(info->form,record_pos,info->ref_pos)))
      {
	record_pos[info->error_offset]=1;
	shortstore(record_pos,error);
      }
      else
	record_pos[info->error_offset]=0;
    }
    info->cache_end=(info->cache_pos=info->cache)+length*info->reclength;
  }
} /* rr_from_cache */


static int rr_cmp(uchar *a,uchar *b)
{
#if defined(WORDS_BIGENDIAN) && MAX_REFLENGTH == 4
  if (a[0] != b[0])
    return (int) a[0] - (int) b[0];
  if (a[1] != b[1])
    return (int) a[1] - (int) b[1];
  if (a[2] != b[2])
    return (int) a[2] - (int) b[2];
  return (int) a[3] - (int) b[3];
#else
  ulong la,lb;

  longget (la,a);
  longget (lb,b);

  return (la < lb) ? -1 : la > lb ? 1 : 0;
#endif
}

#endif /* _MSC_VER */
