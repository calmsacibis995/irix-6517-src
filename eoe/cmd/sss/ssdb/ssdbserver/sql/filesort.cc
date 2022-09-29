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

/* Sorts a database */

#include "mysql_priv.h"
#ifdef HAVE_STDDEF_H
#include <stddef.h>			/* for macro offsetof */
#endif
#include <m_ctype.h>
#ifndef THREAD
#define SKIPP_DBUG_IN_FILESORT
#endif
	/* static variabels */

#define MERGEBUFF  7
#define MERGEBUFF2 15

	/* How to write record_ref. */

#if !defined(WORDS_BIGENDIAN) && MAX_REFLENGTH == 4
#define WRITE_REF(file,from) \
{ \
  uchar buff[4]; \
  buff[0]= *(from+3); buff[1]= *(from+2); \
  buff[2]= *(from+1); buff[3] =*(from+0); \
  if (my_b_write(file,(byte*) buff,4)) \
    DBUG_RETURN(1); \
}
#else
#define WRITE_REF(file,from) \
if (my_b_write((file),(byte*) (from),param->ref_length)) \
  DBUG_RETURN(1);
#endif

typedef struct st_buffpek {		/* Struktur om sorteringsbuffrarna */
  ha_rows file_pos;			/* Position var bufferten finns */
  ha_rows count;				/* Antal nycklar i bufferten */
  uchar *base,*key;			/* Pekare inom sort_key - indexdel */
  uint mem_count;			/* Antal nycklar kvar i minnet */
  uint max_keys;			/* Max keys in buffert */
} BUFFPEK;


typedef struct st_sort_param {
  uint sort_length;			/* Length of sortarg */
  uint keys;				/* Max antal nycklar / buffert */
  uint ref_length;			/* Length of record ref. */
  ha_rows max_records;
  TABLE *sort_form;			/* For quicker make_sortkey */
  SORT_FIELD *local_sortorder;
  SORT_FIELD *end;
#ifdef USE_STRCOLL
  char* tmp_buffer;
#endif
} SORTPARAM;

	/* functions defined in this file */

static char **make_char_array(register uint fields, uint length, myf my_flag);
static ha_rows find_all_keys(SORTPARAM *param,SQL_SELECT *select,
			     uchar * *sort_keys,
			     BUFFPEK *buffpek,uint *maxbuffer,
			     IO_CACHE *tempfile,IO_CACHE *indexfile);
static int write_keys(SORTPARAM *param,uchar * *sort_keys,
		      uint count,BUFFPEK *buffpek,
		      IO_CACHE *tempfile);
static void make_sortkey(SORTPARAM *param,uchar *to,
			 byte *ref_pos);
static bool save_index(SORTPARAM *param,uchar **sort_keys, uint count);
static int merge_many_buff(SORTPARAM *param,uchar * *sort_keys,
			   BUFFPEK *buffpek,
			   uint *maxbuffer, IO_CACHE *t_file);
static uint read_to_buffer(IO_CACHE *fromfile,BUFFPEK *buffpek,
			   uint sort_length);
static int merge_buffers(SORTPARAM *param,IO_CACHE *from_file,
			 IO_CACHE *to_file,uchar * *sort_keys,
			 BUFFPEK *lastbuff,BUFFPEK *Fb,
			 BUFFPEK *Tb,int flag);
static int merge_index(SORTPARAM *param,uchar * *sort_keys,
		       BUFFPEK *buffpek,
		       uint maxbuffer,IO_CACHE *tempfile,
		       IO_CACHE *outfile);
static uint sortlength(SORT_FIELD *sortorder,uint length);

	/* Makes a indexfil of recordnumbers of a sorted database */
	/* outfile is reset before data is written to it, if it wasn't
	 open a new file is opened */

