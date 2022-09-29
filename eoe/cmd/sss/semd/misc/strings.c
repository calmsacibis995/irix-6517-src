#include "common.h"
#include "strings_private.h"
#include <signal.h>

static __uint64_t stringtab_hash_key(char *s,int len)
{
  if(!s || !len)
    return 0;

  return fnv_hash((uchar_t *)s,len) % num_entries_hash;
}

static __uint64_t stringtab_add(char *s,int maxlen)
{
  int index,len=0;
  string_node_t *Pstr,*Pstr_temp;

  if(!s || maxlen<=0)
    return (__uint64_t)-1;

  if(!num_entries_hash)
    return (__uint64_t)-1;

  while(len <= maxlen && s[len++]);

  index=stringtab_hash_key(s,len-1);
  Pstr_temp=PPstring_hash[index];
  
  /*
   * Check for duplicate entries.
   */
  while(Pstr_temp && strncmp(Pstr_temp->str,s,len-1))
    Pstr_temp = Pstr_temp->Pnext_hash;
	
  if(Pstr_temp)		/* duplicate strings */
    {
      return (__uint64_t)-1;
    }

  Pstr=sem_mem_alloc_temp(sizeof(string_node_t));
  if(!Pstr)
    return (__uint64_t)-1;

  Pstr->Pnext_hash=PPstring_hash[index];
  PPstring_hash[index]=Pstr;

#if STRINGS_DEBUG
  fprintf(stderr,"str=%s,len=%d\n",s,len);
  if(Pstr->Pnext_hash)
    fprintf(stderr,"next:0x%x str=%s, refcnt=%d\n",
	    Pstr->Pnext_hash,
	    Pstr->Pnext_hash->str,
	    Pstr->Pnext_hash->refcnt);    
#endif

  Pstr->str=(char *)sem_mem_alloc_temp(len);
  if(!Pstr->str)
    {
      sem_mem_free(Pstr);
      return (__uint64_t)-1;
    }
  memcpy(Pstr->str,s,len-1);
  Pstr->str[len-1]=0;
  Pstr->refcnt=1;

#if STRINGS_DEBUG
  fprintf(stderr,"0x%x str=%s, refcnt=%d\n",Pstr,Pstr->str,Pstr->refcnt);    
#endif
  
  return 0;
}


static __uint64_t stringtab_delete(char *s,int maxlen)
{
  int index,len=0;
  string_node_t *Pstr_temp,**PPstr_temp;
  
  if(!s || maxlen<=0)
    return 0;

  if(!num_entries_hash)
    return (__uint64_t)-1;

  while(len <= maxlen && s[len++]);

  index=stringtab_hash_key(s,len-1);
  
  Pstr_temp=PPstring_hash[index];
  PPstr_temp=&PPstring_hash[index];
  /*
   * Check for string match.
   */
  while(Pstr_temp && strncmp(Pstr_temp->str,s,len-1))
    {
      PPstr_temp=&Pstr_temp->Pnext_hash;
      Pstr_temp = Pstr_temp->Pnext_hash;
    }
	
  if(!Pstr_temp)		/* no string with exact match. */
    return (__uint64_t)-1;

  *PPstr_temp=Pstr_temp->Pnext_hash;
  sem_mem_free(Pstr_temp->str);
  sem_mem_free(Pstr_temp);

  return 0;
}

static __uint64_t stringtab_find_hash(char *s,int maxlen,string_node_t **PPstr)
{
    int index,len=0;
    string_node_t *Pstr_temp;
    
    if(!s || maxlen<=0)
      return (__uint64_t)-1;
    
    if(!num_entries_hash)
      return (__uint64_t)-1;
    
    while(len <= maxlen && s[len++]);
    
    index=stringtab_hash_key(s,len-1);
    *PPstr=NULL;
    Pstr_temp=PPstring_hash[index];
    while(Pstr_temp && strncmp(Pstr_temp->str,s,len-1))
      {
	Pstr_temp = Pstr_temp->Pnext_hash;
      }

    if(Pstr_temp && !strncmp(Pstr_temp->str,s,len-1))
      {
	*PPstr=Pstr_temp;
	return 0;
      }

    return (__uint64_t)-1;
}

char *string_dup(char *s,int maxlen)
{
  string_node_t *Pstr;

  if(!s || !maxlen)
    return NULL;

  lock_strings();
  if(!stringtab_find_hash(s,maxlen,&Pstr))
    {
      Pstr->refcnt++;
      unlock_strings();
      return Pstr->str;
    }

  if(stringtab_add(s,maxlen))
    {
      unlock_strings();
      return NULL;
    }

  if(!stringtab_find_hash(s,maxlen,&Pstr))
    {
      Pstr->refcnt++;
      unlock_strings();
      return Pstr->str;
    }

  unlock_strings();
  return NULL;
}

__uint64_t string_free(char *s,int maxlen)
{
  string_node_t *Pstr=NULL;

  if(!s || !maxlen)
    return NULL;

  /*
   * Decrement reference count and return if refcnt > 1.
   */
  lock_strings();
  if(!stringtab_find_hash(s,maxlen,&Pstr) && Pstr &&
     --Pstr->refcnt)
    {
      unlock_strings();
      return 0;
    }
  if(!Pstr)
    {
      unlock_strings();
      return -1;
    }

  if(!stringtab_delete(s,maxlen))
    {
      unlock_strings();      
      return 0;
    }
  else
    {
      unlock_strings();
      return -1;
    }
}
  
__uint64_t stringtab_init()
{
  num_entries_hash=32;

  PPstring_hash=(string_node_t **)
    sem_mem_alloc_perm(sizeof(string_node_t)*num_entries_hash);
  if(!PPstring_hash)
    return (__uint64_t)-1;

  memset(PPstring_hash,0,sizeof(string_node_t)*num_entries_hash);
  return 0;
}

__uint64_t stringtab_clean()
{
  int index;
  string_node_t *Pstr,*Pstr_temp;

  if(!num_entries_hash)
    return (__uint64_t)-1;
  
  lock_strings();
  for(index=0;index<num_entries_hash;index++)
    {
      Pstr_temp=PPstring_hash[index];
      while(Pstr_temp)
	{
	  sem_mem_free(Pstr_temp->str);
	  Pstr = Pstr_temp->Pnext_hash;
	  sem_mem_free(Pstr_temp);
	  Pstr_temp=Pstr;
	}
    }
  unlock_strings();

  return 0;
}
