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

/* open a isam-database */

#include "isamdef.h"
#if defined(MSDOS) || defined(__WIN32__)
#ifdef __WIN32__
#include <fcntl.h>
#else
#include <process.h>			/* Prototype for getpid */
#endif
#endif
#ifdef VMS
#include "static.c"
#endif

static void setup_functions(ISAM_SHARE *info);
static void setup_key_functions(N_KEYDEF *keyinfo);

#define get_next_element(to,pos,size) { memcpy((char*) to,pos,(size_t) size); \
					pos+=size;}

/******************************************************************************
** Return the shared struct if the table is already open.
** In MySQL the server will handle version issues.
******************************************************************************/

static N_INFO *test_if_reopen(char *filename)
{
  LIST *pos;

  for (pos=nisam_open_list ; pos ; pos=pos->next)
  {
    N_INFO *info=(N_INFO*) pos->data;
    ISAM_SHARE *share=info->s;
    if (!strcmp(share->filename,filename) && share->last_version)
      return info;
  }
  return 0;
}


/******************************************************************************
  open a isam database.
  if handle_locking is 0 then exit with error if database is locked
  if handle_locking is 1 then wait if database is locked
  if handle_locking is 2 then continue, but count-vars in st_i_info
  may be wrong. count-vars are automaticly fixed after next isam
  request.
******************************************************************************/