ha_rows filesort(TABLE **table, SORT_FIELD *sortorder, uint s_length,
		 SQL_SELECT *select, ha_rows special, ha_rows max_records)
{
  int error;
  uint memavl,old_memavl,maxbuffer,skr;
  BUFFPEK *buffpek;
  ha_rows records;
  uchar **sort_keys;
  gptr save_1,save_2;
  IO_CACHE tempfile,*selected_records_file,*outfile;
  SORTPARAM param;
  DBUG_ENTER("filesort");
  DBUG_EXECUTE("info",TEST_filesort(table,sortorder,s_length,special););
#ifdef SKIPP_DBUG_IN_FILESORT
  DBUG_PUSH("");		/* No DBUG here */
#endif

  outfile= table[0]->io_cache;
  my_b_clear(&tempfile);
  save_1=save_2=0;
  buffpek= (BUFFPEK *) NULL; sort_keys= (uchar **) NULL; error= 1;
  maxbuffer=1;
  param.ref_length= table[0]->keyfile_info.ref_length;
  param.sort_length=sortlength(sortorder,s_length)+ param.ref_length;
  param.max_records= max_records;

  if (select && my_b_inited(&select->file))
  {
    records=special=select->records;		/* purecov: deadcode */
    selected_records_file= &select->file;	/* purecov: deadcode */
    reinit_io_cache(selected_records_file,READ_CACHE,0L,0); /* purecov: deadcode */
  }
  else if (special)
  {
    records=special;				/* purecov: deadcode */
    selected_records_file= outfile;		/* purecov: deadcode */
    reinit_io_cache(selected_records_file,READ_CACHE,0L,0); /* purecov: deadcode */
  }
#ifdef CAN_TRUST_RANGE
  else if (select && select->quick && select->quick->records > 0L)
  {
    VOID(ha_info(&table[0]->form,0));	/* Get record-count */
    records=min((ha_rows) (select->quick->records*2+EXTRA_RECORDS*2),
		table[0]->keyfile_info.records)+EXTRA_RECORDS;
    selected_records_file=0;
  }
#endif
  else
  {
    ha_info(table[0],0);		/* Get record-count */
    records=table[0]->keyfile_info.records+EXTRA_RECORDS;
    selected_records_file= 0;
  }
  if (param.sort_length == param.ref_length && records > param.max_records)
    records=param.max_records;			/* purecov: inspected */

#ifdef USE_STRCOLL
  if (!(param.tmp_buffer=my_malloc(param.sort_length,MYF(MY_WME))))
    goto err;
#endif

	/* Reserve memory for IO_CACHE files */
  if (! (save_1=my_malloc(DISK_BUFFER_SIZE,MYF(MY_WME))) ||
      ! (save_2=my_malloc(DISK_BUFFER_SIZE,MYF(MY_WME))))
    goto err;

  memavl=sortbuff_size;
  while (memavl >= MIN_SORT_MEMORY)
  {
    if ((records+1)*(param.sort_length+sizeof(char*))+sizeof(BUFFPEK)*10 <
	(ulong) memavl)
      param.keys=(uint) records+1;
    else
      do
      {
	skr=maxbuffer;
	if (memavl < sizeof(BUFFPEK)*maxbuffer)
	{
	  my_error(ER_OUT_OF_SORTMEMORY,MYF(ME_ERROR+ME_WAITTANG));
	  goto err;
	}
	param.keys=(memavl-sizeof(BUFFPEK)*maxbuffer)/
	  (param.sort_length+sizeof(char*));
      }
      while ((maxbuffer= (uint) (records/param.keys+1)) != skr);

    if ((sort_keys= (uchar **) make_char_array(param.keys,param.sort_length,
					       MYF(0))))
      if ((buffpek = (BUFFPEK*) my_malloc((uint) sizeof(BUFFPEK)*
					  (maxbuffer+10),
					  MYF(0))))
	break;
      else
	my_free((gptr) sort_keys,MYF(0));
    old_memavl=memavl;
    if ((memavl=memavl/4*3) < MIN_SORT_MEMORY && old_memavl > MIN_SORT_MEMORY)
      memavl=MIN_SORT_MEMORY;
  }
  param.keys--;
  maxbuffer+=10;				/* Some extra range */

  if (memavl < MIN_SORT_MEMORY)
  {
    my_error(ER_OUTOFMEMORY,MYF(ME_ERROR+ME_WAITTANG));
    goto err;
  }
  my_free(save_1,MYF(0));		/* Free for later use */
  my_free(save_2,MYF(0));
  save_1=save_2=0;

  param.sort_form= table[0];
  param.end=(param.local_sortorder=sortorder)+s_length;
  if ((records=find_all_keys(&param,select,sort_keys,buffpek,&maxbuffer,
			     &tempfile, selected_records_file)) ==
      HA_POS_ERROR)
    goto err;
  if (maxbuffer == 0)
  {
    if (save_index(&param,sort_keys,(uint) records))
      goto err;
  }
  else
  {
	/* Open cacheed file if it isn't open */
    if (! my_b_inited(outfile) &&
	open_cacheed_file(outfile,mysql_tmpdir,TEMP_PREFIX,READ_RECORD_BUFFER))
      goto err;
    reinit_io_cache(outfile,WRITE_CACHE,0L,0);

    param.keys=((param.keys*(param.sort_length+sizeof(char*))) /
		param.sort_length-1);
    if (merge_many_buff(&param,sort_keys,buffpek,&maxbuffer,&tempfile))
      goto err;
    if (flush_io_cache(&tempfile))
      goto err;
    reinit_io_cache(&tempfile,READ_CACHE,0L,0);
    if (merge_index(&param,sort_keys,buffpek,maxbuffer,&tempfile,outfile))
      goto err;
  }
  if (records > param.max_records)
    records=param.max_records;
  error =0;

 err:
#ifdef USE_STRCOLL
  x_free(param.tmp_buffer);
#endif
  x_free((gptr) sort_keys);
  x_free((gptr) buffpek);
  x_free(save_1);
  x_free(save_2);
  close_cacheed_file(&tempfile);
  if (my_b_inited(outfile))
  {
    if (flush_io_cache(outfile))
      error=1;
    {
      ha_rows save_pos=outfile->pos_in_file;
      reinit_io_cache(outfile,READ_CACHE,0L,0); /* For following reads */
      outfile->end_of_file=save_pos;
    }
  }
  if (error)
    my_error(ER_FILSORT_ABORT,MYF(ME_ERROR+ME_WAITTANG));

#ifdef SKIPP_DBUG_IN_FILESORT
  DBUG_POP();			/* Ok to DBUG */
#endif
  DBUG_PRINT("exit",("records: %ld",records));
  DBUG_RETURN(error ? HA_POS_ERROR : records);
} /* filesort */


	/* Make a array of string pointers */

