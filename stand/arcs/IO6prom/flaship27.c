/***********************************************************************\
*	File:		flaship27.c					*
*									*
*	Contains routines for erasing and programming the IP27 Flash 	*
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
#include <libkl.h>
#include "promgraph.h"
#include "io6fprom.h"  		/* IO6 Flash specific defines */
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/promhdr.h>
#include <sys/SN/promlog.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/SN0/IP31.h>

#include <sys/graph.h>
#include <sys/hwgraph.h>

extern vertex_hdl_t 	hw_vertex_hdl ;
extern graph_hdl_t 	prom_graph_hdl ;
extern unsigned int 	erase_flag ;
extern unsigned int 	count_flag ;
extern int		flash_which;
extern int	        log_erase_flag ;
extern int	        check_flag ;
extern int	        full_check_flag ;
extern char	    	filename[];
extern int          	foundfile, verbose, flash_debug ;
extern unsigned     	boardlist ;
extern int	    	modid, slotid ;
extern nasid_t	    	flashprom_nasid ;
extern int		w, n ;
extern int		confirm_yes ;

extern void copy_buffer_adj_sum(ip27config_t *);
extern int ip31_get_config(nasid_t, ip27config_t *);

extern ip27config_t *new_config_info;

/* an alias wrapper for flash_cmd */
int
flashcpu_cmd(int argc, char **argv)
{
	flash_which = CPU_PROM;
	if (do_flash_cmd (argc, argv))
		return 1;
	return 0;
}

/*
 * program_ip27()
 *      Programs a single ip27 board.
 */

