/*
 * fpu/fpregs.c
 *
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

#ident "$Revision: 1.6 $"

/*
 *					
 *	Floating Point Exerciser - write/read fpu registers
 *					
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/fpu.h>
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

extern void SetFP0(ulong);
extern void SetFP1(ulong);
extern void SetFP2(ulong);
extern void SetFP3(ulong);
extern void SetFP4(ulong);
extern void SetFP5(ulong);
extern void SetFP6(ulong);
extern void SetFP7(ulong);
extern void SetFP8(ulong);
extern void SetFP9(ulong);
extern void SetFP10(ulong);
extern void SetFP11(ulong);
extern void SetFP12(ulong);
extern void SetFP13(ulong);
extern void SetFP14(ulong);
extern void SetFP15(ulong);
extern void SetFP16(ulong);
extern void SetFP17(ulong);
extern void SetFP18(ulong);
extern void SetFP19(ulong);
extern void SetFP20(ulong);
extern void SetFP21(ulong);
extern void SetFP22(ulong);
extern void SetFP23(ulong);
extern void SetFP24(ulong);
extern void SetFP25(ulong);
extern void SetFP26(ulong);
extern void SetFP27(ulong);
extern void SetFP28(ulong);
extern void SetFP29(ulong);
extern void SetFP30(ulong);
extern void SetFP31(ulong);

extern int GetFP31(void);
extern int GetFP30(void);
extern int GetFP29(void);
extern int GetFP28(void);
extern int GetFP27(void);
extern int GetFP26(void);
extern int GetFP25(void);
extern int GetFP24(void);
extern int GetFP23(void);
extern int GetFP22(void);
extern int GetFP21(void);
extern int GetFP20(void);
extern int GetFP19(void);
extern int GetFP18(void);
extern int GetFP17(void);
extern int GetFP16(void);
extern int GetFP15(void);
extern int GetFP14(void);
extern int GetFP13(void);
extern int GetFP12(void);
extern int GetFP11(void);
extern int GetFP10(void);
extern int GetFP9(void);
extern int GetFP8(void);
extern int GetFP7(void);
extern int GetFP6(void);
extern int GetFP5(void);
extern int GetFP4(void);
extern int GetFP3(void);
extern int GetFP2(void);
extern int GetFP1(void);
extern int GetFP0(void);

int
fpregs()
{
    register int fail = 0;
    register ulong status;

    /* enable cache and fpu - cache ecc errors enabled */
    status = GetSR();
#ifdef TFP
    status |= SR_CU0|SR_CU1|SR_FR|SR_KX;
#else
    status |= SR_CU0|SR_CU1;
