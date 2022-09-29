/*
**               Copyright (C) 1989, Silicon Graphics, Inc.
**
**  These coded instructions, statements, and computer programs  contain
**  unpublished  proprietary  information of Silicon Graphics, Inc., and
**  are protected by Federal copyright law.  They  may  not be disclosed
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc.
*/

/*
**      FileName:       mgv_vgi1_reg.c
*/

#include "mgv_diag.h"

/*
 * Forward Function References
 */
__uint32_t 	mgv_VGI1RegPatrnTest (__uint32_t argc, char **argv);
__uint32_t 	mgv_VGI1RegWalkTest (__uint32_t argc, char **argv);
__uint32_t	_mvg_VGI1PollReg(__uint32_t reg, ulonglong_t val, \
		ulonglong_t mask, __uint32_t timeOut, ulonglong_t *regVal);

/*
 * NAME: mgv_VGI1RegPatrnTest
 *
 * FUNCTION: VGI1 Register Pattern Test
 *
 * INPUTS: none
 *
 * OUTPUTS: -1 if error, 0 if no error
 */
__uint32_t
mgv_VGI1RegPatrnTest (__uint32_t argc, char **argv )
{
    mgv_vgi1_rwregs	regInfo[] = {
	"Channel[1] Trigger", VGI1_CH1_TRIG, 0x3,
	"Channel[2] Trigger", VGI1_CH2_TRIG, 0x3,
        "",                                            -1,     0
    };
    __uint32_t 		vgi_num = 1;
    mgv_vgi1_rwregs	*pRegInfo = regInfo;
    ulonglong_t		exp, rcv;
    __uint32_t		pat, errors=0;

    /* get the args */
    argc--; argv++;
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'v':
                        if (argv[0][2]=='\0') {
                            atobu(&argv[1][0], &vgi_num);
                            argc--; argv++;
                        } else {
                            atobu(&argv[0][2], &vgi_num);
                        }
                        break;
                default: break;
        }
        argc--; argv++;
    }

    if ( vgi_num > 2) {
        msg_printf(SUM,
         "Usage: mgv_vgiregpatrn -v <vgi_num>\n\
         -v --> vgi number [1|2]\n\
                 1  - VGI1-1 \n\
                 2  - VGI1-2 \n");
        return(-1);
    }


/* Sets giobase address to either VGI_1 base or VGI_2 base */
    if (vgi_num == 2) {
	MGV_SET_VGI1_2_BASE;
    } else {
	MGV_SET_VGI1_1_BASE;
    }

    msg_printf(SUM, "IMPV:- %s\n", mgv_err[VGI1_REG_PATTERN_TEST].TestStr);

    while (pRegInfo->offset != -1) {
    	for (pat = 0; pat < sizeof(mgv_patrn32)/sizeof(__uint32_t); pat++) {

	    exp = mgv_patrn32[pat] & pRegInfo->mask;

	    msg_printf(DBG, "pat 0x%x; mask 0xll%x\n",
		mgv_patrn32[pat], pRegInfo->mask);

	    VGI1_W64(mgvbase,pRegInfo->offset,mgv_patrn32[pat],pRegInfo->mask);

	    VGI1_R64(mgvbase, pRegInfo->offset, rcv, pRegInfo->mask);

	    msg_printf(DBG, "%s exp 0x%llx rcv 0x%llx\n", pRegInfo->str, exp, rcv);

	    if (exp != rcv) {
		errors++;
		msg_printf(ERR, "mgv_VGI1RegPatrnTest %s exp 0x%llx rcv 0x%llx\n",
			pRegInfo->str, exp, rcv);
		msg_printf(DBG, "mgv_VGI1RegPatrnTest %s offset 0x%x\n",
			pRegInfo->str, pRegInfo->offset);
	    }
	}
	pRegInfo++;
    }
/* Set giobase to VGI_1 before returning */
    MGV_SET_VGI1_1_BASE;

    return (_mgv_reportPassOrFail((&mgv_err[VGI1_REG_PATTERN_TEST]) ,errors));
}

