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

/* Handler-calling-functions */

#include "mysql_priv.h"
#include <errno.h>
#include <nisam.h>
#ifndef NO_MISAM
#include <misam.h>
#else
#define is_open 0				/* Undefine for initarray */
#define is_close 0
#define is_write 0
#define is_update 0
#define is_delete 0
#define is_rkey 0
#define is_rnext 0
#define is_rprev 0
#define is_rfirst 0
#define is_rlast 0
#define is_rsame 0
#define is_rrnd 0
#define is_lock_database 0
#define is_extra 0
#define I_MAXKEY_SEG 5
#endif
#ifndef NO_HEAP
#include <heap.h>
#else
#define heap_open 0				/* Undefine for initarray */
#define heap_close 0
#define heap_write 0
#define heap_update 0
#define heap_delete 0
#define heap_rsame 0
#define heap_rrnd 0
#define heap_extra 0
#define heap_rnext 0
#define heap_rprev 0
#define heap_rfirst 0
#define heap_rlast 0
#define heap_rkey 0
#endif
#ifndef NO_HASH
#include <hashdb.h>
#else
#define h_open 0				/* Undefine for initarray */
#define h_close 0
#define h_write 0
#define h_update 0
#define h_delete 0
#define h_rkey 0
#define h_rnext 0
#define h_rprev 0
#define h_rfirst 0
#define h_rlast 0
#define h_rsame 0
#define h_rrnd 0
#define h_lock_database 0
#define h_extra 0
#endif
#ifndef NO_PISAM
#include <pisam.h>
#else
#define ps_open 0				/* Undefine for initarray */
#define ps_close 0
#define ps_write 0
#define ps_update 0
#define ps_delete 0
#define ps_rkey 0
#define ps_rnext 0
#define ps_rprev 0
#define ps_rfirst 0
#define ps_rlast 0
#define ps_rsame 0
#define ps_rrnd 0
#define ps_lock_database 0
#define ps_extra 0
#endif
#ifdef HAVE_RMS_ISAM
#include <rms_isam.h>
#else
#define rms_open 0				/* Undefine for initarray */
#define rms_close 0
#define rms_write 0
#define rms_update 0
#define rms_delete 0
#define rms_rkey 0
#define rms_rnext 0
#define rms_rprev 0
#define rms_rfirst 0
#define rms_rlast 0
#define rms_rsame 0
#define rms_rrnd 0
#define rms_lock_database 0
#define rms_extra 0
#endif
#ifndef NO_MERGE
#include <merge.h>
#else
#define mrg_open 0				/* Undefine for initarray */
#define mrg_close 0
#define mrg_write 0
#define mrg_update 0
#define mrg_delete 0
#define mrg_rkey 0
#define mrg_rnext 0
#define mrg_rprev 0
#define mrg_rfirst 0
#define mrg_rlast 0
#define mrg_rsame 0
#define mrg_rrnd 0
#define mrg_lock_database 0
#define mrg_extra 0
#endif


	/* static functions defined in this file */

static int NEAR_F _ha_open(TABLE *form, my_string name,int mode,
			   int test_if_locked);
static void NEAR_F set_form_timestamp(byte *record);
static int ha_err(void);
static int ha_ok(void);
static void NEAR_F update_next_number(TABLE *form);

ulong ha_read_count, ha_write_count, ha_delete_count, ha_update_count,
      ha_read_key_count, ha_read_first_count, ha_read_next_count, ha_read_rnd_count;

	/* Index to functions in different handlers */

typedef int (*F) ();

static	int (*funcs[][7])() = {
  { (F) h_open,		(F) is_open,		(F) ps_open,
    (F) rms_open,	(F) heap_open,		(F) ni_open,
    (F) mrg_open },
  { (F) h_close,	(F) is_close,		(F) ps_close,
    (F) rms_close,	(F) heap_close,		(F) ni_close,
    (F) mrg_close },
  { (F) h_write,	(F) is_write,		(F) ps_write,
    (F) rms_write,	(F) heap_write,		(F) ni_write,
    (F) ha_err },
  { (F) h_update,	(F) is_update,		(F) ps_update,
    (F) rms_update,	(F) heap_update,	(F) ni_update,
    (F) mrg_update },
  { (F) h_delete,	(F) is_delete,		(F) ps_delete,
    (F) rms_delete,	(F) heap_delete,	(F) ni_delete,
    (F) mrg_delete },
  { (F) h_rkey,		(F) is_rkey,		(F) ps_rkey,
    (F) rms_rkey,	(F) heap_rkey,		(F) ni_rkey,
    (F) ha_err },
  { (F) h_rnext,	(F) is_rnext,		(F) ps_rnext,
    (F) rms_rnext,	(F) heap_rnext,		(F) ni_rnext,
    (F) ha_err },
  { (F) h_rprev,	(F) is_rprev,		(F) ps_rprev,
    (F) rms_rprev,	(F) heap_rprev,		(F) ni_rprev,
    (F) ha_err },
  { (F) h_rfirst,	(F) is_rfirst,		(F) ps_rfirst,
    (F) rms_rfirst,	(F) heap_rfirst,	(F) ni_rfirst,
    (F) ha_err },
  { (F) h_rlast,	(F) is_rlast,		(F) ps_rlast,
    (F) rms_rlast,	(F) heap_rlast,		(F) ni_rlast,
    (F) ha_err },
#ifdef NOT_NEEDED
  { (F) h_rsame,	(F) is_rsame,		(F) ps_rsame,
    (F) rms_rsame,	(F) heap_rsame,		(F) ni_rsame,
    (F) mrg_rsame },
#else
  { (F) 0,		(F) 0,			(F) 0,
    (F) 0,		(F) 0,			(F) 0,
    (F) 0 },
#endif
  { (F) h_rrnd,		(F) is_rrnd,		(F) ps_rrnd,
    (F) rms_rrnd,	(F) heap_rrnd,		(F) ni_rrnd,
    (F) mrg_rrnd },
  { (F) h_lock_database,(F) is_lock_database,	(F) ps_lock_database,
    (F) rms_lock_database,(F) ha_ok,		(F) ni_lock_database,
    (F) mrg_lock_database },
  { (F) h_extra,	(F) is_extra,		(F) ps_extra,
    (F) rms_extra,	(F) heap_extra,		(F) ni_extra,
    (F) mrg_extra },
};

