/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/*
  Code for handling strings with can grow dynamicly.
  Copyright Monty Program KB.
  By monty.
*/

#include "mysys_priv.h"
#include <m_string.h>

my_bool init_dynamic_string(DYNAMIC_STRING *str, my_string init_str,
			    uint init_alloc, uint alloc_increment)
{
  uint length;
  DBUG_ENTER("init_dynamic_string");

  if (!alloc_increment)
    alloc_increment=128;
  length=1;
  if (init_str && (length=strlen(init_str)+1) < init_alloc)
    init_alloc=((length+alloc_increment-1)/alloc_increment)*alloc_increment;
  if (!init_alloc)
    init_alloc=alloc_increment;

  if (!(str->str=(char*) my_malloc(init_alloc,MYF(MY_WME))))
    DBUG_RETURN(TRUE);
  str->length=length-1;
  if (init_str)
    memcpy(str->str,init_str,length);
  str->max_length=init_alloc;
  str->alloc_increment=alloc_increment;
  DBUG_RETURN(FALSE);
}

my_bool dynstr_set(DYNAMIC_STRING *str, my_string init_str)
{
  uint length;
  DBUG_ENTER("dynstr_set");

  if (init_str && (length=strlen(init_str)+1) > str->max_length)
  {
    str->max_length=((length+str->alloc_increment-1)/str->alloc_increment)*
      str->alloc_increment;
    if (!str->max_length)
      str->max_length=str->alloc_increment;
    if (!(str->str=(char*) my_realloc(str->str,str->max_length,MYF(MY_WME))))
      DBUG_RETURN(TRUE);
  }
  if (init_str)
  {
    str->length=length-1;
    memcpy(str->str,init_str,length);
  }
  else
    str->length=0;
  DBUG_RETURN(FALSE);
}

my_bool dynstr_realloc(DYNAMIC_STRING *str, ulong additional_size)
{
  DBUG_ENTER("dynstr_realloc");

  if (!additional_size) DBUG_RETURN(FALSE);
  if (str->length + additional_size > str->max_length)
  {
    str->max_length=((str->length + additional_size+str->alloc_increment-1)/
		     str->alloc_increment)*str->alloc_increment;
    if (!(str->str=(char*) my_realloc(str->str,str->max_length,MYF(MY_WME))))
      DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


my_bool dynstr_append(DYNAMIC_STRING *str, my_string append)
{
  char *new_ptr;
  uint length=(uint) strlen(append)+1;
  if (str->length+length > str->max_length)
  {
    uint new_length=(str->length+length+str->alloc_increment-1)/
      str->alloc_increment;
    new_length*=str->alloc_increment;
    if (!(new_ptr=(char*) my_realloc(str->str,new_length,MYF(MY_WME))))
      return TRUE;
    str->str=new_ptr;
    str->max_length=new_length;
  }
  memcpy(str->str + str->length,append,length);
  str->length+=length-1;
  return FALSE;
}

void dynstr_free(DYNAMIC_STRING *str)
{
  if (str->str)
  {
    my_free(str->str,MYF(MY_WME));
    str->str=0;
  }
}
