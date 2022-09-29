/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#if defined(IP30)

#include <arcs/types.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include <libsc.h>
#include <sys/RACER/sflash.h>
#include "flash_prom_cmd.h"

SETUP_TIME

#ifdef FDEBUG
static void
fl_warning(void) {
	FPR_MSG((
"\n"
"WARNING: You are about to reprogram the code you are executing! If system\n"
"  does not reboot within 60 seconds something went wrong in the process.\n"
"  To recover, refer to '(B) Using the 40pin prom to program the flash'in:\n"
"  http://femto.engr/hahn/work/flash_alt.html#B\n\n"
	));
}
#endif

int
yes(char *question)
{
	char response[4];

	FPR_PR(("%s?[y/n] ", question));
	response[0] = 0;
	fgets(response, 4, 0);
	return(response[0] == 'y' ||  response[0] == 'Y');
}

/*
 * program the flash 
 */
int
_fl_prom(uint16_t *ld_buf, int reset, int beg_seg, int fl_sz)
{
	int end_seg;
	flash_err_t frv;

	end_seg = beg_seg + SFLASH_BTOS(fl_sz - 1);

	FPR_HI(("src buf 0x%x, beg_seg %d end_seg %d reset=%d\n",
		ld_buf, beg_seg, end_seg, reset));

#ifdef FDEBUG
	if (reset)
	    fl_warning();

	FPR_PR(("flash: erase and program segments %d - %d",
		beg_seg, end_seg));
	if (!yes(""))
	    return(1);
	FPR_MSG(("erase and re-program will take ~2 second per segment\n"));
#endif /*FDEBUG*/

	/*
	 * if we are flashing the memory we are running on
	 * then we must reset after the flash has been programmed
	 */
	if (reset) {
	    (void)flash_program((uint16_t *)ld_buf, reset, beg_seg, end_seg);
	    /* we will never get here */
	} else {
	    START_TIME;
	    frv = flash_program((uint16_t *)ld_buf, reset, beg_seg, end_seg);
	    STOP_TIME("flash programming ");
	    if (frv) {
		flash_print_err(frv);
		return(1);
	    }
	}
	return(0);
}

/*
 * verify if the loaded file is either 
 * fprom.bin or rprom.bin
 */
int
_fl_check_bin_image(char *buf, int len)
{
	flash_header_t *rhdr, *fhdr;
	char *offs = buf;

	/*
	 * rprom.bin check
	 */
	offs += SFLASH_RPROM_HDR_OFF;
	rhdr = (flash_header_t *)offs;
	if (flash_prom_ok(buf, rhdr)) {
	    offs += FLASH_HEADER_SIZE;
	    fhdr = (flash_header_t *)offs;
	    if (flash_prom_ok(offs + FLASH_HEADER_SIZE, fhdr)) {
		/*
		 * we have a good rprom.bin image
		 */
		FPR_MSG(("Rprom current version 0x%x, ",
			(SFLASH_RPROM_HDR_ADDR)->version));
		FPR_MSG(("new version 0x%x\n", rhdr->version));
		flash_upd_prom_hdr(SFLASH_RPROM_SEG, (__psunsigned_t)rhdr);

		FPR_MSG(("Fprom current version 0x%x, ",
			(SFLASH_FPROM_HDR_ADDR)->version));
		FPR_MSG(("new version 0x%x\n", fhdr->version));
		flash_upd_prom_hdr(SFLASH_FPROM_SEG, (__psunsigned_t)fhdr);

		return(IP30PROM_BIN_FORMAT);
	    }
	    /* bad image */
	    return(NO_FORMAT);
	}
	/*
	 * fprom.bin check
	 */
	fhdr = (flash_header_t *)buf;
	if (flash_prom_ok(buf + FLASH_HEADER_SIZE, fhdr)) {
	    /*
	     * we have a good rprom.bin image
	     */
	    FPR_MSG(("Fprom current version 0x%x, ",
		    (SFLASH_FPROM_HDR_ADDR)->version));
	    FPR_MSG(("new version 0x%x\n", fhdr->version));
	    flash_upd_prom_hdr(SFLASH_FPROM_SEG, (__psunsigned_t)fhdr);

	    return(FPROM_BIN_FORMAT);
	}
	return(NO_FORMAT);
}
#endif /* defined(IP30) */
