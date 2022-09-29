/*
 * COPYRIGHT NOTICE
 * Copyright (c) Alteon Networks, Inc. 1996 
 * All rights reserved
 * 
 */
/*
 * HISTORY
 * $Log: tigon_eeprom.h,v $
 * Revision 1.2  1999/05/18 21:44:02  trp
 * Upgrade to firmware level 12.3.9
 *
 * Revision 1.1.4.19  1998/06/30  01:33:48  tibor
 * 	foo[0] not ansi
 * 	[1998/06/30  01:33:24  tibor]
 *
 * Revision 1.1.4.18  1998/06/19  18:16:19  hayes
 * 	fix U32 foo[0] construct for solaris...
 * 	[1998/06/19  18:16:10  hayes]
 * 
 * Revision 1.1.4.17  1998/06/19  07:55:10  tibor
 * 	move eeprom down for platform specificness
 * 	[1998/06/19  07:36:32  tibor]
 * 
 * Revision 1.1.4.16  1998/04/28  03:10:20  tibor
 * 	separated region definitions from EEPROM specific data, removed tigon specific references where possible
 * 	[1998/04/28  02:59:18  tibor]
 * 
 * Revision 1.1.4.15  1998/04/18  21:28:11  hayes
 * 	checkpoint
 * 	[1998/04/16  22:33:41  hayes]
 * 
 * Revision 1.1.4.14  1998/03/12  05:18:37  tibor
 * 	oem area needs to be eeprom size computation
 * 	[1998/03/12  05:16:14  tibor]
 * 
 * Revision 1.1.4.13  1998/03/07  23:14:44  hayes
 * 	add OEM area, add pointer to OEM area in mfg area
 * 	[1998/03/07  23:12:53  hayes]
 * 
 * Revision 1.1.4.12  1998/02/27  05:27:23  tibor
 * 	added model info field
 * 	[1998/02/27  05:22:34  tibor]
 * 
 * Revision 1.1.4.11  1997/12/19  07:21:50  hayes
 * 	add pci subsystem id and subsystem vendor id
 * 	[1997/12/19  07:21:35  hayes]
 * 
 * Revision 1.1.4.10  1997/12/16  22:33:21  ted
 * 	Add new OEM_PARTNER vendor ids
 * 	[1997/12/16  22:33:10  ted]
 * 
 * Revision 1.1.4.9  1997/11/22  04:00:29  tibor
 * 	32K EEPROM support
 * 	[1997/11/22  04:00:06  tibor]
 * 
 * Revision 1.1.4.8  1997/11/04  19:03:42  tibor
 * 	updated for 32k eeprom
 * 	[1997/11/04  18:35:30  tibor]
 * 
 * Revision 1.1.4.7  1997/08/01  00:15:54  ted
 * 	Add #defines for preventing NIC piracy
 * 	[1997/08/01  00:13:37  ted]
 * 
 * Revision 1.1.4.6  1997/06/30  16:20:03  wayne
 * 	Document diff between EEPROM_SIZE and EEPROM_WORDS
 * 	[1997/06/30  16:16:11  wayne]
 * 
 * Revision 1.1.4.5  1997/06/29  20:49:58  nimisha
 * 	Now that tigon also uses eeprom, define the eeprom size in present switch/nic HW
 * 	[1997/06/29  20:49:45  nimisha]
 * 
 * Revision 1.1.4.4  1997/05/08  01:41:55  dbender
 * 	Changed serial_number in eeprom_manufact_area_t from tg_madaddr_t to char[8].
 * 
 * 	Using a tg_macaddr_t for the serial number changed the alignment of the
 * 	entire structure because it was a U64.  Either we had to change the type
 * 	of the serial_number to something that doesn't require double-word alignment,
 * 	or remove and re-program *all* serial eeproms currently in use!  We chose
 * 	the former.  It is recommended that the serial_number be copied into a
 * 	tg_macaddr_t as soon as it is read out of serial eeprom.
 * 	[1997/05/08  01:41:43  dbender]
 * 
 * Revision 1.1.4.3  1997/04/19  01:06:34  nimisha
 * 	changed boot_rev from ascii to number.
 * 	[1997/04/19  01:06:23  nimisha]
 * 
 * Revision 1.1.4.2  1997/04/10  22:00:43  wayne
 * 	Add comments about where eeprom is found in flash region
 * 	[1997/04/10  22:00:37  wayne]
 * 
 * Revision 1.1.4.1  1997/03/26  23:40:11  dbender
 * 	Added new fields to manufact_area_t.  boot_rev, length, cpu_clk, board_rev, pci_dev_id, pci_vend_id,
 * 	and reserved fields are all new.
 * 	[1997/03/23  23:29:12  dbender]
 * 
 * Revision 1.1.2.5  1997/01/20  20:17:09  nimisha
 * 	added ifdef to increase the size of the EEPROM_WORDS define for flash for TS.
 * 	[1997/01/20  20:16:51  nimisha]
 * 
 * Revision 1.1.2.4  1997/01/20  19:52:38  nimisha
 * 	merge problem - resolving it.
 * 	Changed the macaddr to have the right struct type.
 * 	[1997/01/20  19:52:23  nimisha]
 * 
 * Revision 1.1.2.3  1997/01/18  00:23:35  nimisha
 * 	Merged with changes from 1.1.2.2
 * 	[1997/01/18  00:23:30  nimisha]
 * 
 * Revision 1.1.2.3  1997/01/18  00:20:14  nimisha
 * 	typedef strcut for macaddr equivalent
 * 
 * Revision 1.1.2.2  1997/01/18  00:11:46  nimisha
 * 	changed the macaddr to tg_macaddr_t to conform to xxx_conf.h
 * 	[1997/01/18  00:11:27  nimisha]
 * 
 * Revision 1.1.2.1  1996/12/04  19:28:50  hayes
 * 	Initial check in from Dave's eeprom.h
 * 	[1996/12/04  19:28:37  hayes]
 * 
 * $EndLog$
 */


