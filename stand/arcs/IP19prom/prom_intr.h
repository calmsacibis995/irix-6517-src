/***********************************************************************\
*	File:		prom_intr.h					*
*									*
*	Contains interrupt levels which are used for communication	*
*	between slave and master processors.				*
*									*
\***********************************************************************/

#ifndef _IP19PROM_INTR_H_
#define _IP19PROM_INTR_H_

#define	PGRP_MASTER 64		/* Processor group containing Boot Master */
#define IGR_MASTER   1		/* IGR mask value for Boot Master group */
#define PGRP_SLAVE  65		/* Processor group containing slaves */
#define IGR_SLAVE    2		/* IGR mask value for slave processor grp */
#define PGRP_ALL   127		/* Broadcast destination */

/*
 * When the Boot Master wants a slave processor to perform a task, it 
 * generates a group directed-interrupt using one of the following
 * levels.
 */

#define BOOT_LEVEL	6	/* Interrupt indicating BM has taken over */

#define REARB_LEVEL	1	/* Intr Level for Rearbitration request */
#define STATUS_LEVEL	2	/* Intr Level for initializing evcfginfo */
#define LAUNCH_LEVEL	3	/* Intr Level for launching */
#define BIGEND_LEVEL	4	/* Intr Level for big endian notification */
#define LITTLEND_LEVEL	5	/* Intr Level for little endian notify */
#define POD_LEVEL	6	/* Intr Level for jumping to POD mode */
#define MPCONF_INIT_LEVEL 7	/* Intr Level for updating the MPCONF block */
#define MPCONF_LAUNCH_LEVEL 8	/* Intr Level for launching from MPCONF */

#define INTR_LEVEL_MASK ((1<<REARB_LEVEL) | (1<<STATUS_LEVEL)  | \
		         (1<<LAUNCH_LEVEL) | (1<<BIGEND_LEVEL) | \
		         (1<<LITTLEND_LEVEL) | (1<<POD_LEVEL)  | \
			 (1<<MPCONF_INIT_LEVEL) | (1<<MPCONF_LAUNCH_LEVEL))

#define	SYNC_DEST	80	/* Sync the realtime clocks and start them.
				 *   also tells IA to load timeout values.
				 */

#ifdef LANGUAGE_C
#define SENDINT_SLAVES(_l) \
	store_double_lo(EV_SENDINT, EVINTR_VECTOR((_l), PGRP_SLAVE));
#define SENDINT_ALL(_l) \
	store_double_lo(EV_SENDINT, EVINTR_VECTOR((_l), PGRP_ALL));

#else
#define SENDINT_SLAVES(_l) \
	li 	k0, EV_SENDINT;	\
	li	k1, EVINTR_VECTOR((_l), PGRP_SLAVE); \
	sd	k1, 0(k0); \
	nop

#define SENDINT_ALL(_l) \
	li 	k0, EV_SENDINT; \
	li	k1, EVINTR_VECTOR((_l), PGRP_ALL); \
	sd	k1, 0(k0); \
	nop

#endif /* LANGUAGE_C */


/*
 * Boot Master interrupt levels.  The first Eight levels are
 * free, since there is no slot 0.  Levels 8 - 127 are used for
 * slave processor state notification.  Note that two levels are
 * reserved for each processor. 
 */

#define RESET_LEVEL	0x07

#define ACHIP_LEVEL	0x01
#define MC3ERR_LEVEL	0x02
#define IO4ERR_LEVEL	0x03
#define FIXEAROM_LEVEL	0x04

#define MC3ERR_VECTOR	EVINTR_VECTOR(MC3ERR_LEVEL, PGRP_MASTER)
#define IO4ERR_VECTOR	EVINTR_VECTOR(IO4ERR_LEVEL, PGRP_MASTER)

#define SEND_MASTER(_l)	EVINTR_VECTOR((_l), PGRP_MASTER)

/*
 * System timeout values.  These values are used for all of the boards.
 */
#define RSC_TIMEOUT	5000000		/* .1 second timeout */	
#define EBUS_TIMEOUT	4000000		/* .08 sec timeout */	
#define URG_TIMEOUT	0x40		/* Very small */		

#endif /* _IP19PROM_INTR_H_ */ 
