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

/* Skriver ett record till en isam-databas */

#include "isamdef.h"
#ifdef	__WIN32__
#include <errno.h>
#endif

	/* Functions declared in this file */

static int w_search(N_INFO *info,N_KEYDEF *keyinfo,uchar *key,
		    ulong pos, uchar *father_buff, uchar *father_keypos,
		    ulong father_page);
static int _ni_balance_page(N_INFO *info,N_KEYDEF *keyinfo,uchar *key,
			    uchar *curr_buff,uchar *father_buff,
			    uchar *father_keypos,ulong father_page);


	/* Write new record to database */

int ni_write(N_INFO *info, const byte *record)
{
  uint i;
  ulong filepos;
  uchar *buff;
  DBUG_ENTER("ni_write");
  DBUG_PRINT("enter",("isam: %d  data: %d",info->s->kfile,info->dfile));

  if (info->s->base.options & HA_OPTION_READ_ONLY_DATA)
  {
    my_errno=EACCES;
    DBUG_RETURN(-1);
  }
#ifndef NO_LOCKING
  if (_ni_readinfo(info,F_WRLCK,1)) DBUG_RETURN(-1);
#endif
  dont_break();				/* Dont allow SIGHUP or SIGINT */
#if !defined(NO_LOCKING) && defined(USE_RECORD_LOCK)
  if (!info->locked && my_lock(info->dfile,F_WRLCK,0L,F_TO_EOF,
			       MYF(MY_SEEK_NOT_DONE) | info->lock_wait))
    goto err;
#endif
  filepos= ((info->s->state.dellink != NI_POS_ERROR) ?
	    info->s->state.dellink :
	    info->s->state.data_file_length);

  if (info->s->base.reloc == 1L && info->s->base.records == 1L &&
      info->s->state.records == 1L)
  {						/* System file */
    my_errno=HA_ERR_RECORD_FILE_FULL;
    goto err2;
  }

	/* Write all keys to indextree */
  buff=info->lastkey+info->s->base.max_key_length;
  for (i=0 ; i < info->s->state.keys ; i++)
  {
    VOID(_ni_make_key(info,i,buff,record,filepos));
    if (_ni_ck_write(info,i,buff)) goto err;
  }

  if ((*info->s->write_record)(info,record))
    goto err;

  info->update= HA_STATE_CHANGED+HA_STATE_AKTIV+HA_STATE_WRITTEN;
  info->s->state.records++;
  info->lastpos=filepos;
  nisam_log_record(LOG_WRITE,info,record,filepos,0);
  VOID(_ni_writeinfo(info,1));
  allow_break();				/* Allow SIGHUP & SIGINT */
  DBUG_RETURN(0);

err:
  if (my_errno == HA_ERR_FOUND_DUPP_KEY || my_errno == HA_ERR_RECORD_FILE_FULL)
  {
    info->errkey= (int) i;
    while ( i-- > 0)
    {
      VOID(_ni_make_key(info,i,buff,record,filepos));
      if (_ni_ck_delete(info,i,buff))
	break;
    }
  }
  info->update=HA_STATE_CHANGED+HA_STATE_WRITTEN;
err2:
  nisam_log_record(LOG_WRITE,info,record,filepos,my_errno);
  VOID(_ni_writeinfo(info,1));
  allow_break();			/* Allow SIGHUP & SIGINT */
  DBUG_RETURN(-1);
} /* ni_write */


	/* Write one key to btree */

int _ni_ck_write(register N_INFO *info, uint keynr, uchar *key)
{
  int error;
  DBUG_ENTER("_ni_ck_write");

  if ((error=w_search(info,info->s->keyinfo+keynr,key,
		      info->s->state.key_root[keynr], (uchar *) 0, (uchar*) 0,
		      0L)) > 0)
    error=_ni_enlarge_root(info,keynr,key);
  DBUG_RETURN(error);
} /* _ni_ck_write */


	/* Make a new root with key as only pointer */