#endif
    SetSR(status);

    /* clear cause register */
    set_cause(0);

    /* clear fpu status register */
    SetFPSR(0);

    /* write and read fpu registers */
    SetFP0(0);
    SetFP1(1);
    SetFP2(2);
    SetFP3(3);
    SetFP4(4);
    SetFP5(5);
    SetFP6(6);
    SetFP7(7);
    SetFP8(8);
    SetFP9(9);
    SetFP10(10);
    SetFP11(11);
    SetFP12(12);
    SetFP13(13);
    SetFP14(14);
    SetFP15(15);
    SetFP16(16);
    SetFP17(17);
    SetFP18(18);
    SetFP19(19);
    SetFP20(20);
    SetFP21(21);
    SetFP22(22);
    SetFP23(23);
    SetFP24(24);
    SetFP25(25);
    SetFP26(26);
    SetFP27(27);
    SetFP28(28);
    SetFP29(29);
    SetFP30(30);
    SetFP31(31);
    if (GetFP0() != 0){
	msg_printf(DBG, "fpregs FP0 failed, expected 0, got 0x%x\n",
					GetFP0());
	fail = 1;
    }
    if (GetFP1() != 1){
	msg_printf(DBG, "fpregs FP1 failed, expected 1, got 0x%x\n",
					GetFP1());
	fail = 1;
    }
    if (GetFP2() != 2){
	msg_printf(DBG, "fpregs FP2 failed, expected 2, got 0x%x\n",
					GetFP2());
	fail = 1;
    }
    if (GetFP3() != 3){
	msg_printf(DBG, "fpregs FP3 failed, expected 3, got 0x%x\n",
					GetFP3());
	fail = 1;
    }
    if (GetFP4() != 4){
	msg_printf(DBG, "fpregs FP4 failed, expected 4, got 0x%x\n",
					GetFP4());
	fail = 1;
    }
    if (GetFP5() != 5){
	msg_printf(DBG, "fpregs FP5 failed, expected 5, got 0x%x\n",
					GetFP5());
	fail = 1;
    }
    if (GetFP6() != 6){
	msg_printf(DBG, "fpregs FP6 failed, expected 6, got 0x%x\n",
					GetFP6());
	fail = 1;
    }
    if (GetFP7() != 7){
	msg_printf(DBG, "fpregs FP7 failed, expected 7, got 0x%x\n",
					GetFP7());
	fail = 1;
    }
    if (GetFP8() != 8){
	msg_printf(DBG, "fpregs FP8 failed, expected 8, got 0x%x\n",
					GetFP8());
	fail = 1;
    }
    if (GetFP9() != 9){
	msg_printf(DBG, "fpregs FP9 failed, expected 9, got 0x%x\n",
					GetFP9());
	fail = 1;
    }
    if (GetFP10() != 10){
	msg_printf(DBG, "fpregs FP10 failed, expected 10, got 0x%x\n",
					GetFP10());
	fail = 1;
    }
    if (GetFP11() != 11){
	msg_printf(DBG, "fpregs FP11 failed, expected 11, got 0x%x\n",
					GetFP11());
	fail = 1;
    }
    if (GetFP12() != 12){
	msg_printf(DBG, "fpregs FP12 failed, expected 12, got 0x%x\n",
					GetFP12());
	fail = 1;
    }
    if (GetFP13() != 13){
	msg_printf(DBG, "fpregs FP13 failed, expected 13, got 0x%x\n",
					GetFP13());
	fail = 1;
    }
    if (GetFP14() != 14){
	msg_printf(DBG, "fpregs FP14 failed, expected 14, got 0x%x\n",
					GetFP14());
	fail = 1;
    }
    if (GetFP15() != 15){
	msg_printf(DBG, "fpregs FP15 failed, expected 15, got 0x%x\n",
					GetFP15());
	fail = 1;
    }
    if (GetFP16() != 16){
	msg_printf(DBG, "fpregs FP16 failed, expected 16, got 0x%x\n",
					GetFP16());
	fail = 1;
    }
    if (GetFP17() != 17){
	msg_printf(DBG, "fpregs FP17 failed, expected 17, got 0x%x\n",
					GetFP17());
	fail = 1;
    }
    if (GetFP18() != 18){
	msg_printf(DBG, "fpregs FP18 failed, expected 18, got 0x%x\n",
					GetFP18());
	fail = 1;
    }
    if (GetFP19() != 19){
	msg_printf(DBG, "fpregs FP19 failed, expected 19, got 0x%x\n",
					GetFP19());
	fail = 1;
    }
    if (GetFP20() != 20){
	msg_printf(DBG, "fpregs FP20 failed, expected 20, got 0x%x\n",
					GetFP20());
	fail = 1;
    }
    if (GetFP21() != 21){
	msg_printf(DBG, "fpregs FP21 failed, expected 21, got 0x%x\n",
					GetFP21());
	fail = 1;
    }
    if (GetFP22() != 22){
	msg_printf(DBG, "fpregs FP22 failed, expected 22, got 0x%x\n",
					GetFP22());
	fail = 1;
    }
    if (GetFP23() != 23){
	msg_printf(DBG, "fpregs FP23 failed, expected 23, got 0x%x\n",
					GetFP23());
	fail = 1;
    }
    if (GetFP24() != 24){
	msg_printf(DBG, "fpregs FP24 failed, expected 24, got 0x%x\n",
					GetFP24());
	fail = 1;
    }
    if (GetFP25() != 25){
	msg_printf(DBG, "fpregs FP25 failed, expected 25, got 0x%x\n",
					GetFP25());
	fail = 1;
    }
    if (GetFP26() != 26){
	msg_printf(DBG, "fpregs FP26 failed, expected 26, got 0x%x\n",
					GetFP26());
	fail = 1;
    }
    if (GetFP27() != 27){
	msg_printf(DBG, "fpregs FP27 failed, expected 27, got 0x%x\n",
					GetFP27());
	fail = 1;
    }
    if (GetFP28() != 28){
	msg_printf(DBG, "fpregs FP28 failed, expected 28, got 0x%x\n",
					GetFP28());
	fail = 1;
    }
    if (GetFP29() != 29){
	msg_printf(DBG, "fpregs FP29 failed, expected 29, got 0x%x\n",
					GetFP29());
	fail = 1;
    }
    if (GetFP30() != 30){
	msg_printf(DBG, "fpregs FP30 failed, expected 30, got 0x%x\n",
					GetFP30());
	fail = 1;
    }
    if (GetFP31() != 31){
	msg_printf(DBG, "fpregs FP31 failed, expected 31, got 0x%x\n",
					GetFP31());
	fail = 1;
    }

    SetFP0(~0);
    SetFP1(~1);
    SetFP2(~2);
    SetFP3(~3);
    SetFP4(~4);
    SetFP5(~5);
    SetFP6(~6);
    SetFP7(~7);
    SetFP8(~8);
    SetFP9(~9);
    SetFP10(~10);
    SetFP11(~11);
    SetFP12(~12);
    SetFP13(~13);
    SetFP14(~14);
    SetFP15(~15);
    SetFP16(~16);
    SetFP17(~17);
    SetFP18(~18);
    SetFP19(~19);
    SetFP20(~20);
    SetFP21(~21);
    SetFP22(~22);
    SetFP23(~23);
    SetFP24(~24);
    SetFP25(~25);
    SetFP26(~26);
    SetFP27(~27);
    SetFP28(~28);
    SetFP29(~29);
    SetFP30(~30);
    SetFP31(~31);
    if (GetFP0() != ~0){
	msg_printf(DBG, "fpregs FP0 failed, expected 0x%x, got 0x%x\n",
					~0, GetFP0());
	fail = 1;
    }
    if (GetFP1() != ~1){
	msg_printf(DBG, "fpregs FP1 failed, expected 0x%x, got 0x%x\n",
					~1, GetFP1());
	fail = 1;
    }
    if (GetFP2() != ~2){
	msg_printf(DBG, "fpregs FP2 failed, expected 0x%x, got 0x%x\n",
					~2, GetFP2());
	fail = 1;
    }
    if (GetFP3() != ~3){
	msg_printf(DBG, "fpregs FP3 failed, expected 0x%x, got 0x%x\n",
					~3, GetFP3());
	fail = 1;
    }
    if (GetFP4() != ~4){
	msg_printf(DBG, "fpregs FP4 failed, expected 0x%x, got 0x%x\n",
					~4, GetFP4());
	fail = 1;
    }
    if (GetFP5() != ~5){
	msg_printf(DBG, "fpregs FP5 failed, expected 0x%x, got 0x%x\n",
					~5, GetFP5());
	fail = 1;
    }
    if (GetFP6() != ~6){
	msg_printf(DBG, "fpregs FP6 failed, expected 0x%x, got 0x%x\n",
					~6, GetFP6());
	fail = 1;
    }
    if (GetFP7() != ~7){
	msg_printf(DBG, "fpregs FP7 failed, expected 0x%x, got 0x%x\n",
					~7, GetFP7());
	fail = 1;
    }
    if (GetFP8() != ~8){
	msg_printf(DBG, "fpregs FP8 failed, expected 0x%x, got 0x%x\n",
					~8, GetFP8());
	fail = 1;
    }
    if (GetFP9() != ~9){
	msg_printf(DBG, "fpregs FP9 failed, expected 0x%x, got 0x%x\n",
					~9, GetFP9());
	fail = 1;
    }
    if (GetFP10() != ~10){
	msg_printf(DBG, "fpregs FP10 failed, expected 0x%x, got 0x%x\n",
					~10, GetFP10());
	fail = 1;
    }
    if (GetFP11() != ~11){
	msg_printf(DBG, "fpregs FP11 failed, expected 0x%x, got 0x%x\n",
					~11, GetFP11());
	fail = 1;
    }
    if (GetFP12() != ~12){
	msg_printf(DBG, "fpregs FP12 failed, expected 0x%x, got 0x%x\n",
					~12, GetFP12());
	fail = 1;
    }
    if (GetFP13() != ~13){
	msg_printf(DBG, "fpregs FP13 failed, expected 0x%x, got 0x%x\n",
					~13, GetFP13());
	fail = 1;
    }
    if (GetFP14() != ~14){
	msg_printf(DBG, "fpregs FP14 failed, expected 0x%x, got 0x%x\n",
					~14, GetFP14());
	fail = 1;
    }
    if (GetFP15() != ~15){
	msg_printf(DBG, "fpregs FP15 failed, expected 0x%x, got 0x%x\n",
					~15, GetFP15());
	fail = 1;
    }
    if (GetFP16() != ~16){
	msg_printf(DBG, "fpregs FP16 failed, expected 0x%x, got 0x%x\n",
					~16, GetFP16());
	fail = 1;
    }
    if (GetFP17() != ~17){
	msg_printf(DBG, "fpregs FP17 failed, expected 0x%x, got 0x%x\n",
					~17, GetFP17());
	fail = 1;
    }
    if (GetFP18() != ~18){
	msg_printf(DBG, "fpregs FP18 failed, expected 0x%x, got 0x%x\n",
					~18, GetFP18());
	fail = 1;
    }
    if (GetFP19() != ~19){
	msg_printf(DBG, "fpregs FP19 failed, expected 0x%x, got 0x%x\n",
					~19, GetFP19());
	fail = 1;
    }
    if (GetFP20() != ~20){
	msg_printf(DBG, "fpregs FP20 failed, expected 0x%x, got 0x%x\n",
					~20, GetFP20());
	fail = 1;
    }
    if (GetFP21() != ~21){
	msg_printf(DBG, "fpregs FP21 failed, expected 0x%x, got 0x%x\n",
					~21, GetFP21());
	fail = 1;
    }
    if (GetFP22() != ~22){
	msg_printf(DBG, "fpregs FP22 failed, expected 0x%x, got 0x%x\n",
					~22, GetFP22());
	fail = 1;
    }
    if (GetFP23() != ~23){
	msg_printf(DBG, "fpregs FP23 failed, expected 0x%x, got 0x%x\n",
					~23, GetFP23());
	fail = 1;
    }
    if (GetFP24() != ~24){
	msg_printf(DBG, "fpregs FP24 failed, expected 0x%x, got 0x%x\n",
					~24, GetFP24());
	fail = 1;
    }
    if (GetFP25() != ~25){
	msg_printf(DBG, "fpregs FP25 failed, expected 0x%x, got 0x%x\n",
					~25, GetFP25());
	fail = 1;
    }
    if (GetFP26() != ~26){
	msg_printf(DBG, "fpregs FP26 failed, expected 0x%x, got 0x%x\n",
					~26, GetFP26());
	fail = 1;
    }
    if (GetFP27() != ~27){
	msg_printf(DBG, "fpregs FP27 failed, expected 0x%x, got 0x%x\n",
					~27, GetFP27());
	fail = 1;
    }
    if (GetFP28() != ~28){
	msg_printf(DBG, "fpregs FP28 failed, expected 0x%x, got 0x%x\n",
					~28, GetFP28());
	fail = 1;
    }
    if (GetFP29() != ~29){
	msg_printf(DBG, "fpregs FP29 failed, expected 0x%x, got 0x%x\n",
					~29, GetFP29());
	fail = 1;
    }
    if (GetFP30() != ~30){
	msg_printf(DBG, "fpregs FP30 failed, expected 0x%x, got 0x%x\n",
					~30, GetFP30());
	fail = 1;
    }
    if (GetFP31() != ~31){
	msg_printf(DBG, "fpregs FP31 failed, expected 0x%x, got 0x%x\n",
					~31, GetFP31());
	fail = 1;
    }

    /* report any error */
    
    return(fail);
}