/*
 * NAME: mgv_VGI1RegWalkTest
 *
 * FUNCTION: VGI1 Register Walking Bit Test
 *
 * INPUTS: none
 *
 * OUTPUTS: -1 if error, 0 if no error
 */
__uint32_t
mgv_VGI1RegWalkTest (__uint32_t argc, char **argv )
{
    mgv_vgi1_rwregs	regInfo[] = {
	"Channel[1] Trigger", VGI1_CH1_TRIG, 0x3,
	"Channel[2] Trigger", VGI1_CH2_TRIG, 0x3,
        "",                                             -1,     0
    };

   __uint32_t		vgi_num = 1;
    mgv_vgi1_rwregs	*pRegInfo = regInfo;
    ulonglong_t		exp, rcv;
    __uint32_t		pat, i, j, errors=0;

    /* get the args */
    argc--; argv++;
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 'v':
                        if (argv[0][2]=='\0') {
                            atobu(&argv[1][0], &vgi_num);
                            argc--; argv++;
                        } else {
                            atobu(&argv[0][2], &vgi_num);
                        }
                        break;
                default: break;
        }
        argc--; argv++;
    }

    if ( vgi_num > 2) {
        msg_printf(SUM,
         "Usage: mgv_vgiregpatrn -v <vgi_num>\n\
         -v --> vgi number [1|2]\n\
                 1  - VGI1-1 \n\
                 2  - VGI1-2 \n");
        return(-1);
    }



/* Sets giobase address to either VGI_1 base or VGI_2 base */
    if (vgi_num == 2) {
	MGV_SET_VGI1_2_BASE;
    } else {
	MGV_SET_VGI1_1_BASE;
    }

    msg_printf(SUM, "IMPV:- %s\n", mgv_err[VGI1_REG_WALK_BIT_TEST].TestStr);

    while (pRegInfo->offset != -1) {
	for (i = 0; i < 32; i++) {
	    pat = (~0x0) & (1 << i);

	    for (j = 0; j <= 1; j++) {
		if (j) {
		    pat = ~pat; /* get the walk 1 pattern */
		} 

	    	/* compute the expected value from the pattern and mask */
	    	exp = pat & pRegInfo->mask;

	    	VGI1_W64(mgvbase, pRegInfo->offset, pat, pRegInfo->mask);

	    	VGI1_R64(mgvbase, pRegInfo->offset, rcv, pRegInfo->mask);

		msg_printf(DBG, "mgv_VGI1RegWalkTest %s exp 0x%llx rcv 0x%llx\n",
			pRegInfo->str, exp, rcv);

	    	if (exp != rcv) {
		   errors++;
		   msg_printf(ERR, "mgv_VGI1RegWalkTest %s exp 0x%llx rcv 0x%llx\n",
			pRegInfo->str, exp, rcv);
		   msg_printf(DBG, "mgv_VGI1RegWalkTest %s offset 0x%x\n",
			pRegInfo->str, pRegInfo->offset);
	    	}
	    }
	}
	pRegInfo++;
    }

/* Set giobase to VGI_1 before returning */
    MGV_SET_VGI1_1_BASE;

    return (_mgv_reportPassOrFail((&mgv_err[VGI1_REG_WALK_BIT_TEST]) ,errors));
}

__uint32_t      
_mvg_VGI1PollReg(__uint32_t reg, ulonglong_t exp, ulonglong_t mask, 
	__uint32_t timeOut, ulonglong_t *regVal)
{
    ulonglong_t	val;

    do {
	VGI1_R64(mgvbase, reg, val, mask);
	timeOut--;
    } while (timeOut && (val != exp));

    bcopy((char *)&val, (char *)regVal, sizeof(ulonglong_t));

    if (timeOut) {
	return(0x0);
    } else {
	msg_printf(DBG, "_mvg_VGI1PollReg:- reg 0x%x; exp 0x%llx; mask 0x%llx; \
		val 0x%llx;\n", reg, exp, mask, val);
	return(0x1);
    }
}
