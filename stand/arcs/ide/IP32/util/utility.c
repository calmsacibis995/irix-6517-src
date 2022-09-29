/*
 * utility.c - uytilities for the godzilla architecture ide
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
#ident "ide/godzilla/util/utility.c: $Revision: 1.1 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

bool_t
report_pass_or_fail(__uint64_t _e, char * _str, __uint32_t _fru_num)
/* report_pass_or_fail: at the end of each test  . 0=PASS, 1=FAIL */
{
	char 	*_fru;							
	__uint32_t	__fru_num = _fru_num;
	msg_printf(DBG, "FRU is %d\n", __fru_num);
	if (!_e) {
	    msg_printf(SUM, "%s Test PASSED\n", _str);
	    return(0);  
	} else {
	    switch (__fru_num) {
	    	case D_FRU_PMXX:	_fru = fru_pmxx; break;
	    	case D_FRU_IP30:	_fru = fru_ip30; break;
	    	case D_FRU_FRONT_PLANE:	_fru = fru_front_plane; break;
	    	case D_FRU_PCI_CARD:	_fru = fru_pci_card; break;
	    	default:
		     msg_printf(SUM, "Illegal FRU specified\n");
		     _fru = NULL; break;
	    }	
	    msg_printf(SUM, "%s Test FAILED\n", _str);
	    if (_fru) {
	       msg_printf(SUM, "The faulty FRU is %s\n", _fru);	
	    }
	    return(1); 
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
	   __value |= (~mask);	
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
	   __value |= (~mask);
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

#endif /* C || C++ */