static char **make_char_array(register uint fields, uint length, myf my_flag)
{
  register char **pos;
  char **old_pos,*char_pos;
  DBUG_ENTER("make_char_array");

  if ((old_pos= (char**) my_malloc((uint) fields*(length+sizeof(char*)),
				    my_flag)))
  {
    pos=old_pos; char_pos=((char*) (pos+fields)) -length;
    while (fields--) *(pos++) = (char_pos+= length);
  }

  DBUG_RETURN(old_pos);
} /* make_char_array */


	/* Search after sort_keys and place them in a temp. file */

static ha_rows find_all_keys(SORTPARAM *param, SQL_SELECT *select,
			     uchar **sort_keys,
			     BUFFPEK *buffpek, uint *maxbuffer,
			     IO_CACHE *tempfile, IO_CACHE *indexfile)
{
  int error,flag,quick_select;
  uint index,indexpos,ref_length;
  uint32 record;
  byte *ref_pos,*next_pos;
  TABLE *sort_form;
  DBUG_ENTER("find_all_keys");

  index=indexpos=0;
  error=quick_select=0;
  record= (ha_rows)  -((int) param->sort_form->db_record_offset);
  sort_form=param->sort_form;
  ref_length=param->ref_length;
  ref_pos= (byte*) &record;
  quick_select=select && select->quick;
  flag= ((!indexfile && sort_form->db_capabilities & HA_REC_NOT_IN_SEQ)
	 || quick_select);
  if (indexfile || flag)
    ref_pos= &sort_form->keyfile_info.ref.refpos[0];
  next_pos=ref_pos;
  if (! indexfile)
  {
    ha_reset(sort_form);
    next_pos=(byte*) 0;			/* Find records in sequence */
  }

  if (! indexfile && ! quick_select)
    VOID(ha_extra(sort_form,HA_EXTRA_CACHE));	/* Quicker reads */

  for (;;)
  {
    if (quick_select)
    {
      if ((error=select->quick->get_next()))
	break;
    }
    else					/* Not quick-select */
    {
      if (indexfile)
      {
	if (my_b_read(indexfile,(byte*) ref_pos,ref_length)) /* purecov: deadcode */
	{
	  error= -1;				/* Abort */
	  break;
	}
	if (TEST_IF_LASTREF(ref_pos,ref_length))
	{
	  error=HA_ERR_END_OF_FILE;
	  break;
	}
      }
      else
	record+=sort_form->db_record_offset;
      if ((error=ha_r_rnd(sort_form,sort_form->record[0],next_pos)) > 0)
	break;
    }
    if (current_thd->killed)
      break;					/* purecov: inspected */
    if (error == 0 && (!select || select->skipp_record() == 0))
    {
      if (index == param->keys)
      {
	if (indexpos >= *maxbuffer ||
	    write_keys(param,sort_keys,index,buffpek+indexpos,tempfile))
	      DBUG_RETURN(HA_POS_ERROR);
	index=0; indexpos++;
	if (param->ref_length == param->sort_length &&
	    my_b_tell(tempfile)/param->sort_length >= param->max_records)
	{
	  error=HA_ERR_END_OF_FILE;
	  break;			/* Found enough records */
	}
      }
      if (flag)
	ha_info(sort_form,1);		/* Get recordpos */
      make_sortkey(param,sort_keys[index++],ref_pos);
    }
  }
  VOID(ha_extra(sort_form,HA_EXTRA_NO_CACHE));	/* End cacheing of records */
  DBUG_PRINT("test",("error: %d  indexpos: %d",error,indexpos));
  if (error <= 0)
    DBUG_RETURN(HA_POS_ERROR);				/* Aborted by user */
  if (error != HA_ERR_END_OF_FILE)
  {
    ha_error(sort_form,error,MYF(ME_ERROR | ME_WAITTANG)); /* purecov: inspected */
    DBUG_RETURN(HA_POS_ERROR);			/* purecov: inspected */
  }
  if (indexpos)
    if (indexpos >= *maxbuffer ||
	write_keys(param,sort_keys,index,buffpek+indexpos,tempfile))
      DBUG_RETURN(HA_POS_ERROR);		/* purecov: inspected */
  *maxbuffer=indexpos;
  DBUG_RETURN(my_b_inited(tempfile) ? my_b_tell(tempfile)/param->sort_length
	      : index);
} /* find_all_keys */


	/* Skriver en buffert med nycklar till filen */

