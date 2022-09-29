/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/************************************************************************
 *									*
 * io4_tdefs.h - io4 test definitions					*
 *									*
 ************************************************************************/

#ifndef _IO4_TDEFS_H_
#define _IO4_TDEFS_H_

#ifndef TRUE
#define TRUE	1
#endif

#ifndef FALSE
#define FALSE	0
#endif

struct  reg64bit_mask {
        int             reg_no;
        unsigned        bitmask0;
        unsigned        bitmask1;
};

struct  reg32bit_mask {
        int             reg_no;
        unsigned        bitmask0;
};

typedef struct reg32bit_mask    IO4cf_regs;
typedef struct reg64bit_mask    Fchip_regs;
typedef struct reg32bit_mask    Vmecc_regs;

typedef struct  struct_dev_regname {
    int   reg_no;
    char *name;
}Dev_Regname;

extern int io4_tslot, io4_tadap;

int io4_select( int do_adap, int argc, char** argv);
int io4_search(int atype, int *io4slot, int *anum);
int test_adapter(int argc, char **argv, int atype, int (*test_func)(int, int));

char *io4_regname(int regnum);

void save_io4config(int slot);
void restore_io4config(int slot);

#endif