int _ni_enlarge_root(register N_INFO *info, uint keynr, uchar *key)
{
  uint t_length,nod_flag;
  reg2 N_KEYDEF *keyinfo;
  S_PARAM s_temp;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("_ni_enlarge_root");

  nod_flag= (share->state.key_root[keynr] != NI_POS_ERROR) ?
    share->base.key_reflength : 0;
  _ni_kpointer(info,info->buff+2,share->state.key_root[keynr]); /* if nod */
  keyinfo=share->keyinfo+keynr;
  t_length=_ni_get_pack_key_length(keyinfo,nod_flag,(uchar*) 0,(uchar*) 0,
				   key,&s_temp);
  putint(info->buff,t_length+2+nod_flag,nod_flag);
  _ni_store_key(keyinfo,info->buff+2+nod_flag,&s_temp);
  if ((share->state.key_root[keynr]= _ni_new(info,keyinfo)) ==
      NI_POS_ERROR ||
      _ni_write_keypage(info,keyinfo,share->state.key_root[keynr],info->buff))
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
} /* _ni_enlarge_root */


	/* S|ker reda p} vart nyckeln skall s{ttas och placerar den dit */
	/* Returnerar -1 om fel ; 0 om ok.  1 om nyckel propagerar upp}t */

static int w_search(register N_INFO *info, register N_KEYDEF *keyinfo, uchar *key, ulong page, uchar *father_buff, uchar *father_keypos, ulong father_page)
{
  int error,flag;
  uint comp_flag,nod_flag;
  uchar *temp_buff,*keypos;
  uchar keybuff[N_MAX_KEY_BUFF];
  DBUG_ENTER("w_search");
  DBUG_PRINT("enter",("page: %ld",page));

  if (page == NI_POS_ERROR)
    DBUG_RETURN(1);				/* No key, make new */

  if (keyinfo->base.flag & HA_SORT_ALLOWS_SAME)
    comp_flag=SEARCH_BIGGER;			/* Put after same key */
  else if (keyinfo->base.flag & HA_NOSAME)
    comp_flag=SEARCH_FIND;			/* No dupplicates */
  else
    comp_flag=SEARCH_SAME;			/* Keys in rec-pos order */

  if (!(temp_buff= (uchar*) my_alloca((uint) keyinfo->base.block_length+
				      N_MAX_KEY_BUFF)))
    DBUG_RETURN(-1);
  if (!_ni_fetch_keypage(info,keyinfo,page,temp_buff,0))
    goto err;

  flag=(*keyinfo->bin_search)(info,keyinfo,temp_buff,key,0,comp_flag,&keypos,
			      keybuff);
  nod_flag=test_if_nod(temp_buff);
  if (flag == 0)
  {
    my_errno=HA_ERR_FOUND_DUPP_KEY;
	/* get position to record with dupplicated key */
    VOID((*keyinfo->get_key)(keyinfo,nod_flag,&keypos,keybuff));
    info->int_pos=_ni_dpos(info,test_if_nod(temp_buff),keypos);
    my_afree((byte*) temp_buff);
    DBUG_RETURN(-1);
  }
  if ((error=w_search(info,keyinfo,key,_ni_kpos(nod_flag,keypos),
		      temp_buff,keypos,page)) >0)
  {
    error=_ni_insert(info,keyinfo,key,temp_buff,keypos,keybuff,father_buff,
		     father_keypos,father_page);
    if (_ni_write_keypage(info,keyinfo,page,temp_buff))
      goto err;
  }
  my_afree((byte*) temp_buff);
  DBUG_RETURN(error);
err:
  my_afree((byte*) temp_buff);
  DBUG_PRINT("exit",("Error: %d",my_errno));
  DBUG_RETURN (-1);
} /* w_search */


	/* Insert new key at right of key_pos */
	/* Returns 2 if key contains key to upper level */

int _ni_insert(register N_INFO *info, register N_KEYDEF *keyinfo, uchar *key, uchar *anc_buff, uchar *key_pos, uchar *key_buff, uchar *father_buff, uchar *father_key_pos, ulong father_page)



					/* Last key before current key */



