/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/*
  This functions is handle keyblock cacheing for NISAM, MISAM and PISAM
  databases.
  One cache can handle many files. Every different blocksize has it owns
  set of buffers that are allocated from block_mem.
  init_key_cache() should be used to init cache handler.
 */

#include "mysys_priv.h"
#include "my_static.h"
#include <m_string.h>
#include <errno.h>

#if defined(MSDOS) && !defined(M_IC80386)
	/* We nead much memory */
#undef my_malloc_lock
#undef my_free_lock
#define my_malloc_lock(A,B)	halloc((long) (A/IO_SIZE),IO_SIZE)
#define my_free_lock(A,B)	hfree(A)
#endif

/* size of map to be used to find changed files */

#define CHANGED_BLOCKS_HASH	32		/* Must be power of 2 */
#define CHANGED_BLOCKS_MASK	(CHANGED_BLOCKS_HASH-1)

static uint find_next_bigger_power(uint value);
static SEC_LINK *find_key_block(int file,my_off_t filepos,uint block_size,
				int *error);

	/* static variables in this file */
static SEC_LINK *_my_block_root,**_my_hash_root,
		*_my_used_first[MAX_BLOCK_TYPES],*_my_used_last[MAX_BLOCK_TYPES];
static int	_my_disk_blocks;
static uint	_my_disk_blocks_used, _my_hash_blocks;
int		_my_blocks_used,_my_blocks_changed;
#ifndef DBUG_OFF
static my_bool	_my_printed;
#endif
ulong		_my_cache_w_requests,_my_cache_write,_my_cache_r_requests,
		_my_cache_read;
static byte	HUGE_PTR *_my_block_mem;
static SEC_LINK *changed_blocks[CHANGED_BLOCKS_HASH];
static SEC_LINK *file_blocks[CHANGED_BLOCKS_HASH];


	/* Init of disk_buffert */
	/* Returns blocks in use */
	/* ARGSUSED */

int init_key_cache(ulong use_mem, ulong leave_this_much_mem)
{
  uint blocks,length;
  byte *extra_mem=0;
  DBUG_ENTER("init_key_cache");

  if (key_cache_inited && _my_disk_blocks > 0)
  {
    DBUG_PRINT("warning",("key cache allready in use")); /* purecov: inspected */
    DBUG_RETURN(0); /* purecov: inspected */
  }
  if (! key_cache_inited)
  {
    key_cache_inited=TRUE;
    _my_disk_blocks= -1;
#ifndef DBUG_OFF
    _my_printed=0;
#endif
  }

  blocks= (uint) (use_mem/(sizeof(SEC_LINK)+sizeof(SEC_LINK*)*5/4+MIN_KEYBLOCK));
  blocks/=MAX_BLOCK_TYPES;
  blocks*=MAX_BLOCK_TYPES;

  if (blocks >= 8 && _my_disk_blocks < 0)
  {
#if !defined(HAVE_ALLOCA) && !defined(THREAD)
    if ((extra_mem=my_malloc((uint) leave_this_much_mem,MYF(0))) == 0)
      goto err;
#endif
    for (;;)
    {
      if ((_my_hash_blocks=find_next_bigger_power((uint) blocks)) < blocks*5/4)
	_my_hash_blocks<<=1;
      while ((length=(uint) blocks*sizeof(SEC_LINK)+
	      sizeof(SEC_LINK*)*_my_hash_blocks)+(ulong) blocks*MIN_KEYBLOCK >
	     use_mem)
	blocks--;
      if ((_my_block_mem=my_malloc_lock((ulong) blocks*MIN_KEYBLOCK,MYF(0))))
      {
	if ((_my_block_root=(SEC_LINK*) my_malloc((uint) length,MYF(0))) != 0)
	  break;
	my_free_lock(_my_block_mem,MYF(0));
      }
      if (blocks < 8)
	goto err;
      blocks=blocks/4*3/MAX_BLOCK_TYPES;
      blocks*=MAX_BLOCK_TYPES;
    }
    _my_disk_blocks=(int) blocks;
    _my_hash_root=	(SEC_LINK**) (_my_block_root+blocks);
    bzero((byte*) _my_hash_root,_my_hash_blocks*sizeof(SEC_LINK*));
    bzero((byte*) _my_used_first,MAX_BLOCK_TYPES*sizeof(SEC_LINK*));
    bzero((byte*) _my_used_last,MAX_BLOCK_TYPES*sizeof(SEC_LINK*));
    _my_blocks_used=_my_disk_blocks_used=_my_blocks_changed=0;
    _my_cache_w_requests=_my_cache_r_requests=_my_cache_read=_my_cache_write=0;
    DBUG_PRINT("exit",("disk_blocks: %d  block_root: %lx  _my_hash_blocks: %d  hash_root: %lx",
		       _my_disk_blocks,_my_block_root,_my_hash_blocks,
		       _my_hash_root));
#if !defined(HAVE_ALLOCA) && !defined(THREAD)
    my_free(extra_mem,MYF(0));
#endif
  }
  bzero((gptr) changed_blocks,sizeof(changed_blocks[0])*CHANGED_BLOCKS_HASH);
  bzero((gptr) file_blocks,sizeof(file_blocks[0])*CHANGED_BLOCKS_HASH);
  DBUG_RETURN((int) blocks);
err:
  if (extra_mem) /* purecov: inspected */
    my_free(extra_mem,MYF(0));
  my_errno=ENOMEM;
  DBUG_RETURN(0);
} /* init_key_cache */


	/* Remove key_cache from memory */

