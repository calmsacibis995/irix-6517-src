/**************************************************************************/
/*									  */
/* 		 Copyright (C) 1989, Silicon Graphics, Inc.		  */
/*									  */
/*  These coded instructions, statements, and computer programs  contain  */
/*  unpublished  proprietary  information of Silicon Graphics, Inc., and  */
/*  are protected by Federal copyright law.  They  may  not be disclosed  */
/*  to  third  parties  or copied or duplicated in any form, in whole or  */
/*  in part, without the prior written consent of Silicon Graphics, Inc.  */
/*									  */
/**************************************************************************/
/*
 * The r4000 2.2 chip has a bug in double-word variable shifts.
 * These routines will convert variable shifts to immediate shifts,
 * which do work.  It is expected that these routines will only be
 * called in the 64-bit OS, which means they will be compiled into
 * double-word immediate shifts.
 */

#include <sgidefs.h>

__int64_t __dsrav( __int64_t opnd, __int64_t shamt )
{
  /* get the low-order 6-bits for the shift amount */
  switch ( shamt & 0x3f ) {
    case  0: return opnd >>  0;
    case  1: return opnd >>  1;
    case  2: return opnd >>  2;
    case  3: return opnd >>  3;
    case  4: return opnd >>  4;
    case  5: return opnd >>  5;
    case  6: return opnd >>  6;
    case  7: return opnd >>  7;
    case  8: return opnd >>  8;
    case  9: return opnd >>  9;

    case 10: return opnd >> 10;
    case 11: return opnd >> 11;
    case 12: return opnd >> 12;
    case 13: return opnd >> 13;
    case 14: return opnd >> 14;
    case 15: return opnd >> 15;
    case 16: return opnd >> 16;
    case 17: return opnd >> 17;
    case 18: return opnd >> 18;
    case 19: return opnd >> 19;

    case 20: return opnd >> 20;
    case 21: return opnd >> 21;
    case 22: return opnd >> 22;
    case 23: return opnd >> 23;
    case 24: return opnd >> 24;
    case 25: return opnd >> 25;
    case 26: return opnd >> 26;
    case 27: return opnd >> 27;
    case 28: return opnd >> 28;
    case 29: return opnd >> 29;

    case 30: return opnd >> 30;
    case 31: return opnd >> 31;
    case 32: return opnd >> 32;
    case 33: return opnd >> 33;
    case 34: return opnd >> 34;
    case 35: return opnd >> 35;
    case 36: return opnd >> 36;
    case 37: return opnd >> 37;
    case 38: return opnd >> 38;
    case 39: return opnd >> 39;

    case 40: return opnd >> 40;
    case 41: return opnd >> 41;
    case 42: return opnd >> 42;
    case 43: return opnd >> 43;
    case 44: return opnd >> 44;
    case 45: return opnd >> 45;
    case 46: return opnd >> 46;
    case 47: return opnd >> 47;
    case 48: return opnd >> 48;
    case 49: return opnd >> 49;

    case 50: return opnd >> 50;
    case 51: return opnd >> 51;
    case 52: return opnd >> 52;
    case 53: return opnd >> 53;
    case 54: return opnd >> 54;
    case 55: return opnd >> 55;
    case 56: return opnd >> 56;
    case 57: return opnd >> 57;
    case 58: return opnd >> 58;
    case 59: return opnd >> 59;

    case 60: return opnd >> 60;
    case 61: return opnd >> 61;
    case 62: return opnd >> 62;
    case 63: return opnd >> 63;
  }
  /* will never reach here, but return something nonetheless */
  return 0;
}