uint NEAR ha_option_flag[] = {
  0,
  /* DIAB_ISAM */
  HA_READ_NEXT+HA_READ_PREV+HA_READ_ORDER+HA_RSAME_NO_INDEX,
  /* HASH */
  HA_READ_NEXT+HA_READ_PREV+HA_READ_RND_SAME+HA_KEYPOS_TO_RNDPOS+
  HA_BINARY_KEYS+HA_WRONG_ASCII_ORDER,
    /* M-ISAM */
  HA_READ_NEXT+HA_READ_PREV+HA_READ_RND_SAME+HA_KEYPOS_TO_RNDPOS+
  HA_READ_ORDER+HA_LASTKEY_ORDER+HA_HAVE_KEYREAD_ONLY+HA_READ_NOT_EXACT_KEY,
    /* P-ISAM */
  HA_READ_NEXT+HA_READ_PREV+HA_READ_RND_SAME+HA_KEYPOS_TO_RNDPOS+
  HA_READ_ORDER+HA_LASTKEY_ORDER+HA_REC_NOT_IN_SEQ+HA_HAVE_KEYREAD_ONLY+
  HA_READ_NOT_EXACT_KEY,
    /* RMS-ISAM */
  HA_READ_NEXT+HA_READ_RND_SAME+HA_KEYPOS_TO_RNDPOS+
  HA_READ_ORDER+HA_LASTKEY_ORDER+HA_REC_NOT_IN_SEQ+HA_RSAME_NO_INDEX+
  HA_WRONG_ASCII_ORDER+HA_READ_NOT_EXACT_KEY,
    /* HEAP */
  HA_READ_RND_SAME+HA_NO_INDEX+HA_BINARY_KEYS+HA_WRONG_ASCII_ORDER+
  HA_KEYPOS_TO_RNDPOS,
    /* ISAM */
  HA_READ_NEXT+HA_READ_PREV+HA_READ_RND_SAME+HA_KEYPOS_TO_RNDPOS+
  HA_READ_ORDER+HA_LASTKEY_ORDER+HA_HAVE_KEYREAD_ONLY+HA_READ_NOT_EXACT_KEY+
  HA_LONGLONG_KEYS+HA_KEYREAD_WRONG_STR,
    /* MERGE-ISAM */
  HA_READ_RND_SAME+HA_KEYPOS_TO_RNDPOS,
    /* MY_ISAM */
  HA_READ_NEXT+HA_READ_PREV+HA_READ_RND_SAME+HA_KEYPOS_TO_RNDPOS+
  HA_READ_ORDER+HA_LASTKEY_ORDER+HA_HAVE_KEYREAD_ONLY+HA_READ_NOT_EXACT_KEY+
  HA_LONGLONG_KEYS+HA_NULL_KEY,
};

#ifndef MI_MAXKEY
#define MI_MAXKEY 0
#define MI_MAXKEY_SEG 0
#define MI_MAX_KEY_LENGTH 0
#endif

uint NEAR ha_maxrecordlength[]=
{ 0,1024,32767,32767,65535,65535,65535,65535,65535,65535};
uint NEAR ha_max_keys[]=
{ 0,MAX_KEY,MAX_KEY,MAX_KEY,MAX_KEY,MAX_KEY,16,N_MAXKEY,0,MI_MAXKEY };
uint NEAR ha_max_key_parts[]=
{ 0,5,5,I_MAXKEY_SEG,5,5,16,N_MAXKEY_SEG,0,MI_MAXKEY_SEG};
uint NEAR ha_max_key_length[]=
{ N_MAX_KEY_LENGTH,N_MAX_KEY_LENGTH,N_MAX_KEY_LENGTH,N_MAX_KEY_LENGTH,
  N_MAX_KEY_LENGTH,N_MAX_KEY_LENGTH,65535,N_MAX_KEY_LENGTH,
  N_MAX_KEY_LENGTH,MI_MAX_KEY_LENGTH};

char NEAR bas_ext[][2][FN_EXTLEN] = {
  { "","" },
  { ".DAT",".ISM"},				/* ISAM extensions */
  { ".DAT",".HSH"},				/* HASH extensions */
  { ".ISD",".ISM"},				/* M-ISAM extensions */
  { ".PSD",".PSM"},				/* P-ISAM extensions */
  { ".DAT",""},					/* RMS-ISAM extensions */
  { "",""},					/* HEAP extensions */
  { ".ISD",".ISM"},
  { ".MRG",""},
  { ".MYD",".MYM"},
};


	/* Open database-handler. Try O_RDONLY if can't open as O_RDWR */
	/* Don't wait for locks if not HA_OPEN_WAIT_IF_LOCKED is set */