static int write_keys(SORTPARAM *param, register uchar **sort_keys, uint count,
		      BUFFPEK *buffpek, IO_CACHE *tempfile)
{
  uint sort_length;
  DBUG_ENTER("write_keys");

  sort_length=param->sort_length;
#ifdef MC68000
  quicksort(sort_keys,count,sort_length);
#else
  my_string_ptr_sort((gptr) sort_keys,(uint) count,sort_length);
#endif
  if (!my_b_inited(tempfile) &&
      open_cacheed_file(tempfile,mysql_tmpdir,TEMP_PREFIX,DISK_BUFFER_SIZE))
    DBUG_RETURN(1);				/* purecov: inspected */
  buffpek->file_pos=my_b_tell(tempfile);
  if ((ha_rows) count > param->max_records)
    count=(uint) param->max_records;		/* purecov: inspected */
  buffpek->count=(ha_rows) count;
  for (uchar **end=sort_keys+count ; sort_keys != end ; sort_keys++)
    if (my_b_write(tempfile,(byte*) *sort_keys,(uint) sort_length))
      DBUG_RETURN(1);
  DBUG_RETURN(0);
} /* write_keys */


	/* makes a sort-key from record */

static void make_sortkey(register SORTPARAM *param,
			 register uchar *to, byte *ref_pos)
{
  reg3 Field *field;
  reg1 SORT_FIELD *sort_field;
  reg5 uint length;

  for (sort_field=param->local_sortorder ;
       sort_field != param->end ;
       sort_field++)
  {
    if ((field=sort_field->field))
    {						// Field
      if (field->maybe_null())
      {
	if (field->is_null())
	{
	  if (sort_field->reverse)
	    bfill(to,sort_field->length+1,(char) 255);
	  else
	    bzero((char*) to,sort_field->length+1);
	  to+= sort_field->length+1;
	  continue;
	}
	else
	  *to++=1;
      }
      field->sort_string((char*) to,sort_field->length);
    }
    else
    {						// Item
      Item *item=sort_field->item;
      switch (sort_field->result_type) {
      case STRING_RESULT:
	{
	  if (item->maybe_null)
	    *to++=1;
	  /* All item->str() to use some extra byte for end null.. */
	  String tmp((char*) to,sort_field->length+4);
	  String *res=item->str(&tmp);
	  if (!res)
	  {
	    if (item->maybe_null)
	      bzero(to-1,sort_field->length+1);
	    else
	    {
	      DBUG_PRINT("warning",
			 ("Got null on something that shouldn't be null"));
	      bzero(to,sort_field->length);	// Avoid crash
	    }
	    break;
	  }
	  length=res->length();
	  int diff=(int) (sort_field->length-length);
	  if (diff < 0)
	    diff=0;				/* purecov: inspected */
#ifndef USE_STRCOLL
	  if (res->ptr() != (char*) to)
	    memcpy(to,res->ptr(),length);
	  bzero(to+length,diff);
	  if (!item->binary)
	    case_sort((char*) to,length);
#else
	  if (item->binary)
	  {
	    if (res->ptr() != (char*) to)
	      memcpy(to,res->ptr(),length);
	    bzero(to+length,diff);
	  }
	  else
	  {
	    char *from=(char*) res->ptr();
	    if ((unsigned char *)from == to)
	    {
	      set_if_smaller(length,sort_field->length);
	      memcpy(param->tmp_buffer,from,length);
	      from=param->tmp_buffer;
	    }
	    uint tmp_length=my_strnxfrm(to,(unsigned char *) from,
					sort_field->length,
					length);
	    if (tmp_length < sort_field->length)
	      bzero(to+tmp_length,sort_field->length-tmp_length);
	  }
#endif
	  break;
	}
      case INT_RESULT:
	{
	  longlong value=item->val_int();
	  if (item->maybe_null)
	    *to++=1;				/* purecov: inspected */
	  if (item->null_value)
	  {
	    if (item->maybe_null)
	      bzero(to-1,sort_field->length+1);
	    else
	    {
	      DBUG_PRINT("warning",
			 ("Got null on something that shouldn't be null"));
	      bzero(to,sort_field->length);
	    }
	    break;
	  }
#if SIZEOF_LONG_LONG > 4
	  to[7]= (uchar) value;
	  to[6]= (uchar) (value >> 8);
	  to[5]= (uchar) (value >> 16);
	  to[4]= (uchar) (value >> 24);
	  to[3]= (uchar) (value >> 32);
	  to[2]= (uchar) (value >> 40);
	  to[1]= (uchar) (value >> 48);
	  to[0]= (uchar) (value >> 56) ^ 128;	// Fix sign
#else
	  to[3]= (uchar) value;
	  to[2]= (uchar) (value >> 8);
	  to[1]= (uchar) (value >> 16);
	  to[0]= (uchar) (value >> 24) ^ 128;	// Fix sign
#endif
	  break;
	}
      case REAL_RESULT:
	{
	  double value=item->val();
	  if (item->null_value)
	  {
	    bzero(to,sort_field->length+1);
	    to++;
	    break;
	  }
	  if (item->maybe_null)
	    *to++=1;
	  change_double_for_sort(value,(byte*) to);
	  break;
	}
      }
    }
    if (sort_field->reverse)
    {							/* Revers key */
      length=sort_field->length;
      while (length--)
      {
	*to = (uchar) (~ *to);
	to++;
      }
    }
    else
      to+= sort_field->length;
  }

/* simple test to probably get the records in inserted file order */
#if !defined(WORDS_BIGENDIAN) && MAX_REFLENGTH == 4
  to[0] = (uchar) ref_pos[3];
  to[1] = (uchar) ref_pos[2];
  to[2] = (uchar) ref_pos[1];
  to[3] = (uchar) ref_pos[0];
#else
  memcpy((byte*) to,ref_pos,(size_s) param->ref_length);/* Save filepos last */
#endif
  return;
}


