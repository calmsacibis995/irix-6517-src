/* Copyright (C) 1998 TcX AB & Monty Program KB & Detron HB &
   Alexis Mikhailov <root@medinf.chuvashia.su>

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

/* This file defines structures needed by udf functions */

#ifdef __GNUC__
#pragma interface
#endif

typedef struct st_udf_func
{
  char *name;
  int name_length;
  Item_result returns;
  char *dl;
  void *dlhandle;
  void *func;
  void *func_init;
  void *func_deinit;
  ulong usage_count;
} udf_func;

#ifdef HAVE_DLOPEN
void udf_init(void),udf_free(void);
udf_func *find_udf(const char *name, uint len=0,bool mark_used=0);
void free_udf(udf_func *udf);
int mysql_create_function(THD *thd,udf_func *udf);
int mysql_drop_function(THD *thd,const char *name);
#endif