int ha_open(TABLE *form, my_string name, int mode, int test_if_locked)
{
  int error;
  DBUG_ENTER("ha_open");
  DBUG_PRINT("enter",("db_type: %d  db_stat: %d  mode: %d  lock_test: %d",
		      form->db_type, form->db_stat, mode, test_if_locked));

  form->file=0 ;
  form->db_capabilities=ha_option_flag[form->db_type];
  if ((error=_ha_open(form,name,mode,test_if_locked)))
  {
    if ((error == EACCES || error == EROFS) && mode == O_RDWR &&
	(form->db_stat & HA_TRY_READ_ONLY))
    {
      form->db_stat|=HA_READ_ONLY;
      if (!(error=_ha_open(form,name,O_RDONLY,test_if_locked)) &&
	  errmsg[ERRMAPP])
	my_error(ER_OPEN_AS_READONLY,ME_BELL | ME_WAITTOT | ME_COLOUR1,name);
    }
  }
  if (error)
  {
    my_errno=error;			/* Safeguard */
    DBUG_PRINT("error",("error: %d  errno: %d",error,errno));
  }
  DBUG_RETURN (error);
} /* ha_open */


	/* sub_function to ha_open */
	/* Name will be changed !! */

static int NEAR_F _ha_open(register TABLE *form, my_string name, int mode,
			   int test_if_locked)
{
  char name_buff[FN_REFLEN];
  form->keyfile_info.ref_length=sizeof(int32);	/* Length of ref */
  form->locked=F_UNLCK;				/* Not locked */
  switch (form->db_type) {
#ifndef NO_HEAP
  case DB_TYPE_HEAP:
    {
      if (form->db_stat & (HA_OPEN_KEYFILE+HA_OPEN_RNDFILE+HA_GET_INFO))
      {
	uint key,part,parts;
	HP_KEYDEF *keydef;
	HP_KEYSEG *seg;

	for (key=parts=0 ; key < form->keys ; key++)
	  parts+=form->key_info[key].key_parts;

	if (!(keydef=(HP_KEYDEF*) my_alloca(form->keys*sizeof(HP_KEYDEF)+
					    parts*sizeof(HP_KEYSEG))))
	  return my_errno;
	seg=(HP_KEYSEG*) (keydef+form->keys);
	for (key=0 ; key < form->keys ; key++)
	{
	  KEY *pos=form->key_info+key;

	  keydef[key].keysegs=(uint) pos->key_parts;
	  keydef[key].flag = pos->dupp_key ? 0 : HA_NOSAME;
	  keydef[key].seg=seg;

	  for (part=0 ; part < pos->key_parts ; part++)
	  {
	    uint flag=pos->key_part[part].key_type;
	    if (!f_is_packed(flag) &&
		f_packtype(flag) == (int) FIELD_TYPE_DECIMAL &&
		!(flag & FIELDFLAG_BINARY))
	      seg->type= (int) HA_KEYTYPE_TEXT;
	    else
	      seg->type= (int) HA_KEYTYPE_BINARY;
	    seg->start=(uint) pos->key_part[part].offset;
	    seg->length=(uint) pos->key_part[part].length;
	    seg++;
	  }
	}
	form->file=(byte*) heap_open(fn_format(name_buff,name,"","",1+2),mode,
				     form->keys,keydef,
				     form->reclength,form->max_records,
				     form->reloc);
	my_afree((gptr) keydef);
	ha_info(form,2);
	return (!form->file ? errno : 0);
      }
    }
    break;
#endif
  case DB_TYPE_MRG_ISAM:
    if (form->db_stat == HA_GET_INFO)
    {						/* Skipp if not open */
      form->db_stat=0;				/* because .MRG may be wrong */
      break;
    }
    /* fall through */
  case DB_TYPE_MISAM:
  case DB_TYPE_HASH:
  case DB_TYPE_PISAM:
  case DB_TYPE_ISAM:
  case DB_TYPE_RMS_ISAM:
    {
      if (form->db_stat & (HA_OPEN_KEYFILE+HA_OPEN_RNDFILE+HA_GET_INFO))
      {
	if (!(form->file= (* ((byte *(*)(char *,int,int))
			      funcs[0][(int) form->db_type-2]))
	      (fn_format(name_buff,name,"","",2 | 4),mode,test_if_locked)))
	  return(my_errno ? my_errno : -1);
	if (!(test_if_locked == HA_OPEN_WAIT_IF_LOCKED ||
	      test_if_locked == HA_OPEN_ABORT_IF_LOCKED))
	  VOID(ha_extra(form,HA_EXTRA_NO_WAIT_LOCK));
	ha_info(form,2);
	if (!(test_if_locked & HA_OPEN_WAIT_IF_LOCKED))
	  VOID(ha_extra(form,HA_EXTRA_WAIT_LOCK));
	if (form->db_create_options &
	    (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD))
	  form->db_capabilities|=HA_REC_NOT_IN_SEQ;
	if (form->db_create_options & HA_OPTION_READ_ONLY_DATA)
	  form->db_stat|=HA_READ_ONLY;
	if (form->db_type != DB_TYPE_MISAM && form->db_type != DB_TYPE_HASH)
	  form->db_record_offset=form->reclength;
      }
    }
    break;
  default:
    break;
  }
  return 0;
}


	/* close database */