static bool save_index(SORTPARAM *param, uchar **sort_keys, uint count)
{
  uint offset,ref_length;
  byte *to;
  DBUG_ENTER("save_index");

  my_string_ptr_sort((gptr) sort_keys,(uint) count,param->sort_length);
  ref_length=param->ref_length;
  offset=param->sort_length-ref_length;
  if ((ha_rows) count > param->max_records)
    count=(uint) param->max_records;
  if (!(to=param->sort_form->record_pointers=
	(byte*) my_malloc(ref_length*count,MYF(MY_WME))))
    DBUG_RETURN(1);				/* purecov: inspected */
  for (uchar **end=sort_keys+count ; sort_keys != end ; sort_keys++)
  {
#if !defined(WORDS_BIGENDIAN) && MAX_REFLENGTH == 4
    to[0]= *(*sort_keys+offset+3); to[1]= *(*sort_keys+offset+2);
    to[2]= *(*sort_keys+offset+1); to[3] =*(*sort_keys+offset+0);
#else
    memcpy(to,*sort_keys+offset,ref_length);
#endif
    to+=ref_length;
  }
  DBUG_RETURN(0);
}


	/* Merge buffers to make < MERGEBUFF2 buffers */

static int merge_many_buff(SORTPARAM *param, uchar **sort_keys,
			   BUFFPEK *buffpek, uint *maxbuffer, IO_CACHE *t_file)
{
  register int i;
  IO_CACHE t_file2,*from_file,*to_file,*temp;
  BUFFPEK *lastbuff;
  DBUG_ENTER("merge_many_buff");

  if (*maxbuffer < MERGEBUFF2)
    DBUG_RETURN(0);				/* purecov: inspected */
  if (flush_io_cache(t_file) ||
      open_cacheed_file(&t_file2,mysql_tmpdir,TEMP_PREFIX,DISK_BUFFER_SIZE))
    DBUG_RETURN(1);				/* purecov: inspected */

  from_file= t_file ; to_file= &t_file2;
  while (*maxbuffer >= MERGEBUFF2)
  {
    reinit_io_cache(from_file,READ_CACHE,0L,0);
    reinit_io_cache(to_file,WRITE_CACHE,0L,0);
    lastbuff=buffpek;
    for (i=0 ; i <= (int) *maxbuffer-MERGEBUFF*3/2 ; i+=MERGEBUFF)
    {
      if (merge_buffers(param,from_file,to_file,sort_keys,lastbuff++,
			buffpek+i,buffpek+i+MERGEBUFF-1,0))
	break;					/* purecov: inspected */
    }
    if (merge_buffers(param,from_file,to_file,sort_keys,lastbuff++,
		      buffpek+i,buffpek+ *maxbuffer,0))
      break;					/* purecov: inspected */
    if (flush_io_cache(to_file))
      break;					/* purecov: inspected */
    temp=from_file; from_file=to_file; to_file=temp;
    *maxbuffer= (uint) (lastbuff-buffpek)-1;
  }
  close_cacheed_file(to_file);			// This holds old result
  if (to_file == t_file)
    memcpy(t_file,&t_file2,sizeof(t_file2));	// Copy result file

  DBUG_RETURN(*maxbuffer >= MERGEBUFF2);	/* Return 1 if interrupted */
} /* merge_many_buff */


	/* Read data to buffer */
	/* This returns (uint) -1 if something goes wrong */