void end_key_cache(void)
{
  DBUG_ENTER("end_key_cache");
  if (! _my_blocks_changed)
  {
    if (_my_disk_blocks > 0)
    {
      my_free_lock((gptr) _my_block_mem,MYF(0));
      my_free((gptr) _my_block_root,MYF(0));
      _my_disk_blocks= -1;
    }
  }
  DBUG_PRINT("status",
	     ("used: %d  changed: %d  w_requests: %ld  writes: %ld  r_requests: %ld  reads: %ld",
	      _my_blocks_used,_my_blocks_changed,_my_cache_w_requests,
	      _my_cache_write,_my_cache_r_requests,_my_cache_read));
  DBUG_VOID_RETURN;
} /* end_key_cache */


static uint find_next_bigger_power(uint value)
{
  uint old_value=1;
  while (value)
  {
    old_value=value;
    value&= value-1;
  }
  return (old_value << 1);
}

inline void link_into_file_blocks(SEC_LINK *next, int file)
{
  reg1 SEC_LINK **ptr= &file_blocks[(uint) file & CHANGED_BLOCKS_MASK];
  next->prev_changed= ptr;
  if (next->next_changed= *ptr)
    (*ptr)->prev_changed= &next->next_changed;
  *ptr=next;
}


inline void relink_into_file_blocks(SEC_LINK *next, int file)
{
  reg1 SEC_LINK **ptr= &file_blocks[(uint) file & CHANGED_BLOCKS_MASK];
  if (next->next_changed)
    next->next_changed->prev_changed=next->prev_changed;
  *next->prev_changed=next->next_changed;
  next->prev_changed= ptr;
  if (next->next_changed= *ptr)
    (*ptr)->prev_changed= &next->next_changed;
  *ptr=next;
}

inline void link_changed_to_file(SEC_LINK *next,int file)
{
  reg1 SEC_LINK **ptr= &file_blocks[(uint) file & CHANGED_BLOCKS_MASK];
  if (next->next_changed)
    next->next_changed->prev_changed=next->prev_changed;
  *next->prev_changed=next->next_changed;
  if (next->next_changed= *ptr)
    (*ptr)->prev_changed= &next->next_changed;
  *ptr=next;
  next->prev_changed= ptr;
  next->changed=0;
  _my_blocks_changed--;
}

inline void link_file_to_changed(SEC_LINK *next)
{
  reg1 SEC_LINK **ptr= &changed_blocks[(uint) next->file & CHANGED_BLOCKS_MASK];
  if (next->next_changed)
    next->next_changed->prev_changed=next->prev_changed;
  *next->prev_changed=next->next_changed;
  if (next->next_changed= *ptr)
    (*ptr)->prev_changed= &next->next_changed;
  *ptr=next;
  next->prev_changed= ptr;
  next->changed=1;
  _my_blocks_changed++;
}


#ifndef DBUG_OFF
#define DBUG_OFF				/* This should work */
#endif

#ifndef DBUG_OFF
static void test_key_cache(void);
#endif


	/*
	** read a key_buffer
	** filepos Must point at a even MIN_KEYBLOCK block
	** if return_buffer is set then the intern buffer is returned
	** Returns adress to where data is read
	*/