int ha_close(register TABLE *form)
{
  int error=0;
  DBUG_ENTER("ha_close");

  if (form->db_stat & (HA_OPEN_KEYFILE+HA_OPEN_RNDFILE+HA_GET_INFO))
    error= (* ((int (*)(byte*)) (*funcs[1][(int) form->db_type-2])))(form->file);
  if (error)
    my_error(ER_ERROR_ON_CLOSE,MYF(ME_BELL+ME_WAITTANG),
	     form->table_name,my_errno);
  DBUG_RETURN(error);
} /* ha_close */


	/* write to database. Fix timestamp and next_number */

int ha_write(register TABLE *form, byte * buf)
{
  register int error=0;
  DBUG_ENTER("ha_write");
  pthread_mutex_lock(&LOCK_status);
  ha_write_count++;
  pthread_mutex_unlock(&LOCK_status);

  if (form->time_stamp)
    set_form_timestamp(buf+form->time_stamp-1);
  if (form->next_number_field && buf == form->record[0])
    update_next_number(form);
  if ((* ((int (*)(char *,byte *))(*funcs[2][(uint) form->db_type-2])))
      ((char*) form->file,buf))
    error=my_errno ? my_errno : -1;
#ifndef DBUG_OFF
  if (error && error != HA_ERR_FOUND_DUPP_KEY)
    DBUG_PRINT("error",("errno: %d",error));
#endif
  DBUG_RETURN (error);
} /* ha_write */


	/* update database */

int ha_update(register TABLE *form, byte * old_data, byte * new_data)
{
  register int error=0;
  DBUG_ENTER("ha_update");
  pthread_mutex_lock(&LOCK_status);
  ha_update_count++;
  pthread_mutex_unlock(&LOCK_status);

  if (form->time_stamp)
    set_form_timestamp(new_data+form->time_stamp-1);
  if ((* ((int (*)(char *,byte*,byte*))(*funcs[3][(uint) form->db_type-2])))
      ((char*) form->file,old_data,new_data))
    error=my_errno ? my_errno : -1;
#ifndef DBUG_OFF
  if (error)
    DBUG_PRINT("error",("errno: %d",error));
#endif
  DBUG_RETURN (error);
} /* ha_update */


	/* delete from database */

int ha_delete(register TABLE *form, byte * buf)
{
  register int error=0;
  DBUG_ENTER("ha_delete");
  pthread_mutex_lock(&LOCK_status);
  ha_delete_count++;
  pthread_mutex_unlock(&LOCK_status);

  if ((* ((int (*)(char *,byte*))(*funcs[4][(uint) form->db_type-2])))
      ((char*) form->file,buf))
    error=(my_errno ? my_errno : -1);
#ifndef DBUG_OFF
  if (error)
    DBUG_PRINT("error",("errno: %d",error));
#endif
  DBUG_RETURN (error);
} /* ha_delete */


	/* read key in database */

int ha_rkey(register TABLE *form, byte * buf, int inx, byte * key,
	    uint key_len, enum ha_rkey_function find_flag)
{
  reg2 int error=0;
  DBUG_ENTER("ha_rkey");
  DBUG_PRINT("enter",("form: %lx  '%s'",form,form->table_name));
  DBUG_PRINT("enter",("index: %d  key: '%.*s'  key_len: %d  flag: %d",
		      inx,
		      (key_len ? key_len : form->key_info[inx].key_length),key,
		      key_len,find_flag));
  DBUG_DUMP("key",(char*) key,
	    (key_len ? key_len : form->key_info[inx].key_length));

  pthread_mutex_lock(&LOCK_status);
  ha_read_key_count++;
  pthread_mutex_unlock(&LOCK_status);

  switch (form->db_type) {
#if !defined(NO_HASH) || !defined(NO_HEAP)
  case DB_TYPE_HASH:
  case DB_TYPE_HEAP:
    if ((* ((int (*)(char *,byte *,int,byte*))(*funcs[5][(uint) form->db_type-2])))((char*) form->file,buf,inx,key))
      error=my_errno ? my_errno : -1;
    break;
#endif
  default:					/* M_ISAM,PISAM,RMS,ISAM */
    if ((* ((int (*)(char *,byte *,int,byte *,uint,int))(*funcs[5][(uint) form->db_type-2])))((char*) form->file,buf,inx,key,key_len,(int) find_flag))
      error=my_errno ? my_errno : -1;
    break;
  }
  if (error)
  {
    DBUG_PRINT((error == HA_ERR_KEY_NOT_FOUND ? "extra_info" : "error"),
	       ("my_errno: %d",error));
    form->status=STATUS_NOT_FOUND;
  }
  else
    form->status=0;
  DBUG_RETURN (error);
} /* ha_rkey */


	/* read next record from database */