static uint read_to_buffer(IO_CACHE *fromfile, BUFFPEK *buffpek,
			   uint sort_length)
{
  register uint count;
  uint length;

  if ((count=(uint) min((ha_rows) buffpek->max_keys,buffpek->count)))
  {
    if (my_pread(fromfile->file,(byte*) buffpek->base,
		 (length= sort_length*count),buffpek->file_pos,MYF_RW))
      return((uint) -1);			/* purecov: inspected */
    buffpek->key=buffpek->base;
    buffpek->file_pos+= length;			/* New filepos */
    buffpek->count-=	count;
    buffpek->mem_count= count;
  }
  return (count*sort_length);
} /* read_to_buffer */


	/* Merge buffers to one buffer */

static int merge_buffers(SORTPARAM *param, IO_CACHE *from_file,
				IO_CACHE *to_file, uchar **sort_keys,
				BUFFPEK *lastbuff, BUFFPEK *Fb, BUFFPEK *Tb,
				int flag)
{
  int error;
  uint sort_length,offset,maxcount;
  ha_rows count,to_start_filepos,max_records;
  uchar *strpos;
  BUFFPEK *buffpek,**refpek;
  QUEUE queue;
  DBUG_ENTER("merge_buffers");

  count=error=0;
  offset=param->sort_length-param->ref_length;
  maxcount=param->keys/((uint) (Tb-Fb) +1);
  to_start_filepos=my_b_tell(to_file);
  strpos=(uchar*) sort_keys;
  sort_length=param->sort_length;
  max_records=param->max_records;

  if (init_queue(&queue,(uint) (Tb-Fb)+1,offsetof(BUFFPEK,key),0,
		 (int (*) (byte *,byte*))
		 get_ptr_compare(sort_length)))
    DBUG_RETURN(1);				/* purecov: inspected */
  for (buffpek= Fb ; buffpek <= Tb ; buffpek++)
  {
    count+= buffpek->count;
    buffpek->base= strpos;
    buffpek->max_keys=maxcount;
    strpos+= (uint) (error=(int) read_to_buffer(from_file,buffpek,
						sort_length));
    if (error == -1)
      goto err;					/* purecov: inspected */
    queue_insert(&queue,(byte*) buffpek);
  }

  while (queue.elements > 1)
  {
    if (current_thd->killed)
    {
      error=1; goto err;			/* purecov: inspected */
    }
    for (;;)
    {
      buffpek=(BUFFPEK*) queue_top(&queue);
      if (flag == 0)
      {
	if (my_b_write(to_file,(byte*) buffpek->key, sort_length))
	{
	  error=1; goto err;			/* purecov: inspected */
	}
      }
      else
      {
	WRITE_REF(to_file,(byte*) buffpek->key+offset);
      }
      if (!--max_records)
      {
	error=0;				/* purecov: inspected */
	goto end;				/* purecov: inspected */
      }
      buffpek->key+=sort_length;
      if (! --buffpek->mem_count)
      {
	if (!(error=(int) read_to_buffer(from_file,buffpek,
					 sort_length)))
	{
	  uchar *base=buffpek->base;
	  uint max_keys=buffpek->max_keys;

	  VOID(queue_remove(&queue,0));

	  /* Put room used by buffer to use in other buffer */
	  for (refpek= (BUFFPEK**) &queue_top(&queue);
	       refpek <= (BUFFPEK**) &queue_end(&queue);
	       refpek++)
	  {
	    buffpek= *refpek;
	    if (buffpek->base+buffpek->max_keys*sort_length == base)
	    {
	      buffpek->max_keys+=max_keys;
	      break;
	    }
	    else if (base+max_keys*sort_length == buffpek->base)
	    {
	      buffpek->base=base;
	      buffpek->max_keys+=max_keys;
	      break;
	    }
	  }
	  break;			/* One buffer have been removed */
	}
	else if (error == -1)
	  goto err;				/* purecov: inspected */
      }
      queue_replaced(&queue);		/* Top element has been replaced */
    }
  }
  buffpek=(BUFFPEK*) queue_top(&queue);
  buffpek->base=(uchar *) sort_keys;
  buffpek->max_keys=param->keys;
  do
  {
    if ((ha_rows) buffpek->mem_count > max_records)
    {					/* Don't write too many records */
      buffpek->mem_count=(uint) max_records;
      buffpek->count=0;			/* Don't read more */
    }
    if (flag == 0)
    {
      if (my_b_write(to_file,(byte*) buffpek->key,
		     (sort_length*buffpek->mem_count)))
      {
	error=1; goto err;			/* purecov: inspected */
      }
    }
    else
    {
      register uchar *end;
      strpos= buffpek->key+offset;
      for (end=strpos+buffpek->mem_count*sort_length;
	   strpos != end ;
	   strpos+=sort_length)
      {
	WRITE_REF(to_file,strpos);
      }
    }
  }
  while ((error=(int) read_to_buffer(from_file,buffpek,sort_length))
	 != -1 && error != 0);

end:
  lastbuff->count=min(count,param->max_records);
  lastbuff->file_pos=to_start_filepos;
err:
  delete_queue(&queue);
  DBUG_RETURN(error);
} /* merge_buffers */


	/* Do a merge to output-file (save only positions) */