byte *key_cache_read(File file, my_off_t filepos, byte *buff, uint length,
		     uint block_length, int return_buffer)
{
  reg1 SEC_LINK *next;
  int error=0;
  pthread_mutex_lock(&THR_LOCK_keycache);
  if (_my_disk_blocks > 0)
  {						/* We have key_cacheing */
    if ((next=find_key_block(file,filepos,block_length/MIN_KEYBLOCK-1,&error))
	!= 0)
    {
      if (error)
      {						/* Didn't find it in cache */
	next->size=(uint16) block_length;
	if (my_pread(file,next->buffer,length,filepos,MYF(MY_NABP)))
	{
	  pthread_mutex_unlock(&THR_LOCK_keycache);
	  return((byte*) 0);
	}
	_my_cache_read++;
      }
      _my_cache_r_requests++;
      pthread_mutex_unlock(&THR_LOCK_keycache);
#ifndef THREAD				/* buffer may be used a long time */
      if (return_buffer)
	return (next->buffer);
#endif
      if (! (length & 511))
	bmove512(buff,next->buffer,length);
      else
	memcpy(buff,next->buffer,(size_t) length);
      return(buff);
    }
    else if (error)
      return((byte*) 0);
  }
  if (my_pread(file,(byte*) buff,length,filepos,MYF(MY_NABP)))
    error=1;
  pthread_mutex_unlock(&THR_LOCK_keycache);
  return (error ? (byte*) 0 : buff);
} /* key_cache_read */


	/* write a key_buffer */
	/* We don't have to use pwrite because of write locking */
	/* buff must point at a even MIN_KEYBLOCK block */

int key_cache_write(File file, my_off_t filepos, byte *buff, uint length,
		    uint block_length, int dont_write)
{
  reg1 SEC_LINK *next;
  int error=0;

  if (!dont_write)
  {						/* Forced write of buffer */
    if (my_pwrite(file,buff,length,filepos,MYF(MY_NABP | MY_WAIT_IF_FULL)))
      return(1);
  }

  pthread_mutex_lock(&THR_LOCK_keycache);
  if (_my_disk_blocks > 0)
  {						/* We have key_cacheing */
    if ((next=find_key_block(file,filepos,block_length/MIN_KEYBLOCK-1,&error)))
    {
      next->size=(uint16) block_length;
      if (!dont_write)
      {
	if (next->changed)
	  link_changed_to_file(next,next->file);
      }
      else if (!next->changed)
	link_file_to_changed(next);
      if (!(length & 511))
	bmove512(next->buffer,buff,length);
      else
	memcpy(next->buffer,buff,(size_t) length);
      _my_cache_w_requests++;
      error=0;
      goto end;
    }
    else if (error)
      goto end;
  }

  if (dont_write)
  {						/* We must write, no cache */
    if (my_pwrite(file,(byte*) buff,length,filepos,
		  MYF(MY_NABP | MY_WAIT_IF_FULL)))
      error=1;
  }
end:
  pthread_mutex_unlock(&THR_LOCK_keycache);
  return(error);
} /* key_cache_write */


	/* Find block in cache */
	/* IF found sector and error is set then next->changed is cleared */

