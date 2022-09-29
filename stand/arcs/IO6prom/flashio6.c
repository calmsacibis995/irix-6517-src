/***********************************************************************\
*	File:		flashio6.c					*
*									*
*	Contains routines for erasing and programming the IO6  Flash 	*
*	EPROM. The Flash EPROM is an AMD Am29F080. 			*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <arcs/io.h>
#include <libsc.h>
#include <libsk.h>
#include "io6fprom.h"
#include <sys/SN/promhdr.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/klconfig.h>
#include <sys/PCI/bridge.h>

#define FLASHIO_DEBUG 1

extern unsigned int 	erase_flag ;
extern int		flash_which;
extern char         	filename[];
extern int          	foundfile, verbose, flash_debug ;
extern unsigned     	boardlist ;
extern int	    	modid, slotid ;
extern int		widgetid ;
extern int		w, n ;
extern nasid_t		flashprom_nasid ;
extern int		confirm_yes ;

/* an alias wrapper for flash_cmd */
int
flashio_cmd(int argc, char **argv)
{
	flash_which = IO_PROM;
	if (do_flash_cmd (argc, argv))
		return 1;
	return 0;
}

/*
 * program_io6()
 *      Programs a single io6 board.
 */

int
program_io6(promhdr_t *ph, char *buffer)
{
    	int         manu_code, dev_code, i ;
	__uint64_t  reg, value ;
	int	    r, quiesced = 0 ;
	bridge_t    *bridge_base ;
	fprom_t	    f;
	char 		ans ;

	if (!confirm_yes) {
		ans = confirm("IO", (erase_flag?"Erase":"Flash")) ;

		if (ans == 'q')
			return FLASH_QUIT ;
		else if (ans == 'n')
			return FLASH_NO ;
	}

        if ((!flash_debug) && (!erase_flag)) {
		if (ph->length > 0xe0000) {
	    		printf("> PROM too large (exceeds 14 sectors) ***\n");
	    	return 0;
		}
	}

    	/*
     	 * Erase the entire PROM
     	 */

    	printf("> Initializing IO FLASH PROM\n");

	bridge_base = (bridge_t *)(SN0_WIDGET_BASE(flashprom_nasid, widgetid)) ;
	f.base = (void *)(SN0_WIDGET_BASE(flashprom_nasid, widgetid) + 
				BRIDGE_EXTERNAL_FLASH) ;

#ifdef SABLE
goto success ;
#endif

	if (verbose) {
		printf("Trying to interpret current contents\n");
    		printf("Accessing the IO flash prom at %x\n", f.base) ;
		prom_check(&f) ;
	}

	if (flash_debug) {
		printf("flashio: base = %x\n", f.base) ;
		goto success ;
	}

	bridge_base->b_wid_control |= (0x1 << 31) ;
	bridge_base->b_wid_control;	/* inval addr bug war */

	f.dev    = FPROM_DEV_IO6_P0;
	f.afn    = NULL;

	((bridge_t *) ((__psunsigned_t)f.base & 0xffffffffff000000))
	    ->b_wid_control |= (0x1 << 31) ;
	((bridge_t *) ((__psunsigned_t)f.base & 0xffffffffff000000))
            ->b_wid_control;		/* inval addr bug war */

    	r = fprom_probe(&f, &manu_code, &dev_code);

	if (r < 0 && r != FPROM_ERROR_DEVICE) {
        	printf("Error initializing IO6 PROM: %s\n", 
			fprom_errmsg(r));
        	goto done;
    	}

    	printf("> Manufacturer code: 0x%02x\n", manu_code);
    	printf("> Device code      : 0x%02x\n", dev_code);

	if (r == FPROM_ERROR_DEVICE)
		printf("*** Warning: Unknown manufacture/device code\n");

    	printf("> Erasing code sectors (45 to 60 seconds)\n");

    	if ((r = fprom_flash_sectors(&f, 0x3fff)) < 0) {
        	printf("Error erasing remote PROM: %s\n", fprom_errmsg(r));
        	goto done;
    	}

    	printf("> Erasure complete and verified\n");

	if (erase_flag)
		return 1 ;

    	/*
     	* Program the PROM
     	*/

    	printf("> Programming the IO6 PROM\n");

	printf("> Writing %d bytes of data ...\n", ph->length) ;

	if (!(fprom_write_buffer(&f, buffer, ph->length)))
		goto done ;

	if (verbose)
		prom_check(&f) ;

	/* XXX do some check summing */

success:
    	printf("\n> Programmed and verified\n");
	return 0;

done:
    	printf("> IO PROM programming completed with error.\n");
	return 1 ;
}