int ha_rnext(register TABLE *form, byte * buf, register int inx)
{
  reg3 int error=0;
  DBUG_ENTER("ha_rnext");
  DBUG_PRINT("enter",("form: %lx  '%s'",form,form->table_name));

  pthread_mutex_lock(&LOCK_status);
  ha_read_next_count++;
  pthread_mutex_unlock(&LOCK_status);

  if ((* ((int (*)(char *,byte *,int))(*funcs[6][(uint) form->db_type-2])))((char*) form->file,buf,inx))
    error=(my_errno ? my_errno : HA_ERR_END_OF_FILE);
  if (error)
  {
    DBUG_PRINT((error == HA_ERR_END_OF_FILE ? "extra_info" : "error"),
	       ("my_errno: %d",error));
    form->status=STATUS_NOT_FOUND;
  }
  else
    form->status=0;
  DBUG_RETURN (error);
} /* ha_rnext */


	/* read previous record from database */

int ha_rprev(register TABLE *form, byte * buf, register int inx)
{
  reg3 int error=0;
  DBUG_ENTER("ha_rprev");

  pthread_mutex_lock(&LOCK_status);
  ha_read_next_count++;
  pthread_mutex_unlock(&LOCK_status);

  if ((* ((int (*)(char *,byte *,int))(*funcs[7][(uint) form->db_type-2])))((char*) form->file,buf,inx))
    error=my_errno ? my_errno : HA_ERR_END_OF_FILE;
  if (error)
  {
    DBUG_PRINT("error",("my_errno: %d",error));
    form->status=STATUS_NOT_FOUND;
  }
  else
    form->status=0;
  DBUG_RETURN (error);
} /* ha_rprev */

	/* read first record from database */

int ha_rfirst(register TABLE *form, byte * buf, int inx)
{
  register int error=0;
  DBUG_ENTER("ha_rfirst");

  pthread_mutex_lock(&LOCK_status);
  ha_read_key_count++;
  pthread_mutex_unlock(&LOCK_status);

  if ((* ((int (*)(char *,byte *,int))(*funcs[8][(uint) form->db_type-2])))((char*) form->file,buf,inx))
    error=my_errno ? my_errno : HA_ERR_END_OF_FILE;
  if (error)
  {
    DBUG_PRINT("error",("my_errno: %d",error));
    form->status=STATUS_NOT_FOUND;
  }
  else
    form->status=0;
  DBUG_RETURN (error);
} /* ha_rfirst */


	/* read last record from database */

int ha_rlast(register TABLE *form, byte * buf, int inx)
{
  register int error=0;
  DBUG_ENTER("ha_rlast");

  pthread_mutex_lock(&LOCK_status);
  ha_read_next_count++;
  pthread_mutex_unlock(&LOCK_status);

  if ((* ((int (*)(char *,byte *,int))(*funcs[9][(uint) form->db_type-2])))((char*)form->file,buf,inx))
    error=my_errno ? my_errno : HA_ERR_END_OF_FILE;
  if (error)
  {
    DBUG_PRINT("error",("my_errno: %d",error));
    form->status=STATUS_NOT_FOUND;
  }
  else
    form->status=0;
  DBUG_RETURN (error);
} /* ha_rlast */


	/* read the same record that we read last time */

#ifdef NOT_NEEDED_YET
int ha_rsame(register TABLE *form, byte * buf, int inx)
{
  register int error=0;
  DBUG_ENTER("ha_rsame");

  pthread_mutex_lock(&LOCK_status);
  ha_read_key_count++;
  pthread_mutex_unlock(&LOCK_status);

  if ((* ((int (*)(char *,byte *,int))(*funcs[10][(uint) form->db_type-2])))((char*) form->file,buf,inx))
    error=my_errno ? my_errno : -1;
  if (error)
  {
    DBUG_PRINT("error",("my_errno: %d",error));
    form->status=STATUS_NOT_FOUND;
  }
  else
    form->status=0;
  DBUG_RETURN (error);
} /* ha_rsame */
#endif

	/* read the same record that we read last time and fix keys */

#ifdef NOT_NEEDED_YET
int ha_rsame_with_pos(register TABLE *form, byte * buf, int inx, byte *pos)
{
  register int error=0;
  DBUG_ENTER("ha_rsame_with_pos");

  pthread_mutex_lock(&LOCK_status);
  ha_read_key_count++;
  pthread_mutex_unlock(&LOCK_status);

  if (form->db_type == DB_TYPE_ISAM)
  {
    error=ni_rsame_with_pos((N_INFO*) form->file,buf,inx,
			    (long) *((uint32*) pos));
#ifndef DBUG_OFF
    if (error)
      DBUG_PRINT("error",("my_errno: %d",error));
#endif
  }
  else
  {
    if (!(error=ha_r_rnd(form,buf,pos)) && inx != -1)
      error=ha_rsame(form,buf,inx);
  }
  if (error)
    form->status=STATUS_NOT_FOUND;
  else
    form->status=0;
  DBUG_RETURN (error);
} /* ha_rsame_with_pos */
#endif

	/* read a record with rnd-acsess */
	/* If pos == NullS find next record */
	/* else pos must be get using ha_info() after some read */
	/* If ha_r_rnd() = -1 then record is not in database */