static SEC_LINK *find_key_block(int file, my_off_t filepos, uint block_size,
				int *error)
{
  reg1 SEC_LINK *next,**start;

  *error=0;
  next= *(start= &_my_hash_root[((ulong) (filepos/MIN_KEYBLOCK)+(ulong) file) &
				(_my_hash_blocks-1)]);
  while (next && (next->diskpos != filepos || next->file != file))
    next= next->next_hash;

  if (next)
  {						/* Found block */
    if (next != _my_used_last[block_size])
    {						/* Relink used-chain */
      if (next == _my_used_first[block_size])
	_my_used_first[block_size]=next->next_used;
      else
      {
	next->prev_used->next_used = next->next_used;
	next->next_used->prev_used = next->prev_used;
      }
      next->prev_used=_my_used_last[block_size];
      _my_used_last[block_size]->next_used=next;
    }
  }
  else
  {						/* New block */
    if (_my_disk_blocks_used+block_size+1 <= (uint) _my_disk_blocks)
    {						/* There are unused blocks */
      next= &_my_block_root[_my_blocks_used++]; /* Link in hash-chain */
      next->buffer=ADD_TO_PTR(_my_block_mem,
			      (ulong) _my_disk_blocks_used*MIN_KEYBLOCK,byte*);
      /* link first in file_blocks */
      next->changed=0;
      link_into_file_blocks(next,file);
      _my_disk_blocks_used+=block_size+1;
      if (!_my_used_first[block_size])
	_my_used_first[block_size]=next;
      if (_my_used_last[block_size])
	_my_used_last[block_size]->next_used=next; /* Last in used-chain */
    }
    else
    {						/* Reuse old block */
      if ((next=_my_used_first[block_size]) == 0)
	return ((SEC_LINK*) 0);			/* No room for cacheing !!! */
      if (next->changed)
      {
	if (my_pwrite(next->file,next->buffer,(uint) next->size,next->diskpos,
		      MYF(MY_NABP | MY_WAIT_IF_FULL)))
	{
	  *error=1;
	  return((SEC_LINK*) 0);
	}
	_my_cache_write++;
	link_changed_to_file(next,file);
      }
      else
      {
	if (next->file == -1)
	  link_into_file_blocks(next,file);
	else
	  relink_into_file_blocks(next,file);
      }
      if (next->prev_hash)			/* If in hash-link */
	if ((*next->prev_hash=next->next_hash) != 0) /* Remove from link */
	  next->next_hash->prev_hash= next->prev_hash;

      _my_used_last[block_size]->next_used=next;
      _my_used_first[block_size]=next->next_used;
    }
    if (*start)					/* Link in first in h.-chain */
      (*start)->prev_hash= &next->next_hash;
    next->next_hash= *start; next->prev_hash=start; *start=next;
    next->prev_used=_my_used_last[block_size];
    next->file=file;
    next->diskpos=filepos;
    next->block_size=(int7) block_size;
    *error=1;					/* Block wasn't in memory */
  }
  _my_used_last[block_size]=next;
#ifndef DBUG_OFF
  DBUG_EXECUTE("exec",test_key_cache(););
#endif
  return next;
} /* find_key_block */


static void free_block(SEC_LINK *used)
{
  uint block_size=used->block_size;
  used->file= -1;
  if (used != _my_used_first[block_size]) /* Relink used-chain */
  {
    if (used == _my_used_last[block_size])
      _my_used_last[block_size]=used->prev_used;
    else
    {
      used->prev_used->next_used = used->next_used;
      used->next_used->prev_used = used->prev_used;
    }
    used->next_used=_my_used_first[block_size];
    used->next_used->prev_used=used;
    _my_used_first[block_size]=used;
  }
  if ((*used->prev_hash=used->next_hash))	/* Relink hash-chain */
    used->next_hash->prev_hash= used->prev_hash;
  if (used->next_changed)		/* Relink changed/file list */
    used->next_changed->prev_changed=used->prev_changed;
  *used->prev_changed=used->next_changed;
  used->prev_hash=0; used->next_hash=0;		/* Safety */
}

	/* Flush all changed blocks to disk. Free used blocks if requested */

int flush_key_blocks(File file, pbool free_used_blocks)
{
  DBUG_ENTER("flush_key_blocks");
  DBUG_PRINT("enter",("file: %d  blocks_used: %d  blocks_changed: %d",
		      file,_my_blocks_used,_my_blocks_changed));

  pthread_mutex_lock(&THR_LOCK_keycache);
  if (_my_disk_blocks > 0 || !(my_disable_flush_key_blocks || free_used_blocks))
  {
    SEC_LINK *used,*next;
    for (used=changed_blocks[(uint) file & CHANGED_BLOCKS_MASK];
	 used ;
	 used=next)
    {
      next=used->next_changed;
      if (used->file == file)
      {
	if (my_pwrite(file,used->buffer,(uint) used->size,used->diskpos,
		      MYF(MY_NABP | MY_WAIT_IF_FULL)))
	  goto err;
	_my_cache_write++;
	if (free_used_blocks)
	{
	  used->changed=0;
	  _my_blocks_changed--;
	  free_block(used);
	}
	else
	  link_changed_to_file(used,file);
      }
    }
    if (free_used_blocks)			/* This happens very seldom */
    {
      for (used=file_blocks[(uint) file & CHANGED_BLOCKS_MASK];
	   used ;
	   used=next)
      {
	next=used->next_changed;
	if (used->file == file)
	  free_block(used);
      }
    }
  }
#ifndef DBUG_OFF
  DBUG_EXECUTE("exec",test_key_cache(););
#endif
  pthread_mutex_unlock(&THR_LOCK_keycache);
  DBUG_RETURN(0);
err:
  pthread_mutex_unlock(&THR_LOCK_keycache);	/* purecov: inspected */
  DBUG_RETURN(1);				/* purecov: inspected */
} /* flush_key_blocks */



