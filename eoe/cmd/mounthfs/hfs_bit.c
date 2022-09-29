/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.3 $"

#include <sys/types.h>
#include "hfs_bit.h"


/*********
 * bit_ffz	Find first zero.
 */
int bit_ffz(char *map, u_int max){

  int pos;
  for (pos=0;pos<max;pos++)
    if (ISCLR(map,pos))
      return pos;
  return -1;
}


/*********
 * bit_run	Return data on run of same bits.
 */
int bit_run(u_int pos, char *map, u_int mapsz, u_int *runsize){
  u_int p;
  *runsize=0;

  if (pos>= mapsz)
    return 0;

  if (ISSET(map,pos)){
    for (p=pos;ISSET(map,p) && p<mapsz;p++)
      *runsize +=1;
    return 1;
  }

  for (p=pos;!ISSET(map,p) && p<mapsz;p++)
    *runsize += 1;
  return 0;
}


/*************
 * bit_run_set	Set a run of bits
 */
void bit_run_set(u_int pos, char *map, u_int runsize){
  while (runsize--){
    SET(map,pos);
    pos++;
  }
}


/*************
 * bit_run_clr	Clear a run of bits
 */
void bit_run_clr(u_int pos, char *map, u_int runsize){
  while (runsize--){
    CLR(map,pos);
    pos++;
  }
}


/***************
 * bit_run_isset  Returns true if a run of bits is all set.
 */
int bit_run_isset(u_int pos, char *map, u_int runsize){
  while (runsize--){
    if (ISCLR(map,pos))
      return 0;
    pos++;
  }
  return 1;
}


/***************
 * bit_run_isclr  Returns true if a run of bits is all clr.
 */
int bit_run_isclr(u_int pos, char *map, u_int runsize){
  while (runsize--){
    if (ISSET(map,pos))
      return 0;
    pos++;
  }
  return 1;
}