N_INFO *ni_open(const char *name, int mode, int handle_locking)
{
  int lock_error,kfile,open_mode,save_errno;
  uint i,j,len,errpos,head_length,base_pos,offset,info_length,extra;
  char name_buff[FN_REFLEN],*disk_cache,*disk_pos;
  N_INFO info,*m_info,*old_info;
  ISAM_SHARE share_buff,*share;
  DBUG_ENTER("ni_open");

  LINT_INIT(m_info);
  kfile= -1;
  lock_error=1;
  errpos=0;
  head_length=sizeof(share_buff.state.header);
  bzero((byte*) &info,sizeof(info));

  VOID(fn_format(name_buff,name,"",N_NAME_IEXT,4+16));
  pthread_mutex_lock(&THR_LOCK_isam);
  if (!(old_info=test_if_reopen(name_buff)))
  {
    share= &share_buff;
    bzero((gptr) &share_buff,sizeof(share_buff));

    if ((kfile=my_open(name_buff,(open_mode=O_RDWR) | O_SHARE,MYF(0))) < 0)
    {
      if ((errno != EROFS && errno != EACCES) ||
	  mode != O_RDONLY ||
	  (kfile=my_open(name_buff,(open_mode=O_RDONLY) | O_SHARE,MYF(0))) < 0)
	goto err;
    }
    errpos=1;
    if (my_read(kfile,(char*) share->state.header.file_version,head_length,
		MYF(MY_NABP)))
      goto err;

    if (memcmp((byte*) share->state.header.file_version,
	       (byte*) nisam_file_magic, 3) ||
	share->state.header.file_version[3] == 0 ||
	(uchar) share->state.header.file_version[3] > 3)
    {
      DBUG_PRINT("error",("Wrong header in %s",name_buff));
      DBUG_DUMP("error_dump",(char*) share->state.header.file_version,
		head_length);
      my_errno=HA_ERR_CRASHED;
      goto err;
    }
    if (uint2korr(share->state.header.options) &
	~(HA_OPTION_PACK_RECORD | HA_OPTION_PACK_KEYS |
	  HA_OPTION_COMPRESS_RECORD | HA_OPTION_READ_ONLY_DATA |
	  HA_OPTION_TEMP_COMPRESS_RECORD))
    {
      DBUG_PRINT("error",("wrong options: 0x%lx",
			  uint2korr(share->state.header.options)));
      my_errno=HA_ERR_OLD_FILE;
      goto err;
    }
    info_length=uint2korr(share->state.header.header_length);
    base_pos=uint2korr(share->state.header.base_pos);
    if (!(disk_cache=(char*) my_alloca(info_length)))
    {
      my_errno=ENOMEM;
      goto err;
    }
    errpos=2;

    VOID(my_seek(kfile,0L,MY_SEEK_SET,MYF(0)));
#ifndef NO_LOCKING
    if ((lock_error=my_lock(kfile,F_RDLCK,0L,F_TO_EOF,
			    MYF(handle_locking == HA_OPEN_WAIT_IF_LOCKED ?
				0 : MY_DONT_WAIT))) &&
	handle_locking != HA_OPEN_IGNORE_IF_LOCKED)
      goto err;
#endif
    errpos=3;
    if (my_read(kfile,disk_cache,info_length,MYF(MY_NABP)))
      goto err;
    len=uint2korr(share->state.header.state_info_length);
    if (len != sizeof(N_STATE_INFO))
    {
      DBUG_PRINT("warning",
		 ("saved_state_info_length: %d  base_info_length: %d",
		  len,sizeof(N_STATE_INFO)));
    }
    if (len > sizeof(N_STATE_INFO))
      len=sizeof(N_STATE_INFO);
    share->state_length=len;
    memcpy(&share->state.header.file_version[0],disk_cache,(size_t) len);
    len=uint2korr(share->state.header.base_info_length);
    if (len != sizeof(N_BASE_INFO))
    {
      DBUG_PRINT("warning",("saved_base_info_length: %d  base_info_length: %d",
			    len,sizeof(N_BASE_INFO)));
      if (len <= offsetof(N_BASE_INFO,sortkey))
	share->base.sortkey=(ushort) ~0;
    }
    memcpy((char*) (byte*) &share->base,disk_cache+base_pos,
	   (size_t) min(len,sizeof(N_BASE_INFO)));
    disk_pos=disk_cache+base_pos+len;
    share->base.options=uint2korr(share->state.header.options);
    if (share->base.max_key_length > N_MAX_KEY_BUFF)
    {
      my_errno=HA_ERR_UNSUPPORTED;
      goto err;
    }
    if (share->base.options & HA_OPTION_COMPRESS_RECORD)
      share->base.max_key_length+=2;	/* For safety */

    if (!my_multi_malloc(MY_WME,
			 &share,sizeof(*share),
			 &share->keyinfo,share->base.keys*sizeof(N_KEYDEF),
			 &share->rec,(share->base.fields+1)*sizeof(N_RECINFO),
			 &share->blobs,sizeof(N_BLOB)*share->base.blobs,
			 &share->filename,strlen(name_buff)+1,
			 NullS))
      goto err;
    errpos=4;
    memcpy((gptr) share,(gptr) &share_buff,sizeof(share_buff));
    strmov(share->filename,name_buff);

    /* Fix key in used if old nisam-database */
    if (share->state_length <= offsetof(N_STATE_INFO,keys))
      share->state.keys=share->base.keys;

    share->blocksize=min(IO_SIZE,nisam_block_size);
    for (i=0 ; i < share->base.keys ; i++)
    {
      get_next_element(&share->keyinfo[i].base,disk_pos,sizeof(N_SAVE_KEYDEF));
      setup_key_functions(share->keyinfo+i);
      set_if_smaller(share->blocksize,share->keyinfo[i].base.block_length);
      for (j=0 ; j <= share->keyinfo[i].base.keysegs ; j++)
      {
	get_next_element(&share->keyinfo[i].seg[j],disk_pos,
			 sizeof(N_SAVE_KEYSEG));
      }
    }

    for (i=j=offset=0 ; i < share->base.fields ; i++)
    {
      get_next_element(&share->rec[i].base,disk_pos,sizeof(N_SAVE_RECINFO));
#ifndef NOT_PACKED_DATABASES
      share->rec[i].pack_type=0;
      share->rec[i].huff_tree=0;
#endif
      if (share->rec[i].base.type == (int) FIELD_BLOB)
      {
	share->blobs[j].pack_length=share->rec[i].base.length;
	share->blobs[j].offset=offset;
	j++;
	offset+=sizeof(char*);
      }
      offset+=share->rec[i].base.length;
    }
    share->rec[i].base.type=(int) FIELD_LAST;

#ifndef NO_LOCKING
    if (! lock_error)
    {
      VOID(my_lock(kfile,F_UNLCK,0L,F_TO_EOF,MYF(MY_SEEK_NOT_DONE)));
      lock_error=1;			/* Database unlocked */
    }
#endif

    if ((info.dfile=my_open(fn_format(name_buff,name,"",N_NAME_DEXT,2+4),
			    mode | O_SHARE,
			    MYF(MY_WME))) < 0)
      goto err;
    errpos=5;

    share->kfile=kfile;
    share->mode=open_mode;
    share->this_process=(ulong) getpid();
    share->rnd= (int)	 share->this_process;	/* rnd-counter for splitts */
    share->last_process= share->state.process;
    if (!(share->last_version=share->state.version))
      share->last_version=1;			/* Safety */
    share->rec_reflength=share->base.rec_reflength; /* May be changed */

    share->data_file_type=STATIC_RECORD;
    if (share->base.options & HA_OPTION_COMPRESS_RECORD)
    {
      share->data_file_type = COMPRESSED_RECORD;
      share->base.options|= HA_OPTION_READ_ONLY_DATA;
      info.s=share;
      if (_ni_read_pack_info(&info,
			     (pbool) test(!(share->base.options &
					    (HA_OPTION_PACK_RECORD |
					     HA_OPTION_TEMP_COMPRESS_RECORD)))))
	goto err;
    }
    else if (share->base.options & HA_OPTION_PACK_RECORD)
      share->data_file_type = DYNAMIC_RECORD;
    my_afree((gptr) disk_cache);
    setup_functions(share);
#ifdef THREAD
    thr_lock_init(&share->lock);
    VOID(pthread_mutex_init(&share->intern_lock,NULL));
#endif
  }
  else
  {
    share= old_info->s;
    if (mode == O_RDWR && share->mode == O_RDONLY)
    {
      my_errno=EACCES;				/* Can't open in write mode*/
      goto err;
    }
    if ((info.dfile=my_open(fn_format(name_buff,old_info->filename,"",
				      N_NAME_DEXT,2+4),
			    mode | O_SHARE,MYF(MY_WME))) < 0)
    {
      my_errno=errno;
      goto err;
    }
    errpos=5;
  }

  /* alloc and set up private structure parts */
  if (!my_multi_malloc(MY_WME,
		       &m_info,sizeof(N_INFO),
		       &info.blobs,sizeof(N_BLOB)*share->base.blobs,
		       &info.buff,(share->base.max_block*2+
				   share->base.max_key_length),
		       &info.lastkey,share->base.max_key_length*3+1,
		       &info.filename,strlen(name)+1,
		       NullS))
    goto err;
  errpos=6;
  strmov(info.filename,name);
  memcpy(info.blobs,share->blobs,sizeof(N_BLOB)*share->base.blobs);

  info.s=share;
  info.lastpos= NI_POS_ERROR;
  info.update= (short) (HA_STATE_NEXT_FOUND+HA_STATE_PREV_FOUND);
  info.opt_flag=READ_CHECK_USED;
  info.alloced_rec_buff_length=share->base.pack_reclength;
  info.this_uniq= (ulong) info.dfile;	/* Uniq number in process */
  info.this_loop=0;			/* Update counter */
  info.last_uniq= share->state.uniq;
  info.last_loop= share->state.loop;
  info.options=share->base.options |
    (mode == O_RDONLY ? HA_OPTION_READ_ONLY_DATA : 0);
  info.read_record=share->read_record;
  info.lock_type=F_UNLCK;
  info.errkey= -1;
  pthread_mutex_lock(&share->intern_lock);
  share->reopen++;
  if (share->base.options & HA_OPTION_READ_ONLY_DATA)
  {
    info.lock_type=F_RDLCK;
    share->r_locks++;
  }
#ifdef NO_LOCKING
  share->w_locks++;
  info.lock_type=F_WRLCK;
#endif
  pthread_mutex_unlock(&share->intern_lock);

  /* Allocate buffer for one record */

  extra=0;
  if (share->base.options & HA_OPTION_PACK_RECORD)
    extra=ALIGN_SIZE(MAX_DYN_BLOCK_HEADER)+N_SPLITT_LENGTH+
      DYN_DELETE_BLOCK_HEADER;
  if (!(info.rec_alloc=(byte*) my_malloc(share->base.pack_reclength+extra+
					 6,
					 MYF(MY_WME | MY_ZEROFILL))))
    goto err;
  if (extra)
    info.rec_buff=info.rec_alloc+ALIGN_SIZE(MAX_DYN_BLOCK_HEADER);
  else
    info.rec_buff=info.rec_alloc;

  memcpy((byte*) m_info,(byte*) &info,sizeof(info));
#ifdef THREAD
  thr_lock_data_init(&share->lock,&m_info->lock);
#endif

  m_info->open_list.data=(void*) m_info;
  nisam_open_list=list_add(nisam_open_list,&m_info->open_list);

  pthread_mutex_unlock(&THR_LOCK_isam);
  nisam_log(LOG_OPEN,m_info,share->filename,(uint) strlen(share->filename));
  DBUG_RETURN(m_info);

err:
  save_errno=my_errno ? my_errno : HA_ERR_END_OF_FILE;
  switch (errpos) {
  case 6:
    my_free((gptr) m_info,MYF(0));
    /* fall through */
  case 5:
    VOID(my_close(info.dfile,MYF(0)));
    if (old_info)
      break;					/* Don't remove open table */
    /* fall through */
  case 4:
    my_free((gptr) share,MYF(0));
    /* fall through */
  case 3:
#ifndef NO_LOCKING
    if (! lock_error)
      VOID(my_lock(kfile, F_UNLCK, 0L, F_TO_EOF, MYF(MY_SEEK_NOT_DONE)));
#endif
    /* fall through */
  case 2:
    my_afree((gptr) disk_cache);
    /* fall through */
  case 1:
    VOID(my_close(kfile,MYF(0)));
    /* fall through */
  case 0:
  default:
    break;
  }
  pthread_mutex_unlock(&THR_LOCK_isam);
  my_errno=save_errno;
  DBUG_RETURN (NULL);
} /* ni_open */


	/* Set up functions in structs */

