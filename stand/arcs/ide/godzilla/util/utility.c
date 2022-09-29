/*
 * utility.c - utilities for the godzilla architecture ide
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 *	These functions are used throughout the godzilla architecture ide.
 *	report_pass_or_fail: at the end of each test, returns 0/1
 *	pio_reg_mod_32/64: modify the content of a PIO register and
 *				return the incremental # of errors
 */
#ident "ide/godzilla/util/utility.c: $Revision: 1.10 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "stringlist.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/* similar to emfail (stand/arcs/ide/godzilla/uif/status.c), used by field */
int
report_pass_or_fail( _Test_Info *Test, __uint32_t errors )
/* report_pass_or_fail: at the end of each test  . 0=PASS, 1=FAIL */
{
#ifdef NOTNOW
	char 	*_fru, msg[256];
	__uint32_t	__fru_num = _fru_num;
#ifdef MFG_USED
	strcpy(msg, _str);
#endif
	msg_printf(DBG, "FRU is %d\n", __fru_num);
	if (!_e) {
	    msg_printf(SUM, "%s Test PASSED\n", _str);
#ifdef MFG_USED
	    strcat(msg, ".pass");
	    _send_bootp_msg(msg);
#endif
	    return(0);  
	} else {
	    msg_printf(SUM, "%s Test FAILED (%d errors)\n", _str, _e);
	    if (__fru_num < GODZILLA_FRUS) {
		msg_printf(SUM, "The faulty FRU is %s\n", 
				godzilla_fru_names[__fru_num]);	
	    }
	    else  msg_printf(SUM, "UNKNOWN FRU! \n");
#ifdef MFG_USED
	    strcat(msg, ".fail");
	    _send_bootp_msg(msg);
#endif
	    return(1); 
	}	      
#endif

#if (defined(MFG_USED) && defined(IP30))
  char msg[256];

  strcpy(msg, Test->TestStr);
#endif

  if (!errors) {
    msg_printf(SUM, "     %s Test passed.\n" ,Test->TestStr);
#if (defined(MFG_USED) && defined(IP30))
    strcat(msg, ".pass");
    _send_bootp_msg(msg);
#endif
    return 0;
  }
  else {
    msg_printf(ERR, "**** %s Test failed.  ErrCode %s Errors %d\n",
         Test->TestStr, Test->ErrStr, errors);
#ifdef MFG_USED
    strcat(msg, "_");
    strcat(msg, Test->ErrStr);
    strcat(msg, ".fail");
    _send_bootp_msg(msg);
#endif

    return -1;
  }
}

__uint64_t pio_reg_mod_32(__uint32_t address, __uint32_t mask, int mode)
/* modify the content of a PIO register and return the incremental # of errors*/
{
	__uint32_t	__value;
	__uint64_t	incr_d_errors = 0;
	PIO_REG_RD_32(address, ~0x0, __value);
	if (mode == SET_MODE) {		
	   __value |= mask;	
	} else if (mode == CLR_MODE) {
	   __value &= (~mask);	
	} else {
	   msg_printf(ERR, "Illegal mode in PIO_REG_MOD_32\n");
	   incr_d_errors++;	
	}
	PIO_REG_WR_32(address, ~0x0, __value);
	return(incr_d_errors);
}

__uint64_t pio_reg_mod_64(__uint32_t address, __uint32_t mask, int mode)
/* modify the content of a PIO register and return the incremental # of errors*/
{
	__uint64_t	__value;
	__uint64_t	incr_d_errors = 0;
	PIO_REG_RD_64(address, ~0x0, __value);
	if (mode == SET_MODE) {
	   __value |= mask;
	} else if (mode == CLR_MODE) {
	   __value &= (~mask);
	} else {
	   msg_printf(ERR, "Illegal mode in PIO_REG_MOD_64\n");
	   incr_d_errors++;	
	}
	PIO_REG_WR_64(address, ~0x0, __value);
	return(incr_d_errors);
}

__uint64_t br_reg_mod_64(__uint32_t address, __uint32_t mask, int mode)
/* modify the content of a bridge register and return the incr. # of errors*/
{
	return(pio_reg_mod_32(address+BRIDGE_BASE, mask, mode));
}

int _mfg_oven = 0;

void
bootpon(void)
{
	_mfg_oven = 1;
}


void
bootpoff(void)
{
	_mfg_oven = 0;
}

#ifdef MFG_USED
extern int _dodelay(int argc, char **argv);
void
_bootp_delay(void)
{
	struct string_list av;
	int ac;
	char cmdbuf[256];

	sprintf( cmdbuf, "_dodelay -s 5");
	ac = _argvize( cmdbuf, &av );
	_dodelay(ac, av.strptrs);
}

void
_send_bootp_msg(char *msg)
{
	struct string_list av;
	int ac, i = 0;
	char cmdbuf[256], tmp[256];
	char *pMsg = msg;

	if (!_mfg_oven) return;
	while (*pMsg != NULL) {
	   if (*pMsg == ' ') {
		tmp[i] = '_';
	   } else {
		tmp[i] = *pMsg;
	   }
	   i++; pMsg++;
	}
	tmp[i] = NULL;
	sprintf( cmdbuf, "boot -f bootp()%s", tmp);
	ac = _argvize( cmdbuf, &av );
	_bootp_delay();
	boot(ac, av.strptrs, NULL, NULL);
}

#endif

#endif /* C || C++ */

