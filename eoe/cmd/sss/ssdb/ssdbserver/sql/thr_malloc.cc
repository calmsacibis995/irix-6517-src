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

/* Mallocs for used in threads */

#include "mysql_priv.h"

extern "C" {
  static void sql_alloc_error_handler(void)
  {
    current_thd->fatal_error=1; /* purecov: inspected */
  }
}

void init_sql_alloc(MEM_ROOT *mem_root,uint block_size)
{
  init_alloc_root(mem_root,block_size);
  mem_root->error_handler=sql_alloc_error_handler;
}


gptr sql_alloc(uint Size)
{
  MEM_ROOT *root=my_pthread_getspecific_ptr(MEM_ROOT*,THR_MALLOC);
  return alloc_root(root,Size);
}


gptr sql_calloc(uint size)
{
  gptr ptr;
  if ((ptr=sql_alloc(size)))
    bzero((char*) ptr,size);
  return ptr;
}


char *sql_strdup(const char *str)
{
  uint len=strlen(str)+1;
  char *pos;
  if ((pos=sql_alloc(len)))
    memcpy(pos,str,len);
  return pos;
}


char *sql_strmake(const char *str,uint len)
{
  char *pos;
  if ((pos=sql_alloc(len+1)))
  {
    memcpy(pos,str,len);
    pos[len]=0;
  }
  return pos;
}


gptr sql_memdup(const void *ptr,uint len)
{
  char *pos;
  if ((pos=sql_alloc(len)))
    memcpy(pos,ptr,len);
  return pos;
}

void sql_element_free(void *ptr __attribute__((unused)))
{} /* purecov: deadcode */