static void setup_functions(register ISAM_SHARE *share)
{
  if (share->base.options & HA_OPTION_COMPRESS_RECORD)
  {
    share->read_record=_ni_read_pack_record;
    share->read_rnd=_ni_read_rnd_pack_record;
  }
  else if (share->base.options & HA_OPTION_PACK_RECORD)
  {
    share->read_record=_ni_read_dynamic_record;
    share->read_rnd=_ni_read_rnd_dynamic_record;
    share->delete_record=_ni_delete_dynamic_record;
    share->compare_record=_ni_cmp_dynamic_record;
    if (share->base.blobs)
    {
      share->update_record=_ni_update_blob_record;
      share->write_record=_ni_write_blob_record;
    }
    else
    {
      share->write_record=_ni_write_dynamic_record;
      share->update_record=_ni_update_dynamic_record;
    }
  }
  else
  {
    share->read_record=_ni_read_static_record;
    share->read_rnd=_ni_read_rnd_static_record;
    share->delete_record=_ni_delete_static_record;
    share->compare_record=_ni_cmp_static_record;
    share->update_record=_ni_update_static_record;
    share->write_record=_ni_write_static_record;
  }
  return;
}


static void setup_key_functions(register N_KEYDEF *keyinfo)
{
  if (keyinfo->base.flag & (HA_PACK_KEY | HA_SPACE_PACK_USED))
  {
    keyinfo->bin_search=_ni_seq_search;
    keyinfo->get_key=_ni_get_key;
  }
  else
  {
    keyinfo->bin_search=_ni_bin_search;
    keyinfo->get_key=_ni_get_static_key;
  }
  return;
}