int ha_r_rnd(register TABLE *form, byte * buf, byte *pos)
{
  int error;
  DBUG_ENTER("ha_r_rnd");

  pthread_mutex_lock(&LOCK_status);
  ha_read_rnd_count++;
  pthread_mutex_unlock(&LOCK_status);
#ifdef HAVE_RMS_ISAM

  if (form->db_type == DB_TYPE_RMS_ISAM)
  {
    error= -rms_rrnd((RMS_INFO*) form->file,buf,pos);	/* Get 1 if eof */
  }
  else
#endif
  {
    DBUG_PRINT("enter",("form: %lx  buf: %lx  pos: %ld",form,buf,
			pos ? (long) *((uint32*) pos) : -1L));
    if ((error=(* ((int (*)(char *,byte *,long))(*funcs[11][(uint) form->db_type-2])))((char*) form->file,buf,pos ? (long) *((uint32*) pos) : -1L)))
    {
      if (error == 1)
	error= -1;				/* record deleted */
      else
	error= my_errno > 0 ? my_errno : HA_ERR_END_OF_FILE;
    }
  }
#ifndef DBUG_OFF
  if (error > 0 && error != HA_ERR_END_OF_FILE)
    DBUG_PRINT("error",("error: %d  my_errno: %d",error,my_errno));
#endif
  if (error)
    form->status=STATUS_NOT_FOUND;
  else
    form->status=0;
  DBUG_RETURN (error);
} /* ha_r_rnd */


	/* L{ser f|rsta recorden fr}n en fil */
	/* Detta oberoende hur filen {r |ppnad */

int ha_readfirst(register TABLE *form, byte * buf)
{
  register int error;
  DBUG_ENTER("ha_readfirst");

  pthread_mutex_lock(&LOCK_status);
  ha_read_first_count++;
  pthread_mutex_unlock(&LOCK_status);

  error=0;
  if (!(form->db_stat & HA_OPEN_KEYFILE) || form->keys == 0 ||
      ! (ha_option_flag[form->db_type] & HA_READ_ORDER))
  {
    ha_reset(form);
    while ((error= ha_r_rnd(form,buf,(byte*) NullS)) <0) ;
    DBUG_RETURN(error);
  }
  else
    error=ha_rfirst(form,buf,0);
  if (error)
    form->status=STATUS_NOT_FOUND;
  else
    form->status=0;
  DBUG_RETURN (error);
} /* ha_readfirst */


	/* Get KEYFILE_INFO from database */
	/* if flag == 1 we only get lastpos */

void ha_info(register TABLE *form, int flag)
{
  DBUG_ENTER("ha_info");

  switch (form->db_type) {
#ifndef NO_HASH
  case DB_TYPE_HASH:
    {
      HASHINFO info;
      VOID(h_info((H_INFO*) form->file,&info,flag));
      form->keyfile_info.ref.lastpos = info.h_recpos;
      if (flag != 1)
      {
	form->keyfile_info.records = info.h_ant;
	form->keyfile_info.deleted = info.h_delete;
	form->keyfile_info.errkey  = info.h_errkey;
	form->keyfile_info.mean_rec_length=form->reclength;
	form->keyfile_info.data_file_length=form->reclength*
	  (info.h_ant+info.h_delete);
      }
    }
    break;
#endif
#ifndef NO_MISAM
  case DB_TYPE_MISAM:
    {
      ISAMINFO info;
      VOID(is_info((I_INFO*) form->file,&info,flag));
      form->keyfile_info.ref.lastpos = info.i_recpos;
      if (flag != 1)
      {
	form->keyfile_info.records = info.i_ant;
	form->keyfile_info.deleted = info.i_delete;
	form->keyfile_info.errkey  = info.i_errkey;
	form->keyfile_info.mean_rec_length=form->reclength;
	form->keyfile_info.data_file_length=form->reclength*
	  (info.i_ant+info.i_delete);
      }
    }
    break;
#endif
#ifndef NO_PISAM
  case DB_TYPE_PISAM:
    {
      P_ISAMINFO info;
      VOID(ps_info((P_INFO*) form->file,&info,flag));
      form->keyfile_info.ref.lastpos = info.p_recpos;
      if (flag != 1)
      {
	form->keyfile_info.records = info.p_ant;
	form->keyfile_info.deleted = info.p_delete;
	form->keyfile_info.errkey  = info.p_errkey;
	form->keyfile_info.mean_rec_length=form->reclength;
      }
    }
    break;
#endif
#ifdef HAVE_RMS_ISAM
  case DB_TYPE_RMS_ISAM:
    {
      RMSINFO info;
      VOID(rms_info((RMS_INFO*) form->file,&info,flag));
      memcpy(form->keyfile_info.ref.refpos,info.recpos,RMS_REFLENGTH);
      if (flag != 1)
      {
	form->keyfile_info.records = info.ant;
	form->keyfile_info.deleted = info.deleted;
	form->keyfile_info.errkey  = info.errkey;
	form->keyfile_info.ref_length=RMS_REFLENGTH;
	form->keyfile_info.mean_rec_length=form->reclength;
      }
    }
    break;
#endif
#ifndef NO_HEAP
  case DB_TYPE_HEAP:
    {
      HEAPINFO info;
      VOID(heap_info((HP_INFO *) form->file,&info,flag));
      form->keyfile_info.ref.lastpos = info.current_record;
      if (flag != 1)
      {
	form->keyfile_info.records = info.records;
	form->keyfile_info.deleted = info.deleted;
	form->keyfile_info.errkey  = info.errkey;
	form->keyfile_info.mean_rec_length=form->reclength;
	form->keyfile_info.data_file_length=form->reclength*
	  (info.records+info.deleted);
      }
    }
    break;
#endif
  case DB_TYPE_ISAM:
    {
      N_ISAMINFO info;
      VOID(ni_info((N_INFO*) form->file,&info,flag));
      form->keyfile_info.ref.lastpos = info.recpos;
      if (flag != 1)
      {
	form->keyfile_info.records = info.records;
	form->keyfile_info.deleted = info.deleted;
	form->keyfile_info.data_file_length=info.data_file_length;
	form->keyfile_info.errkey  = info.errkey;
	form->keyfile_info.create_time = info.create_time;
	form->keyfile_info.check_time  = info.isamchk_time;
	form->keyfile_info.update_time = info.update_time;
	form->keyfile_info.mean_rec_length=info.mean_reclength;
	form->keyfile_info.rec_per_key=info.rec_per_key;
	form->keyfile_info.sortkey = info.sortkey;
	form->keyfile_info.filenr  = info.filenr;
	form->keyfile_info.block_size=nisam_block_size;
	form->db_create_options    = info.options;
	/* Set keys in use */
	if (form->db_stat & HA_OPEN_KEYFILE)
	  form->keys		   = min(form->keys,info.keys);
      }
    }
    break;
  case DB_TYPE_MRG_ISAM:
#ifndef NO_MERGE
    {
      MERGE_INFO info;
      VOID(mrg_info((MRG_INFO*) form->file,&info,flag));
      form->keyfile_info.ref.lastpos = (ha_rows) info.recpos;
      if (flag != 1)
      {
	form->keyfile_info.records = (ha_rows) info.records;
	form->keyfile_info.deleted = (ha_rows) info.deleted;
	form->keyfile_info.errkey  = info.errkey;
	form->keyfile_info.data_file_length=info.data_file_length;
	form->db_create_options    = info.options;
      }
    }
#endif
    break;
  default:
    break;						/* impossible  */
  }
  DBUG_PRINT("exit",("records: %ld  deleted: %ld  lastpos: %ld",
		     form->keyfile_info.records,
		     form->keyfile_info.deleted,
		     form->keyfile_info.ref.lastpos));
  DBUG_VOID_RETURN;
} /* ha_info */


	/* Some extra flags we can use on database */