static int merge_index(SORTPARAM *param, uchar **sort_keys,
		       BUFFPEK *buffpek, uint maxbuffer,
		       IO_CACHE *tempfile, IO_CACHE *outfile)
{
  DBUG_ENTER("merge_index");
  if (merge_buffers(param,tempfile,outfile,sort_keys,buffpek,buffpek,
		    buffpek+maxbuffer,1))
    DBUG_RETURN(1);				/* purecov: inspected */
  DBUG_RETURN(my_b_write(outfile,last_ref,param->ref_length));
} /* merge_index */


	/* Calculate length of sort key */

static uint
sortlength(SORT_FIELD *sortorder, uint s_length)
{
  reg2 uint length;

  length=0;
  for (; s_length-- ; sortorder++)
  {
    if (sortorder->field)
    {
      if (sortorder->field->type() == FIELD_TYPE_BLOB)
	sortorder->length=max_item_sort_length;
      else
      {
	sortorder->length=sortorder->field->pack_length();
#ifdef USE_STRCOLL
	if (!sortorder->field->binary())
	  sortorder->length= sortorder->length*MY_STRXFRM_MULTIPLY;
#endif
      }
      if (sortorder->field->maybe_null())
	length++;				// Place for NULL marker
    }
    else
    {
      switch ((sortorder->result_type=sortorder->item->result_type())) {
      case STRING_RESULT:
	sortorder->length=sortorder->item->max_length;
#ifdef USE_STRCOLL
	if (!sortorder->item->binary)
	  sortorder->length= sortorder->length*MY_STRXFRM_MULTIPLY;
#endif
	break;
      case INT_RESULT:
#if SIZEOF_LONG_LONG > 4
	sortorder->length=8;			// Size of intern longlong
#else
	sortorder->length=4;
#endif
	break;
      case REAL_RESULT:
	sortorder->length=sizeof(double);
	break;
      }
      if (sortorder->item->maybe_null)
	length++;				// Place for NULL marker
    }
    set_if_smaller(sortorder->length,max_item_sort_length);
    length+=sortorder->length;
  }
  sortorder->field= (Field*) 0;			// end marker
  DBUG_PRINT("info",("sort_length: %d",length));
  return length;
}


