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

#ident "$Revision: 1.4 $"

/*
 *					
 *	Floating Point Exerciser - write/read fpu registers
 *					
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <regdef.h>
#include <sys/fpu.h>
#include "everr_hints.h"
#include "ide_msg.h"
#include "pattern.h"
#include "ip19.h"

static int cpu_loc[2];

int
fpregs()
{
    unsigned retval = 0;
    uint osr;

    getcpu_loc(cpu_loc);
    msg_printf(INFO, " (fpregs) \n");
    msg_printf(DBG, "Running on CPU %d on IP19 in slot %d\n", cpu_loc[1], cpu_loc[0]);

    /* enable cache and fpu - cache ecc errors enabled */

    osr = GetSR();
    msg_printf(DBG, "Original status reg setting : 0x%x\n", osr);
    SetSR(osr | SR_CU1);
    msg_printf(DBG, "Status reg setting for test : 0x%x\n", GetSR());

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
	err_msg(FPREG_DATA, cpu_loc, 0, GetFP0());
	retval = 1;
    }
    if (GetFP1() != 1){
	err_msg(FPREG_DATA, cpu_loc, 1, GetFP1());
	retval = 1;
    }
    if (GetFP2() != 2){
	err_msg(FPREG_DATA, cpu_loc, 2, GetFP2());
	retval = 1;
    }
    if (GetFP3() != 3){
	err_msg(FPREG_DATA, cpu_loc, 3, GetFP3());
	retval = 1;
    }
    if (GetFP4() != 4){
	err_msg(FPREG_DATA, cpu_loc, 4, GetFP4());
	retval = 1;
    }
    if (GetFP5() != 5){
	err_msg(FPREG_DATA, cpu_loc, 5, GetFP5());
	retval = 1;
    }
    if (GetFP6() != 6){
	err_msg(FPREG_DATA, cpu_loc, 6, GetFP6());
	retval = 1;
    }
    if (GetFP7() != 7){
	err_msg(FPREG_DATA, cpu_loc, 7, GetFP7());
	retval = 1;
    }
    if (GetFP8() != 8){
	err_msg(FPREG_DATA, cpu_loc, 8, GetFP8());
	retval = 1;
    }
    if (GetFP9() != 9){
	err_msg(FPREG_DATA, cpu_loc, 9, GetFP9());
	retval = 1;
    }
    if (GetFP10() != 10){
	err_msg(FPREG_DATA, cpu_loc, 10, GetFP10());
	retval = 1;
    }
    if (GetFP11() != 11){
	err_msg(FPREG_DATA, cpu_loc, 11, GetFP11());
	retval = 1;
    }
    if (GetFP12() != 12){
	err_msg(FPREG_DATA, cpu_loc, 12, GetFP12());
	retval = 1;
    }
    if (GetFP13() != 13){
	err_msg(FPREG_DATA, cpu_loc, 13, GetFP13());
	retval = 1;
    }
    if (GetFP14() != 14){
	err_msg(FPREG_DATA, cpu_loc, 14, GetFP14());
	retval = 1;
    }
    if (GetFP15() != 15){
	err_msg(FPREG_DATA, cpu_loc, 15, GetFP15());
	retval = 1;
    }
    if (GetFP16() != 16){
	err_msg(FPREG_DATA, cpu_loc, 16, GetFP16());
	retval = 1;
    }
    if (GetFP17() != 17){
	err_msg(FPREG_DATA, cpu_loc, 17, GetFP17());
	retval = 1;
    }
    if (GetFP18() != 18){
	err_msg(FPREG_DATA, cpu_loc, 18, GetFP18());
	retval = 1;
    }
    if (GetFP19() != 19){
	err_msg(FPREG_DATA, cpu_loc, 19, GetFP19());
	retval = 1;
    }
    if (GetFP20() != 20){
	err_msg(FPREG_DATA, cpu_loc, 20, GetFP20());
	retval = 1;
    }
    if (GetFP21() != 21){
	err_msg(FPREG_DATA, cpu_loc, 21, GetFP21());
	retval = 1;
    }
    if (GetFP22() != 22){
	err_msg(FPREG_DATA, cpu_loc, 22, GetFP22());
	retval = 1;
    }
    if (GetFP23() != 23){
	err_msg(FPREG_DATA, cpu_loc, 23, GetFP23());
	retval = 1;
    }
    if (GetFP24() != 24){
	err_msg(FPREG_DATA, cpu_loc, 24, GetFP24());
	retval = 1;
    }
    if (GetFP25() != 25){
	err_msg(FPREG_DATA, cpu_loc, 25, GetFP25());
	retval = 1;
    }
    if (GetFP26() != 26){
	err_msg(FPREG_DATA, cpu_loc, 26, GetFP26());
	retval = 1;
    }
    if (GetFP27() != 27){
	err_msg(FPREG_DATA, cpu_loc, 27, GetFP27());
	retval = 1;
    }
    if (GetFP28() != 28){
	err_msg(FPREG_DATA, cpu_loc, 28, GetFP28());
	retval = 1;
    }
    if (GetFP29() != 29){
	err_msg(FPREG_DATA, cpu_loc, 29, GetFP29());
	retval = 1;
    }
    if (GetFP30() != 30){
	err_msg(FPREG_DATA, cpu_loc, 30, GetFP30());
	retval = 1;
    }
    if (GetFP31() != 31){
	err_msg(FPREG_DATA, cpu_loc, 31, GetFP31());
	retval = 1;
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
	err_msg(FPREG_INVD, cpu_loc, ~0, GetFP0());
	retval = 1;
    }
    if (GetFP1() != ~1){
	err_msg(FPREG_INVD, cpu_loc, ~1, GetFP1());
	retval = 1;
    }
    if (GetFP2() != ~2){
	err_msg(FPREG_INVD, cpu_loc, ~2, GetFP2());
	retval = 1;
    }
    if (GetFP3() != ~3){
	err_msg(FPREG_INVD, cpu_loc, ~3, GetFP3());
	retval = 1;
    }
    if (GetFP4() != ~4){
	err_msg(FPREG_INVD, cpu_loc, ~4, GetFP4());
	retval = 1;
    }
    if (GetFP5() != ~5){
	err_msg(FPREG_INVD, cpu_loc, ~5, GetFP5());
	retval = 1;
    }
    if (GetFP6() != ~6){
	err_msg(FPREG_INVD, cpu_loc, ~6, GetFP6());
	retval = 1;
    }
    if (GetFP7() != ~7){
	err_msg(FPREG_INVD, cpu_loc, ~7, GetFP7());
	retval = 1;
    }
    if (GetFP8() != ~8){
	err_msg(FPREG_INVD, cpu_loc, ~8, GetFP8());
	retval = 1;
    }
    if (GetFP9() != ~9){
	err_msg(FPREG_INVD, cpu_loc, ~9, GetFP9());
	retval = 1;
    }
    if (GetFP10() != ~10){
	err_msg(FPREG_INVD, cpu_loc, ~10, GetFP10());
	retval = 1;
    }
    if (GetFP11() != ~11){
	err_msg(FPREG_INVD, cpu_loc, ~11, GetFP11());
	retval = 1;
    }
    if (GetFP12() != ~12){
	err_msg(FPREG_INVD, cpu_loc, ~12, GetFP12());
	retval = 1;
    }
    if (GetFP13() != ~13){
	err_msg(FPREG_INVD, cpu_loc, ~13, GetFP13());
	retval = 1;
    }
    if (GetFP14() != ~14){
	err_msg(FPREG_INVD, cpu_loc, ~14, GetFP14());
	retval = 1;
    }
    if (GetFP15() != ~15){
	err_msg(FPREG_INVD, cpu_loc, ~15, GetFP15());
	retval = 1;
    }
    if (GetFP16() != ~16){
	err_msg(FPREG_INVD, cpu_loc, ~16, GetFP16());
	retval = 1;
    }
    if (GetFP17() != ~17){
	err_msg(FPREG_INVD, cpu_loc, ~17, GetFP17());
	retval = 1;
    }
    if (GetFP18() != ~18){
	err_msg(FPREG_INVD, cpu_loc, ~18, GetFP18());
	retval = 1;
    }
    if (GetFP19() != ~19){
	err_msg(FPREG_INVD, cpu_loc, ~19, GetFP19());
	retval = 1;
    }
    if (GetFP20() != ~20){
	err_msg(FPREG_INVD, cpu_loc, ~20, GetFP20());
	retval = 1;
    }
    if (GetFP21() != ~21){
	err_msg(FPREG_INVD, cpu_loc, ~21, GetFP21());
	retval = 1;
    }
    if (GetFP22() != ~22){
	err_msg(FPREG_INVD, cpu_loc, ~22, GetFP22());
	retval = 1;
    }
    if (GetFP23() != ~23){
	err_msg(FPREG_INVD, cpu_loc, ~23, GetFP23());
	retval = 1;
    }
    if (GetFP24() != ~24){
	err_msg(FPREG_INVD, cpu_loc, ~24, GetFP24());
	retval = 1;
    }
    if (GetFP25() != ~25){
	err_msg(FPREG_INVD, cpu_loc, ~25, GetFP25());
	retval = 1;
    }
    if (GetFP26() != ~26){
	err_msg(FPREG_INVD, cpu_loc, ~26, GetFP26());
	retval = 1;
    }
    if (GetFP27() != ~27){
	err_msg(FPREG_INVD, cpu_loc, ~27, GetFP27());
	retval = 1;
    }
    if (GetFP28() != ~28){
	err_msg(FPREG_INVD, cpu_loc, ~28, GetFP28());
	retval = 1;
    }
    if (GetFP29() != ~29){
	err_msg(FPREG_INVD, cpu_loc, ~29, GetFP29());
	retval = 1;
    }
    if (GetFP30() != ~30){
	err_msg(FPREG_INVD, cpu_loc, ~30, GetFP30());
	retval = 1;
    }
    if (GetFP31() != ~31){
	err_msg(FPREG_INVD, cpu_loc, ~31, GetFP31());
	retval = 1;
    }
    SetSR(osr);
    msg_printf(INFO, "Completed FP registers\n");

    /* report any error */
    
    return(retval);
}