#ifndef DBUG_OFF

	/* Test if disk-cachee is ok */

static void test_key_cache()
{
  reg1 uint i,found,error,changed;
  SEC_LINK *pos,**prev;

  found=error=0;
  for (i= 0 ; i < _my_hash_blocks ; i++)
  {

    for (pos= *(prev= &_my_hash_root[i]) ;
	 pos && found < _my_blocks_used+2 ;
	 found++, pos= *(prev= &pos->next_hash))
    {
      if (prev != pos->prev_hash)
      {
	error=1;
	DBUG_PRINT("error",
		   ("hash: %d  pos: %lx  : prev: %lx  !=  pos->prev: %lx",
		    i,pos,prev,pos->prev_hash));
      }

      if (((pos->diskpos/MIN_KEYBLOCK)+pos->file)%_my_hash_blocks != i)
      {
	DBUG_PRINT("error",("hash: %d  pos: %lx  : Wrong disk_buffer %ld",
			    i,pos,pos->diskpos));
	error=1;
      }
    }
  }
  if (found > _my_blocks_used)
  {
    DBUG_PRINT("error",("Found too many hash_pointers"));
    error=1;
  }
  if (error && !_my_printed)
  {						/* Write all hash-pointers */
    _my_printed=1;
    for (i= 0 ; i < _my_hash_blocks ; i++)
    {
      DBUG_PRINT("loop",("hash: %d  _my_hash_root: %lx",i,&_my_hash_root[i]));
      pos= _my_hash_root[i]; found=0;
      while (pos && found < 10)
      {
	DBUG_PRINT("loop",("pos: %lx  prev: %lx  next: %lx  file: %d  disk_buffer: %ld", pos,pos->prev_hash,pos->next_hash,pos->file,pos->diskpos));
	found++; pos= pos->next_hash;
      }
    }
  }

  found=changed=0;
  for (i=0 ; i < MAX_BLOCK_TYPES ; i++)
  {
    if ((pos=_my_used_first[i]))
    {
      while (pos != _my_used_last[i] && found < _my_blocks_used+2)
      {
	found++;
	if (pos->changed)
	  changed++;
	if (pos->next_used->prev_used != pos || pos->block_size != i)
	  DBUG_PRINT("error",("pos: %lx  next_used: %lx  next_used->prev: %lx",
			      pos,pos->next_used,pos->next_used->prev_hash));
	pos=pos->next_used;
      }
      found++;
      if (pos->changed)
	changed++;
    }
  }
  if (found != _my_blocks_used)
    DBUG_PRINT("error",("Found %d of %d keyblocks",found,_my_blocks_used));

  for (i= 0 ; i < CHANGED_BLOCKS_HASH ; i++)
  {
    found=0;
    prev= &changed_blocks[i];
    for (pos= *prev ;  pos && found < _my_blocks_used+2; pos=pos->next_changed)
    {
      found++;
      if (pos->prev_changed != prev)
	DBUG_PRINT("error",("changed_block list %d doesn't point backwards properly",i));
      prev= &pos->next_changed;
      if (((uint) pos->file & CHANGED_BLOCKS_MASK) != i)
	DBUG_PRINT("error",("Wrong file %d in changed blocks: %d",pos->file,i));
      changed--;
    }
    if (pos)
      DBUG_PRINT("error",("changed_blocks %d has recursive link",i));

    found=0;
    prev= &file_blocks[i];
    for (pos= *prev ;  pos && found < _my_blocks_used+2; pos=pos->next_changed)
    {
      found++;
      if (pos->prev_changed != prev)
	DBUG_PRINT("error",("file_block list %d doesn't point backwards properly",i));
      prev= &pos->next_changed;
      if (((uint) pos->file & CHANGED_BLOCKS_MASK) != i)
	DBUG_PRINT("error",("Wrong file %d in file_blocks: %d",pos->file,i));
    }
    if (pos)
      DBUG_PRINT("error",("File_blocks %d has recursive link",i));
  }
  if (changed != 0)
    DBUG_PRINT("error",("Found %d blocks that wasn't in changed blocks",
			changed));
  return;
} /* test_key_cache */
#endif