{
  uint a_length,t_length,nod_flag;
  uchar *endpos;
  S_PARAM s_temp;
  DBUG_ENTER("_ni_insert");
  DBUG_PRINT("enter",("key_pos: %lx",key_pos));
  DBUG_EXECUTE("key",_ni_print_key(DBUG_FILE,keyinfo->seg,key););

  nod_flag=test_if_nod(anc_buff);
  a_length=getint(anc_buff);
  endpos= anc_buff+ a_length;
  t_length=_ni_get_pack_key_length(keyinfo,nod_flag,
				   (key_pos == endpos ? (uchar*) 0 : key_pos),
				   (key_pos == anc_buff+2+nod_flag ?
				    (uchar*) 0 : key_buff),key,&s_temp);
#ifndef DBUG_OFF
  if (key_pos != anc_buff+2+nod_flag)
    DBUG_DUMP("prev_key",(byte*) key_buff,_ni_keylength(keyinfo,key_buff));
  if (keyinfo->base.flag & HA_PACK_KEY)
  {
    DBUG_PRINT("test",("t_length: %d  ref_len: %d",
		       t_length,s_temp.ref_length));
    DBUG_PRINT("test",("n_ref_len: %d  n_length: %d  key: %lx",
		       s_temp.n_ref_length,s_temp.n_length,s_temp.key));
  }
#endif
  bmove_upp((byte*) endpos+t_length,(byte*) endpos,(uint) (endpos-key_pos));
  _ni_store_key(keyinfo,key_pos,&s_temp);
  a_length+=t_length;
  putint(anc_buff,a_length,nod_flag);
  if (a_length <= keyinfo->base.block_length)
    DBUG_RETURN(0);				/* There is room on page */

  /* Page is full */

  if (!(keyinfo->base.flag & (HA_PACK_KEY | HA_SPACE_PACK_USED)) &&
      father_buff)
    DBUG_RETURN(_ni_balance_page(info,keyinfo,key,anc_buff,father_buff,
				 father_key_pos,father_page));
  DBUG_RETURN(_ni_splitt_page(info,keyinfo,key,anc_buff,key_buff));
} /* _ni_insert */


	/* splitt a full page in two and assign emerging item to key */

int _ni_splitt_page(register N_INFO *info, register N_KEYDEF *keyinfo, uchar *key, uchar *buff, uchar *key_buff)
{
  uint length,a_length,key_ref_length,t_length,nod_flag;
  uchar *key_pos,*pos;
  ulong new_pos;
  S_PARAM s_temp;
  DBUG_ENTER("ni_splitt_page");
  DBUG_DUMP("buff",(byte*) buff,getint(buff));

  nod_flag=test_if_nod(buff);
  key_ref_length=2+nod_flag;
  key_pos=_ni_find_half_pos(info,keyinfo,buff,key_buff);
  length=(uint) (key_pos-buff);
  a_length=getint(buff);
  putint(buff,length,nod_flag);

	/* Correct new page pointer */
  VOID((*keyinfo->get_key)(keyinfo,nod_flag,&key_pos,key_buff));
  if (nod_flag)
  {
    DBUG_PRINT("test",("Splitting nod"));
    pos=key_pos-nod_flag;
    memcpy((byte*) info->buff+2,(byte*) pos,(size_t) nod_flag);
  }

	/* Move midle item to key and pointer to new page */
  if ((new_pos=_ni_new(info,keyinfo)) == NI_POS_ERROR)
    DBUG_RETURN(-1);
  _ni_kpointer(info,_ni_move_key(keyinfo,key,key_buff),new_pos);

	/* Store new page */
  VOID((*keyinfo->get_key)(keyinfo,nod_flag,&key_pos,key_buff));
  t_length=_ni_get_pack_key_length(keyinfo,nod_flag,key_pos,(uchar*)0,key_buff,
				   &s_temp);
  length=(uint) ((buff+a_length)-key_pos);
  memcpy((byte*) info->buff+key_ref_length+t_length,(byte*) key_pos,
	 (size_t) length);
  _ni_store_key(keyinfo,info->buff+key_ref_length,&s_temp);
  putint(info->buff,length+t_length+key_ref_length,nod_flag);

  if (_ni_write_keypage(info,keyinfo,new_pos,info->buff))
    DBUG_RETURN(-1);
  DBUG_DUMP("key",(byte*) key,_ni_keylength(keyinfo,key));
  DBUG_RETURN(2);				/* Middle key up */
} /* _ni_splitt_page */


	/* find out how much more room a key will take */