/*
** functions to change a double or float to a sortable string
** The following should work for IEEE
*/

#define DBL_EXP_DIG (sizeof(double)*8-DBL_MANT_DIG)

void change_double_for_sort(double nr,byte *to)
{
  uchar *tmp=(uchar*) to;
  if (nr == 0.0)
  {						/* Change to zero string */
    tmp[0]=(uchar) 128;
    bzero((char*) tmp+1,sizeof(nr)-1);
  }
  else
  {
#ifdef WORDS_BIGENDIAN
    memcpy(tmp,&nr,sizeof(nr));
#else
    {
      uchar *ptr= (uchar*) &nr;
      tmp[0]= ptr[7]; tmp[1]=ptr[6]; tmp[2]= ptr[5]; tmp[3]=ptr[4];
      tmp[4]= ptr[3]; tmp[5]=ptr[2]; tmp[6]= ptr[1]; tmp[7]=ptr[0];
    }
#endif
    if (tmp[0] & 128)				/* Negative */
    {						/* make complement */
      uint i;
      for (i=0 ; i < sizeof(nr); i++)
	tmp[i]=tmp[i] ^ (uchar) 255;
    }
    else
    {					/* Set high and move exponent one up */
      ushort exp=((ushort) tmp[0] << 8) | (ushort) tmp[1] | (ushort) 32768;
      exp+= (ushort) 1 << (16-1-DBL_EXP_DIG);
      tmp[0]= (uchar) (exp >> 8);
      tmp[1]= (uchar) exp;
    }
  }
}
