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
  Gives a approximated number of how many records there is between two keys.
  Used when optimizing querries.
 */

#include "isamdef.h"

static ulong _ni_record_pos(N_INFO *info,const byte *key,uint key_len,
			    enum ha_rkey_function search_flag);
static double _ni_search_pos(N_INFO *info,N_KEYDEF *keyinfo,uchar *key,
			     uint key_len,uint nextflag,ulong pos);
static uint _ni_keynr(N_INFO *info,N_KEYDEF *keyinfo,uchar *page,
		      uchar *keypos,uint *ret_max_key);


	/* If start_key = 0 assume read from start */
	/* If end_key = 0 assume read to end */
	/* Returns NI_POS_ERROR on error */

ulong ni_records_in_range(N_INFO *info, int inx, const byte *start_key,
			  uint start_key_len,
			  enum ha_rkey_function start_search_flag,
			  const byte *end_key, uint end_key_len,
			  enum ha_rkey_function end_search_flag)
{
  ulong start_pos,end_pos;
  DBUG_ENTER("ni_records_in_range");

  if ((inx = _ni_check_index(info,inx)) < 0)
    DBUG_RETURN(NI_POS_ERROR);

#ifndef NO_LOCKING
  if (_ni_readinfo(info,F_RDLCK,1))
    DBUG_RETURN(NI_POS_ERROR);
#endif
  info->update&= HA_STATE_CHANGED;
  start_pos= (start_key ?
	      _ni_record_pos(info,start_key,start_key_len,start_search_flag) :
	      0L);
  end_pos=   (end_key ?
	      _ni_record_pos(info,end_key,end_key_len,end_search_flag) :
	      info->s->state.records+1L);
  VOID(_ni_writeinfo(info,0));
  if (start_pos == NI_POS_ERROR || end_pos == NI_POS_ERROR)
    DBUG_RETURN(NI_POS_ERROR);
  DBUG_PRINT("info",("records: %ld",end_pos-start_pos));
  DBUG_RETURN(end_pos < start_pos ? 0L :
	      (end_pos == start_pos ? 1L : end_pos-start_pos));
}


	/* Find relative position (in records) for key in index-tree */

static ulong _ni_record_pos(N_INFO *info, const byte *key, uint key_len,
			    enum ha_rkey_function search_flag)
{
  uint inx=(uint) info->lastinx;
  N_KEYDEF *keyinfo=info->s->keyinfo+inx;
  uchar *key_buff;
  double pos;

  DBUG_ENTER("_ni_record_pos");
  DBUG_PRINT("enter",("search_flag: %d",search_flag));

  if (key_len >= (keyinfo->base.keylength-info->s->rec_reflength)
      && !(keyinfo->base.flag & HA_SPACE_PACK_USED))
    key_len=USE_HOLE_KEY;
  key_buff=info->lastkey+info->s->base.max_key_length;
  key_len=_ni_pack_key(info,inx,key_buff,(uchar*) key,key_len);
  DBUG_EXECUTE("key",_ni_print_key(DBUG_FILE,keyinfo->seg,
				    (uchar*) key_buff););
  pos=_ni_search_pos(info,keyinfo,key_buff,key_len,
		     nisam_read_vec[search_flag] | SEARCH_SAVE_BUFF,
		     info->s->state.key_root[inx]);
  if (pos >= 0.0)
  {
    DBUG_PRINT("exit",("pos: %ld",(ulong) (pos*info->s->state.records)));
    DBUG_RETURN((ulong) (pos*info->s->state.records+0.5));
  }
  DBUG_RETURN(NI_POS_ERROR);
}


	/* This is a modified version of _ni_search */
	/* Returns offset for key in indextable (decimal 0.0 <= x <= 1.0) */

static double _ni_search_pos(register N_INFO *info, register N_KEYDEF *keyinfo,
			     uchar *key, uint key_len, uint nextflag,
			     register ulong pos)
{
  int flag;
  uint nod_flag,keynr,max_keynr;
  uchar *keypos,*buff;
  double offset;
  DBUG_ENTER("_ni_search_pos");

  if (pos == NI_POS_ERROR)
    DBUG_RETURN(0.5);

  if (!(buff=_ni_fetch_keypage(info,keyinfo,pos,info->buff,1)))
    goto err;
  flag=(*keyinfo->bin_search)(info,keyinfo,buff,key,key_len,nextflag,
			      &keypos,info->lastkey);
  nod_flag=test_if_nod(buff);
  keynr=_ni_keynr(info,keyinfo,buff,keypos,&max_keynr);

  if (flag)
  {
    /*
    ** Didn't found match. keypos points at next (bigger) key
    *  Try to find a smaller, better matching key.
    ** Matches keynr + [0-1]
    */
    if ((offset=_ni_search_pos(info,keyinfo,key,key_len,nextflag,
			       _ni_kpos(nod_flag,keypos))) < 0)
      DBUG_RETURN(offset);
  }
  else
  {
    /*
    ** Found match. Keypos points at the start of the found key
    ** Matches keynr+1
    */
    offset=1.0;					/* Matches keynr+1 */
    if (nextflag & SEARCH_FIND && (!(keyinfo->base.flag & HA_NOSAME)
				   || key_len) && nod_flag)
    {
      /*
      ** There may be identical keys in the tree. Try to match on of those.
      ** Matches keynr + [0-1]
      */
      if ((offset=_ni_search_pos(info,keyinfo,key,key_len,SEARCH_FIND,
				 _ni_kpos(nod_flag,keypos))) < 0)
	DBUG_RETURN(offset);			/* Read error */
    }
  }
  DBUG_PRINT("info",("keynr: %d  offset: %g  max_keynr: %d  nod: %d  flag: %d",
		     keynr,offset,max_keynr,nod_flag,flag));
  DBUG_RETURN((keynr+offset)/(max_keynr+1));
err:
  DBUG_PRINT("exit",("Error: %d",my_errno));
  DBUG_RETURN (-1.0);
}


	/* Get keynummer of current key and max number of keys in nod */

static uint _ni_keynr(N_INFO *info, register N_KEYDEF *keyinfo, uchar *page, uchar *keypos, uint *ret_max_key)
{
  uint nod_flag,keynr,max_key;
  uchar t_buff[N_MAX_KEY_BUFF],*end;

  end= page+getint(page);
  nod_flag=test_if_nod(page);
  page+=2+nod_flag;

  if (!(keyinfo->base.flag &
	(HA_PACK_KEY | HA_SPACE_PACK | HA_SPACE_PACK_USED)))
  {
    *ret_max_key= (uint) (end-page)/(keyinfo->base.keylength+nod_flag);
    return (uint) (keypos-page)/(keyinfo->base.keylength+nod_flag);
  }

  max_key=keynr=0;
  while (page < end)
  {
    t_buff[0]=0;			/* Don't move packed key */
    VOID((*keyinfo->get_key)(keyinfo,nod_flag,&page,t_buff));
    max_key++;
    if (page == keypos)
      keynr=max_key;
  }
  *ret_max_key=max_key;
  return(keynr);
}