uint _ni_get_pack_key_length(N_KEYDEF *keyinfo, uint nod_flag, uchar *key_pos, uchar *key_buff, uchar *key, S_PARAM *s_temp)

					/* If nod: Length of nod-pointer */
					/* Position to pos after key in buff */
					/* Last key before current key */
					/* Current key */
					/* How next key will be packed */
{
  reg1 N_KEYSEG *keyseg;
  int length;
  uint key_length,ref_length,n_length,diff_flag,same_length;
  uchar *start,*end,*key_end;

  s_temp->key=key;
  if (!(keyinfo->base.flag & HA_PACK_KEY))
    return (s_temp->totlength=_ni_keylength(keyinfo,key)+nod_flag);
  s_temp->ref_length=s_temp->n_ref_length=s_temp->n_length=0;

  same_length=0; keyseg=keyinfo->seg;
  key_length=_ni_keylength(keyinfo,key)+nod_flag;

  if (keyseg->base.flag & HA_SPACE_PACK)
  {
    diff_flag=1;
    end=key_end= key+ *key+1;
    if (key_buff)
    {
      if (*key == *key_buff && *key)
	same_length=1;			/* Don't use key-pack if length == 0 */
      else if (*key > *key_buff)
	end=key+ *key_buff+1;
      key_buff++;
    }
    key++;
  }
  else
  {
    diff_flag=0;
    key_end=end= key+keyseg->base.length;
  }

  start=key;
  if (key_buff)
    while (key < end && *key == *key_buff)
    {
      key++; key_buff++;
    }

  s_temp->key=key; s_temp->key_length= (uint) (key_end-key);

  if (same_length && key == key_end)
  {
    s_temp->ref_length=128;
    length=(int) key_length-(int)(key_end-start); /* Same as prev key */
  }
  else
  {
    if (start != key)
    {						/* Starts as prev key */
      s_temp->ref_length= (uint) (key-start)+128;
      length=(int) (1+key_length-(uint) (key-start));
    }
    else
      length=(int) (key_length+ (1-diff_flag));		/* Not packed key */
  }
  s_temp->totlength=(uint) length;

  DBUG_PRINT("test",("tot_length: %d  length: %d  uniq_key_length: %d",
		     key_length,length,s_temp->key_length));

	/* If something after that is not 0 length test if we can combine */

  if (key_pos && (n_length= *key_pos))
  {
    key_pos++;
    ref_length=0;
    if (n_length & 128)
    {
      if ((ref_length=n_length & 127))
	if (diff_flag)
	  n_length= *key_pos++;			/* Length of key-part */
	else
	  n_length=keyseg->base.length - ref_length;
    }
    else
      if (*start == *key_pos && diff_flag && start != key_end)
	length++;				/* One new pos for ref.len */

    DBUG_PRINT("test",("length: %d  key_pos: %lx",length,key_pos));
    if (n_length != 128)
    {						/* Not same key after */
      key=start+ref_length;
      while (n_length > 0 && key < key_end && *key == *key_pos)
      {
	key++; key_pos++;
	ref_length++;
	n_length--;
	length--;				/* We gained one char */
      }

      if (n_length == 0 && diff_flag)
      {
	n_length=128;				/* Same as prev key */
	length--;				/* We don't need key-length */
      }
      else if (ref_length)
	s_temp->n_ref_length=ref_length | 128;
    }
    s_temp->n_length=n_length;
  }
  return (uint) length;
} /* _ni_get_pack_key_length */


	/* store a key in page-buffert */

