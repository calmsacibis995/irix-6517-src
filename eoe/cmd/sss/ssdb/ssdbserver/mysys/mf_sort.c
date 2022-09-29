/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* Sort of string pointers in string-order with radix or qsort */

#include "mysys_priv.h"
#include <m_string.h>

void my_string_ptr_sort(void *base, uint items, size_s size)
{
#if INT_MAX > 65536L
  uchar **ptr=0;

  if (size <= 20 && items >= 1000 &&
      (ptr= (uchar**) my_malloc(items*sizeof(char*),MYF(0))))
  {
    radixsort_for_str_ptr((uchar**) base,items,size,ptr);
    my_free((gptr) ptr,MYF(0));
  }
  else
#endif
  {
    if (size && items)
      qsort(base,items,sizeof(byte*),get_ptr_compare(size));
  }
}
