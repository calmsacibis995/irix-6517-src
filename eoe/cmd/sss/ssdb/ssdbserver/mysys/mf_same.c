/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* Kopierar biblioteksstrukturen och extensionen fr}n ett filnamn */

#include "mysys_priv.h"
#include <m_string.h>

	/* Formaterar ett filnamn i avsende p} ett annat namn */
	/* Klarar {ven to = name */
	/* Denna funktion r|r inte p} utg}ngsnamnet */

my_string fn_same(my_string toname, const char *name, int flag)
{
  char dev[FN_REFLEN],*ext;
  DBUG_ENTER("fn_same");
  DBUG_PRINT("mfunkt",("to: %s  name: %s  flag: %d",toname,name,flag));

  if ((ext=strrchr(name+dirname_part(dev,name),FN_EXTCHAR)) == 0)
    ext="";

  DBUG_RETURN(fn_format(toname,toname,dev,ext,flag));
} /* fn_same */