int ha_extra(TABLE *form, enum ha_extra_function operation)
{
  DBUG_ENTER("ha_extra");
  DBUG_PRINT("enter",("operation: %d",operation));

  if ((specialflag & SPECIAL_SAFE_MODE || test_flags & TEST_NO_EXTRA) &&
      (operation == HA_EXTRA_WRITE_CACHE ||
       operation == HA_EXTRA_KEYREAD))
    DBUG_RETURN(0);
  DBUG_RETURN((* ((int (*)(char *,int))(*funcs[13][(uint) form->db_type-2])))
	      ((char*) form->file,(int) operation));
} /* ha_extra */


	/* Reset database to state after open */

void ha_reset(TABLE *form)
{
  DBUG_ENTER("ha_reset");
  VOID((* ((int (*)(char *,int))(*funcs[13][(uint) form->db_type-2])))((char*) form->file,(int) HA_EXTRA_RESET));
  DBUG_VOID_RETURN;
} /* ha_reset */


	/* Lock a database for reading or writing */

int ha_lock(TABLE *form, int lock_type)
{
  int error;
  DBUG_ENTER("ha_lock");
  if (!(error=(* ((int (*)(char *,int))(*funcs[12][(uint) form->db_type-2])))((char*) form->file,lock_type)))
    form->locked=lock_type;
  DBUG_RETURN(error);
} /* ha_lock */


	/* close, flush or restart databases */
	/* Ignore this for other databases than ours */

int ha_panic(enum ha_panic_function flag)
{
  int error=0;
#ifndef NO_MERGE
  error|=mrg_panic(flag);
#endif
#ifndef NO_HASH
  error|=h_panic(flag);			/* fix hash */
#endif
#ifndef NO_MISAM
  error|=is_panic(flag);		/* close m_isam files */
#endif
#ifndef NO_PISAM
  error|=ps_panic(flag);		/* close p_isam files */
#endif
#ifndef NO_HEAP
  error|=heap_panic(flag);
#endif
  error|=ni_panic(flag);
  return error;
} /* ha_panic */


	/* Use key cacheing on all databases */

void ha_key_cache(void)
{
  if (keybuff_size)
    VOID(init_key_cache(keybuff_size,(uint) (10*4*(IO_SIZE+MALLOC_OVERHEAD))));
} /* ha_key_cache */


	/* Print error that we got from handler function */