void _ni_store_key(N_KEYDEF *keyinfo, register uchar *key_pos, register S_PARAM *s_temp)
{
  uint length;
  uchar *start;

  if (! (keyinfo->base.flag & HA_PACK_KEY))
  {
    memcpy((byte*) key_pos,(byte*) s_temp->key,(size_t) s_temp->totlength);
    return;
  }
  start=key_pos;
  if ((*key_pos=(uchar) s_temp->ref_length))
    key_pos++;
  if (s_temp->ref_length == 0 ||
      (s_temp->ref_length > 128 &&
       (keyinfo->seg[0].base.flag & HA_SPACE_PACK)))
    *key_pos++= (uchar) s_temp->key_length;
  bmove((byte*) key_pos,(byte*) s_temp->key,
	(length=s_temp->totlength-(uint) (key_pos-start)));

  key_pos+=length;
  if ((*key_pos = (uchar) s_temp->n_ref_length))
  {
    if (! (keyinfo->seg[0].base.flag & HA_SPACE_PACK))
      return;					/* Don't save keylength */
    key_pos++;					/* Store ref for next key */
  }
  *key_pos = (uchar) s_temp->n_length;
  return;
} /* _ni_store_key */


	/* Calculate how to much to move to split a page in two */
	/* Returns pointer and key for get_key() to get mid key */
	/* There is at last 2 keys after pointer in buff */

uchar *_ni_find_half_pos(N_INFO *info, N_KEYDEF *keyinfo, uchar *page, uchar *key)
{
  uint keys,length,key_ref_length,nod_flag;
  uchar *end,*lastpos;
  DBUG_ENTER("_ni_find_half_pos");

  nod_flag=test_if_nod(page);
  key_ref_length=2+nod_flag;
  length=getint(page)-key_ref_length;
  page+=key_ref_length;
  if (!(keyinfo->base.flag & (HA_PACK_KEY | HA_SPACE_PACK_USED)))
  {
    keys=(length/(keyinfo->base.keylength+nod_flag))/2;
    DBUG_RETURN(page+keys*(keyinfo->base.keylength+nod_flag));
  }

  end=page+length/2-key_ref_length;		/* This is aprox. half */
  *key='\0';
  do
  {
    lastpos=page;
    VOID((*keyinfo->get_key)(keyinfo,nod_flag,&page,key));
   } while (page < end);

   DBUG_PRINT("exit",("returns: %lx  page: %lx  half: %lx",lastpos,page,end));
   DBUG_RETURN(lastpos);
} /* _ni_find_half_pos */


	/* Balance page with not packed keys with page on right/left */
	/* returns 0 if balance was done */