/***************************************************************************
 *
 *  Name :	eeprom.h
 *
 *  		EEPROM structure.
 *
 *  Detail :	This structure is intended to be used as the eeprom structure
 *		for the tigon NIC, tigon switch, genius products.
 *
 *		The tigon eeprom is broken into several areas; the bootstrap
 *		area, code offset area, manufacturing area, and program area.
 *		  The bootstrap area contains a magic value, and entries
 *		describing where program 0 starts in eeprom, should
 *		be loaded in sram, and its length.
 *		   The code offset area is an array of structures which give
 *		information similar to that in the bootstrap area but for 
 *		programs 1 up to 8.  Each entry consists of a start address
 *		in eeprom, a destination address in sram and a length.
 *		  The manufacturing area contains various revision information
 *		about the board and the tigon chip.
 *		  The program area contains programs that are loaded in
 *		sequence after program 0 is finished.  Each program performs
 *		its function and then loads the next stage.
 *
 *		Each of the bootstrap, manufacturing and program areas has a
 *		32-bit CRC checksum as its last field -in the case of the
 *		program area, each program is followed by a 32-bit CRC.
 *
 *  $Source: /proj/irix6.5.7m/isms/irix/kern/bsd/sys/RCS/tigon_eeprom.h,v $
 *  $Date: 1999/05/18 21:44:02 $
 *
 ***************************************************************************/
#ifndef _EEPROM_H_ 
#define _EEPROM_H_

#include "tigon_media_regions.h"

/* no reason for this except historical.  Long ago there was a flash part in
 * which first 16 bytes couldn't be read.  Since EEPROM and flash were both
 * read by ROM the layout had to be the same.
 */
#define EEPROM_RESERVED_SIZE	4

/***************************************************************************
 *	eeprom_program_region_t
 ***************************************************************************/

/*
 * The serial EEPROM itself can be 8K/32K bytes,
 */

#if defined(EEPROM_8K)
#define EEPROM_SIZE		8192     /* tigon1 nic size */
#elif !defined(EEPROM_4K)
#define EEPROM_SIZE		32768    /* tigon2 nic eeprom */
#else
#define EEPROM_SIZE		4096     /* cost reduced nic */
#endif /* defined(EEPROM_8K) */

typedef struct eeprom_program_region {
    U32 text[1];
} eeprom_program_region_t;

/***************************************************************************
 *	eeprom_layout_t
 ***************************************************************************/
typedef struct eeprom_layout {
    U32                        reserved[EEPROM_RESERVED_SIZE];
    media_bootstrap_region_t   bootstrap;
    media_code_offset_region_t code_offsets[MEDIA_MAX_LOAD_STAGES];
    media_manufact_region_t    manufact;
    media_oem_region_t         oem;
    eeprom_program_region_t     programs;	/* just place holder */
} eeprom_layout_t;

#endif /* _EEPROM_H_ */