void ha_error(TABLE *form, int error, myf errflag)
{
  int textno=ER_GET_ERRNO;
  switch (error) {
  case EAGAIN:
    textno=ER_FILE_USED;
    break;
  case ENOENT:
    textno=ER_FILE_NOT_FOUND;
    break;
  case HA_ERR_KEY_NOT_FOUND:
  case HA_ERR_NO_ACTIVE_RECORD:
  case HA_ERR_END_OF_FILE:
    textno=ER_KEY_NOT_FOUND;
    break;
  case HA_ERR_FOUND_DUPP_KEY:
  {
    int key_nr=ha_keyerror(form,error);
    if (key_nr >= 0)
    {
      /* Write the dupplicated key in the error message */
      char key[MAX_KEY_LENGTH];
      String str(key,sizeof(key));
      key_unpack(&str,form,(uint) key_nr);
      uint max_length=MYSQL_ERRMSG_SIZE-strlen(ER(ER_DUP_ENTRY));
      if (str.length() >= max_length)
      {
	str.length(max_length-4);
	str.append("...");
      }
      my_error(ER_DUP_ENTRY,MYF(0),str.c_ptr(),key_nr+1);
      return;
    }
    textno=ER_DUP_KEY;
    break;
  }
  case HA_ERR_RECORD_CHANGED:
    textno=ER_CHECKREAD;
    break;
  case HA_ERR_CRASHED:
    textno=ER_NOT_KEYFILE;
    break;
  case HA_ERR_OUT_OF_MEM:
    textno=ER_OUTOFMEMORY;
    break;
  case HA_ERR_WRONG_COMMAND:
    textno=ER_ILLEGAL_HA;
    break;
  case HA_ERR_OLD_FILE:
    textno=ER_OLD_KEYFILE;
    break;
  case HA_ERR_UNSUPPORTED:
    textno=ER_UNSUPPORTED_EXTENSION;
    break;
  case HA_ERR_RECORD_FILE_FULL:
    textno=ER_RECORD_FILE_FULL;
    break;
  default:
    {
      my_error(ER_GET_ERRNO,errflag,error);
      return;
    }
  }
  my_error(textno,errflag,form->table_name,error);
} /* ha_error() */


	/* Return key if error because of dupplicated keys */

int ha_keyerror(TABLE *form, int error)
{
  DBUG_ENTER("ha_keyerror");
  form->keyfile_info.errkey  = -1;
  if (error == HA_ERR_FOUND_DUPP_KEY)
    ha_info(form,2);
  DBUG_RETURN(form->keyfile_info.errkey);
} /* ha_keyerror */


	/* Use other databasehandler if databasehandler is not incompiled */

enum db_type ha_checktype(enum db_type database_type)
{
  switch (database_type) {
#ifndef NO_HASH
  case DB_TYPE_HASH:
#endif
#ifndef NO_MISAM
  case DB_TYPE_MISAM:
#endif
#ifndef NO_PISAM
  case DB_TYPE_PISAM:
#endif
#ifdef HAVE_RMS_ISAM
  case DB_TYPE_RMS_ISAM:
#endif
#ifndef NO_HEAP
  case DB_TYPE_HEAP:
#endif
#ifndef NO_MERGE
  case DB_TYPE_MRG_ISAM:
#endif
  case DB_TYPE_ISAM:
    return (database_type);			/* Database exists on system */
  default:
    break;
  }
  return(DB_TYPE_ISAM);				/* Use this as default */
} /* ha_checktype */



	/* rename database (indexfile and datafile) */

int ha_frename(enum db_type base, const char * from, const char * to)
{
  DBUG_ENTER("ha_frename");

  if ((bas_ext[(uint) base][0][0] &&
       rename_file_ext(from,to,bas_ext[(uint) base][0])) ||
      (bas_ext[(uint) base][1][0] &&
       rename_file_ext(from,to,bas_ext[(uint) base][1])))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}

	/* dummys for not existing functions */

static int ha_err(void)
{
  my_errno=HA_ERR_WRONG_COMMAND;
  return -1;
}

static int ha_ok(void)
{
  return 0;
}

static int delete_file(my_string name,my_string ext,int extflag)
{
  char buff[FN_REFLEN];
  VOID(fn_format(buff,name,"",ext,extflag | 4));
  return(my_delete(buff,MYF(MY_WME)));
}


int ha_fdelete(enum db_type base,my_string name)
{
  DBUG_ENTER("ha_fdelete");

  if ((bas_ext[base][0][0] && delete_file(name,bas_ext[base][0],2)) ||
      (bas_ext[base][1][0] && delete_file(name,bas_ext[base][1],2)))
   DBUG_RETURN(1);

  switch (base) {
#ifndef NO_HEAP
  case DB_TYPE_HEAP:
    {
      char buff[FN_REFLEN];
      DBUG_RETURN (heap_delete_all(fn_format(buff,name,"","",1+2)));
    }
#endif
  default:
    break;
  }
  DBUG_RETURN(0);
} /* ha_fdelete */


	/* Set a timestamp in record */

static void NEAR_F set_form_timestamp(byte *record)
{
  long skr= (long) current_thd->query_start();
  longstore(record,skr);
  return;
}


	/* Updates field with field_type NEXT_NUMBER according to following:
	   if field = 0 change field to the next free key in database.
	   */

static void NEAR_F update_next_number(TABLE *form)
{
  longlong nr;
  int error;
  THD *thd;
  DBUG_ENTER("update_next_number");

  if (form->next_number_field->val_int() != 0)
    DBUG_VOID_RETURN;
  thd=current_thd;
  if ((nr=thd->next_insert_id))
  {
    thd->next_insert_id=0;			// Clear after use
  }
  else
  {
    VOID(ha_extra(form,HA_EXTRA_KEYREAD));
    error=ha_rlast(form,form->record[1],(int) form->next_number_index);
    if (error)
      nr=1;
    else
      nr=(longlong) form->next_number_field->
	val_int_offset(form->rec_buff_length)+1;
    VOID(ha_extra(form,HA_EXTRA_NO_KEYREAD));
  }
  thd->insert_id((ulonglong) nr);
  form->next_number_field->store(nr);
  DBUG_VOID_RETURN;
}