static int _ni_balance_page(register N_INFO *info, N_KEYDEF *keyinfo, uchar *key, uchar *curr_buff, uchar *father_buff, uchar *father_key_pos, ulong father_page)
{
  my_bool right;
  uint k_length,father_length,father_keylength,nod_flag,curr_keylength,
       right_length,left_length,new_right_length,new_left_length,extra_length,
       length,keys;
  uchar *pos,*buff,*extra_buff;
  ulong next_page,new_pos;
  DBUG_ENTER("_ni_balance_page");

  k_length=keyinfo->base.keylength;
  father_length=getint(father_buff);
  father_keylength=k_length+info->s->base.key_reflength;
  nod_flag=test_if_nod(curr_buff);
  curr_keylength=k_length+nod_flag;

  if ((father_key_pos != father_buff+father_length && (info->s->rnd++ & 1)) ||
      father_key_pos == father_buff+2+info->s->base.key_reflength)
  {
    DBUG_PRINT("test",("use right page"));
    right=1;
    next_page= _ni_kpos(info->s->base.key_reflength,
			father_key_pos+father_keylength);
    buff=info->buff;
  }
  else
  {
    DBUG_PRINT("test",("use left page"));
    right=0;
    father_key_pos-=father_keylength;
    next_page= _ni_kpos(info->s->base.key_reflength,father_key_pos);
					/* Fix that curr_buff is to left */
    buff=curr_buff; curr_buff=info->buff;
  }					/* father_key_pos ptr to parting key */

  if (!_ni_fetch_keypage(info,keyinfo,next_page,info->buff,0))
    goto err;
  DBUG_DUMP("next",(byte*) info->buff,getint(info->buff));

	/* Test if there is room to share keys */

  left_length=getint(curr_buff);
  right_length=getint(buff);
  keys=(left_length+right_length-4-nod_flag*2)/curr_keylength;

  if ((right ? right_length : left_length) + curr_keylength <=
      keyinfo->base.block_length)
  {						/* Merge buffs */
    new_left_length=2+nod_flag+(keys/2)*curr_keylength;
    new_right_length=2+nod_flag+((keys+1)/2)*curr_keylength;
    putint(curr_buff,new_left_length,nod_flag);
    putint(buff,new_right_length,nod_flag);

    if (left_length < new_left_length)
    {						/* Move keys buff -> leaf */
      pos=curr_buff+left_length;
      memcpy((byte*) pos,(byte*) father_key_pos, (size_t) k_length);
      memcpy((byte*) pos+k_length, (byte*) buff+2,
	     (size_t) (length=new_left_length - left_length - k_length));
      pos=buff+2+length;
      memcpy((byte*) father_key_pos,(byte*) pos,(size_t) k_length);
      bmove((byte*) buff+2,(byte*) pos+k_length,new_right_length);
    }
    else
    {						/* Move keys -> buff */

      bmove_upp((byte*) buff+new_right_length,(byte*) buff+right_length,
		right_length-2);
      length=new_right_length-right_length-k_length;
      memcpy((byte*) buff+2+length,father_key_pos,(size_t) k_length);
      pos=curr_buff+new_left_length;
      memcpy((byte*) father_key_pos,(byte*) pos,(size_t) k_length);
      memcpy((byte*) buff+2,(byte*) pos+k_length,(size_t) length);
    }

    if (_ni_write_keypage(info,keyinfo,next_page,info->buff) ||
	_ni_write_keypage(info,keyinfo,father_page,father_buff))
      goto err;
    DBUG_RETURN(0);
  }

	/* curr_buff[] and buff[] are full, lets splitt and make new nod */

  extra_buff=info->buff+info->s->base.max_block;
  new_left_length=new_right_length=2+nod_flag+(keys+1)/3*curr_keylength;
  extra_length=nod_flag+left_length+right_length-new_left_length*2
    -curr_keylength;
  putint(curr_buff,new_left_length,nod_flag);
  putint(buff,new_right_length,nod_flag);
  putint(extra_buff,extra_length+2,nod_flag);

  pos=buff+right_length-extra_length;
  memcpy((byte*) extra_buff+2,pos,(size_t) extra_length);
  bmove_upp((byte*) buff+new_right_length+k_length,(byte*) pos,
	    right_length-extra_length-2);
  pos= curr_buff+new_left_length;
  memcpy((byte*) buff+2,(byte*) pos+k_length,
	 (size_t) (length=left_length-new_left_length-k_length));
  memcpy((byte*) buff+2+length,father_key_pos,(size_t) k_length);

  memcpy((byte*) (right ? key : father_key_pos),pos,(size_t) k_length);
  bmove((byte*) (right ? father_key_pos : key),(byte*) buff+new_right_length,
	k_length);

  if ((new_pos=_ni_new(info,keyinfo)) == NI_POS_ERROR)
    goto err;
  _ni_kpointer(info,key+k_length,new_pos);
  if (_ni_write_keypage(info,keyinfo,(right ? new_pos : next_page),
			info->buff) ||
      _ni_write_keypage(info,keyinfo,(right ? next_page : new_pos),extra_buff))
    goto err;

  DBUG_RETURN(1);				/* Middle key up */

err:
  DBUG_RETURN(-1);
} /* _ni_balance_page */
