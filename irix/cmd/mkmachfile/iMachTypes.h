/*
 * Copyright 1994, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ifndef iMachTypes_h
#define iMachTypes_h

const int MACH_STRING_SIZE = 256;

struct Mach_Map  {
    int   class_;
    int   type_;
    int   state_;
    int   statemask_;
    int   attribute_;
    char  val_[MACH_STRING_SIZE];
};

struct Subgr_Map  {
    char   cpu_[MACH_STRING_SIZE];
    char   gfx_[MACH_STRING_SIZE];
    char   gfxboard_[MACH_STRING_SIZE];
    char   subgr_[MACH_STRING_SIZE];
    char   machine_[MACH_STRING_SIZE];
};


struct Arch_Map {
    char cpu_[MACH_STRING_SIZE];
    char arch_[MACH_STRING_SIZE];
    char mode_[MACH_STRING_SIZE];
};

enum iMachAttribute { NOMACH=-1, GFX=0, CPU=1, SUBGR=2, ARCH=3, VIDEO=4,
			MODE=5, TARGOS=6, DISTOS=7, MACHINE=8, SASHNAME=9 };

int 
parse_subgr(char* mach_line,Subgr_Map& map);

int 
parse_mach(char* mach_line,Mach_Map& map);

int 
parse_arch(char* arch_line,Arch_Map& map);

int getMachTables(Subgr_Map*& sList, Mach_Map*& mList, Arch_Map*& aList,
		  int& scount,       int& mCount,      int& acount);

extern const char* MACH_PATH;
#endif