int
program_ip27(promhdr_t *ph, char *buffer)
{
	int             quiesced = 0  ;
    	int         	manu_code, dev_code;
    	char       	*src, *src_end;
    	off_t       	dst_off;
    	int         	r, done, first, n, i ;
	__uint64_t  	reg, value ;
	int		length ;
	fprom_t		f;
        char            ans ;
	promlog_t	l;
	ip27config_t    *config_info;
	char            input_str[8];

	if (count_flag) {
		config_info = ((ip27config_t *) malloc(sizeof(ip27config_t)));
		if (flashprom_nasid < 0)
			bcopy((char *)(IP27CONFIG_ADDR), 
			      (char *) config_info, 
			      sizeof(ip27config_t));
		else
			bcopy((char *)(IP27CONFIG_ADDR_NODE(flashprom_nasid)),
			      (char *) config_info, 
			      sizeof(ip27config_t));

		printf("The prom for nasid %d has been flashed %d times\n",
		       flashprom_nasid, config_info->flash_count);

		free(config_info);

		return 0;

	}

	if (log_erase_flag) {
		printf("Initializing prom log\n");
		if (ip27log_setup(&l, flashprom_nasid,
				  IP27LOG_DO_INIT | IP27LOG_VERBOSE) < 0) {
			printf("Failed to initalize prom log\n");
		}
		return 0;
	}


	if (!confirm_yes) {
        	ans = confirm("CPU", (erase_flag?"Erase":"Flash")) ;

        	if (ans == 'q')
                	return FLASH_QUIT ;
        	else if (ans == 'n')
                	return FLASH_NO ;
	}



	if (! (full_check_flag || check_flag)) {
		/* get the configutation values for each board */
		
		if (flashprom_nasid < 0)
			bcopy((char *)(IP27CONFIG_ADDR), 
			      (char *) new_config_info, 
			      sizeof(ip27config_t));
		else {
			bcopy((char *)(IP27CONFIG_ADDR_NODE(flashprom_nasid)),
			      (char *) new_config_info, 
			      sizeof(ip27config_t));

			if (ip31_pimm_psc(flashprom_nasid, NULL, NULL) >= 0)
				ip31_get_config(flashprom_nasid, new_config_info);
		}

		new_config_info->check_sum_adj = (uint) 0;
		if (new_config_info->magic != CONFIG_MAGIC) {
			printf("prom has insufficient configuration values - please set with -f or -F\n");
			return 0;
		}
		/* now copy info into buffer */
	}
	else {
		/* here we just need to set the flash count */
		config_info = ((ip27config_t *) malloc(sizeof(ip27config_t)));
		if (flashprom_nasid < 0)
			bcopy((char *)(IP27CONFIG_ADDR), 
			      (char *) config_info, 
			      sizeof(ip27config_t));
		else
			bcopy((char *)(IP27CONFIG_ADDR_NODE(flashprom_nasid)),
			      (char *) config_info, 
			      sizeof(ip27config_t));
		new_config_info->flash_count = config_info->flash_count;
		free(config_info);
	}
	/* now copy the info to the buffer we will write the prom with */

	new_config_info->flash_count = new_config_info->flash_count + 1;
	new_config_info->check_sum_adj = (uint) 0;
	new_config_info->pvers_vers = ph->version;
	new_config_info->pvers_rev = ph->revision;

	/* Make sure we have a valid fprom_wr bit and fprom_cyc bit */
	if (new_config_info->fprom_wr == 0) {
		if (SN00) {
			new_config_info->fprom_wr = CONFIG_SN00_FPROM_WR;
			new_config_info->fprom_cyc = CONFIG_SN00_FPROM_CYC;
		}
		else {
			new_config_info->fprom_wr = CONFIG_SN0_FPROM_WR;
			new_config_info->fprom_cyc = CONFIG_SN0_FPROM_CYC;
		}
		printf("warning: 0 values setting fprom_wr to %d and fprom_cyc to %d\n",
		       new_config_info->fprom_wr, new_config_info->fprom_cyc);
	}

	copy_buffer_adj_sum(new_config_info);
	
	length = ph->length - ph->segs[0].offset ;

	if ((!flash_debug) && (!erase_flag)) {
		if (length > 0xe0000) {
	    		printf("> PROM too large (exceeds 14 sectors) ***\n");
	    		return 0;
		}
	}

	/* see if the log is valid if not erase it and make it valid */
	if (ip27log_setup(&l, flashprom_nasid, IP27LOG_VERBOSE) < 0) {
		/* log is invalid fix it up */
		if (ip27log_setup(&l, flashprom_nasid,
				  IP27LOG_DO_INIT | IP27LOG_VERBOSE) < 0) {
			printf("Error: Failed to initalize prom log\n");
			printf("Do you wish to continue with flash anyway y/n [n]");
			gets(input_str);
			if (input_str[0] != 'y') {
				return 0;
			}
		}
	}


	/* Check if it is own nasid */

    	if (flashprom_nasid != get_nasid()) {
		/* If hub is remote, check if we can reach it */

		if (! hub_accessible(flashprom_nasid)) {
			printf("> Remote hub is not responding\n");
			return 0 ;
		}

		/*
		 * Quiesce remote CPUs so they aren't accessing the PROM while
		 * we're trying to flash it.
		 */

        	printf("> Quiescing CPUs on remote Hub\n");

        	(SD(REMOTE_HUB(flashprom_nasid, PI_CPU_ENABLE_A), 0));
        	(SD(REMOTE_HUB(flashprom_nasid, PI_CPU_ENABLE_B), 0));

        	us_delay(100000);

        	quiesced = 1;
    	}

    	/*
	 * Erase the entire PROM
	 */

    	printf("> Initializing PROM\n");

#ifdef SABLE
return 1 ;
#endif

    	reg = (__uint64_t) REMOTE_HUB(flashprom_nasid, MD_MEMORY_CONFIG);

    	value = LD(reg);
    	value &= ~(MMC_FPROM_CYC_MASK | MMC_FPROM_WR_MASK);
    	value |= (0x08L << MMC_FPROM_CYC_SHFT | 0x01L << MMC_FPROM_WR_SHFT);
    	SD(reg, value);

    	f.base   = (void *) (flashprom_nasid < 0 ? LBOOT_BASE : NODE_RBOOT_BASE(flashprom_nasid));
	f.dev    = FPROM_DEV_HUB;
	f.afn    = NULL;

	if (flash_debug) {
    		printf("flaship27: base = %x\n", f.base) ;
		goto done ;
	}

    	r = fprom_probe(&f, &manu_code, &dev_code);

	if (r < 0 && r != FPROM_ERROR_DEVICE) {
		printf("Error initializing IP27 PROM: %s\n",
			fprom_errmsg(r));
		goto done;
	}

    	printf("> Manufacturer code: 0x%02x\n", manu_code);
    	printf("> Device code      : 0x%02x\n", dev_code);

	if (r == FPROM_ERROR_DEVICE)
		printf("Warning: Unknown manufacturer/device code\n");

    	/* printf("> Use ^C to abort erasing or programming\n"); */

    	printf("> Erasing code sectors (15 to 20 seconds)\n");

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

    	printf("> Programming the IP27 PROM\n");
    	printf(">   It has been programmed %d times\n",new_config_info->flash_count);

        printf("> Writing %d bytes of data ...\n", 
			length) ;

        if (!(fprom_write_buffer(&f, buffer + ph->segs[0].offset, length)))
                goto done ;

    	printf("\n> Programmed and verified\n");

done:

    if (quiesced) {
        /*
         * Restart remote Hub.
         */

        	printf("> Restarting CPU(s); reset required.\n");

        	SD(REMOTE_HUB(flashprom_nasid, PI_CPU_ENABLE_A), 1);
        	SD(REMOTE_HUB(flashprom_nasid, PI_CPU_ENABLE_B), 1);

#if 0
        	us_delay(100000);

		/*
		 * Do not use soft reset.  There seems to be a nasty bug where
		 * if a node is soft reset, memory accesses become
		 * unpredictable and memory tests fail.
		 */

        	SD(REMOTE_HUB(flashprom_nasid, PI_SOFTRESET), 0);
#endif
    	}

	return 1 ;
}
