#pragma once

/**************************************************************************
 *                                                                        *
 *       Copyright (C) 1990, Silicon Graphics, Inc.                       *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*  this is part of the showcase code.. so perhaps has to be linked 
 * from there.. got this file from
 * /hosts/bonnie/disks/lv10/showcase_dev/src/help.DELETED.DELETED
 * very klugy..  - laks 250995
 */ 

#ifdef _LANGUAGE_C_PLUS_PLUS
extern "C" {
#endif  /* CPLUSPLUS */

long HELPdisplay(char *filename);
void HELPposition(long x1, long x2, long y1, long y2);
void HELPclose();

#ifdef _LANGUAGE_C_PLUS_PLUS
};
#endif  /* CPLUSPLUS */