__uint64_t __dsrlv( __uint64_t opnd, __int64_t shamt )
{
  /* get the low-order 6-bits for the shift amount */
  switch ( shamt & 0x3f ) {
    case  0: return opnd >>  0;
    case  1: return opnd >>  1;
    case  2: return opnd >>  2;
    case  3: return opnd >>  3;
    case  4: return opnd >>  4;
    case  5: return opnd >>  5;
    case  6: return opnd >>  6;
    case  7: return opnd >>  7;
    case  8: return opnd >>  8;
    case  9: return opnd >>  9;

    case 10: return opnd >> 10;
    case 11: return opnd >> 11;
    case 12: return opnd >> 12;
    case 13: return opnd >> 13;
    case 14: return opnd >> 14;
    case 15: return opnd >> 15;
    case 16: return opnd >> 16;
    case 17: return opnd >> 17;
    case 18: return opnd >> 18;
    case 19: return opnd >> 19;

    case 20: return opnd >> 20;
    case 21: return opnd >> 21;
    case 22: return opnd >> 22;
    case 23: return opnd >> 23;
    case 24: return opnd >> 24;
    case 25: return opnd >> 25;
    case 26: return opnd >> 26;
    case 27: return opnd >> 27;
    case 28: return opnd >> 28;
    case 29: return opnd >> 29;

    case 30: return opnd >> 30;
    case 31: return opnd >> 31;
    case 32: return opnd >> 32;
    case 33: return opnd >> 33;
    case 34: return opnd >> 34;
    case 35: return opnd >> 35;
    case 36: return opnd >> 36;
    case 37: return opnd >> 37;
    case 38: return opnd >> 38;
    case 39: return opnd >> 39;

    case 40: return opnd >> 40;
    case 41: return opnd >> 41;
    case 42: return opnd >> 42;
    case 43: return opnd >> 43;
    case 44: return opnd >> 44;
    case 45: return opnd >> 45;
    case 46: return opnd >> 46;
    case 47: return opnd >> 47;
    case 48: return opnd >> 48;
    case 49: return opnd >> 49;

    case 50: return opnd >> 50;
    case 51: return opnd >> 51;
    case 52: return opnd >> 52;
    case 53: return opnd >> 53;
    case 54: return opnd >> 54;
    case 55: return opnd >> 55;
    case 56: return opnd >> 56;
    case 57: return opnd >> 57;
    case 58: return opnd >> 58;
    case 59: return opnd >> 59;

    case 60: return opnd >> 60;
    case 61: return opnd >> 61;
    case 62: return opnd >> 62;
    case 63: return opnd >> 63;
  }
  /* will never reach here, but return something nonetheless */
  return 0;
}

__int64_t __dsllv( __int64_t opnd, __int64_t shamt )
{
  /* get the low-order 6-bits for the shift amount */
  switch ( shamt & 0x3f ) {
    case  0: return opnd <<  0;
    case  1: return opnd <<  1;
    case  2: return opnd <<  2;
    case  3: return opnd <<  3;
    case  4: return opnd <<  4;
    case  5: return opnd <<  5;
    case  6: return opnd <<  6;
    case  7: return opnd <<  7;
    case  8: return opnd <<  8;
    case  9: return opnd <<  9;

    case 10: return opnd << 10;
    case 11: return opnd << 11;
    case 12: return opnd << 12;
    case 13: return opnd << 13;
    case 14: return opnd << 14;
    case 15: return opnd << 15;
    case 16: return opnd << 16;
    case 17: return opnd << 17;
    case 18: return opnd << 18;
    case 19: return opnd << 19;

    case 20: return opnd << 20;
    case 21: return opnd << 21;
    case 22: return opnd << 22;
    case 23: return opnd << 23;
    case 24: return opnd << 24;
    case 25: return opnd << 25;
    case 26: return opnd << 26;
    case 27: return opnd << 27;
    case 28: return opnd << 28;
    case 29: return opnd << 29;

    case 30: return opnd << 30;
    case 31: return opnd << 31;
    case 32: return opnd << 32;
    case 33: return opnd << 33;
    case 34: return opnd << 34;
    case 35: return opnd << 35;
    case 36: return opnd << 36;
    case 37: return opnd << 37;
    case 38: return opnd << 38;
    case 39: return opnd << 39;

    case 40: return opnd << 40;
    case 41: return opnd << 41;
    case 42: return opnd << 42;
    case 43: return opnd << 43;
    case 44: return opnd << 44;
    case 45: return opnd << 45;
    case 46: return opnd << 46;
    case 47: return opnd << 47;
    case 48: return opnd << 48;
    case 49: return opnd << 49;

    case 50: return opnd << 50;
    case 51: return opnd << 51;
    case 52: return opnd << 52;
    case 53: return opnd << 53;
    case 54: return opnd << 54;
    case 55: return opnd << 55;
    case 56: return opnd << 56;
    case 57: return opnd << 57;
    case 58: return opnd << 58;
    case 59: return opnd << 59;

    case 60: return opnd << 60;
    case 61: return opnd << 61;
    case 62: return opnd << 62;
    case 63: return opnd << 63;
  }
  /* will never reach here, but return something nonetheless */
  return 0;
}

