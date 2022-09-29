/*
 * COPYRIGHT NOTICE
 * Copyright (c) Alteon Networks, Inc. 1996, 1997 
 * All rights reserved
 * 
 */
/*
 * HISTORY
 * $Log: tigon_media_regions.h,v $
 * Revision 1.2  1999/05/18 21:44:02  trp
 * Upgrade to firmware level 12.3.9
 *
 * Revision cur_hayes/3 1999/03/26 22:34:54 hayes
 * 	fix comments
 * 
 * Revision cur_hayes/2 1999/03/26 19:27:31 hayes
 * 	add test for HW_NIC to manufacturing region structure
 * 
 * Revision sbrev6_tibor/6 1999/03/19 01:18:30 tibor
 * 	manuf date is now week and year
 * 
 * Revision sbrev6_tibor/4 1999/02/25 20:36:57 tibor
 * 	added HW defs back in for tigon to fix nic side
 * 
 * Revision sb_tibor/2 1999/02/25 11:06:09 tibor
 * 	changed tigon side to use HW_TS
 * 
 * Revision sbrev6_tibor/2 1999/02/25 09:58:12 tibor
 * 	changed define to check for HW_GS
 * 
 * Revision 1.1.2.8  1999/02/02  09:44:03  tibor
 * 	added diag test region.  This area must be avoided for any CRC calculations
 * 	[1999/02/02  08:23:10  tibor]
 *
 * Revision 1.1.2.7  1998/11/12  03:17:33  tibor
 * 	aligned data areas better and added more space to the partData field
 * 	[1998/11/12  03:02:50  tibor]
 * 
 * Revision 1.1.2.6  1998/10/29  02:21:22  hayes
 * 	add NETGEAR support
 * 	[1998/10/29  02:13:57  hayes]
 * 
 * Revision 1.1.5.1  1998/10/23  17:25:36  hayes
 * 	add NETGEAR support
 * 	[1998/10/23  16:50:13  hayes]
 * 
 * Revision 1.1.2.5  1998/07/16  21:39:34  tibor
 * 	documented why partData is 72 bytes
 * 	[1998/07/03  01:27:12  tibor]
 * 
 * Revision 1.1.2.4  1998/06/19  07:54:49  tibor
 * 	Merged with changes from 1.1.2.3
 * 	[1998/06/19  07:54:30  tibor]
 * 
 * 	fixed REV number
 * 	[1998/06/19  07:14:07  tibor]
 * 
 * Revision 1.1.2.3  1998/06/19  01:17:00  dbender
 * 	Added ifdef _IN_ASM_.  Moved MAGIC_VALUE define into forementioned region so
 * 	rom code can use it from assembly.
 * 	[1998/06/19  01:12:45  dbender]
 * 
 * Revision 1.1.2.2  1998/06/04  23:33:44  tibor
 * 	GNE manufacturiong region defintions, initial pass
 * 	[1998/06/04  23:32:08  tibor]
 * 
 * Revision 1.1.2.1  1998/04/28  03:10:21  tibor
 * 	initial rev
 * 	[1998/04/28  03:00:10  tibor]
 * 
 * $EndLog$
 */

#ifndef _MEDIA_REGIONS_H_ 
#define _MEDIA_REGIONS_H_
/***************************************************************************
 *
 * The following media region structures are used to define the contents
 * of eeprom and flash. The eeprom.h and flash.h files define the actual
 * layout of what regions and in what order.
 *
 **************************************************************************/
/**************************************************************************
 *	media_bootstrap_region_t
 *	THIS STRUCT CANNOT CHANGE!!!  ROM DEPENDS ON IT!
 ***************************************************************************/
#define MAGIC_VALUE	0x669955aa

#if !defined(_IN_ASM_)

typedef struct media_bootstrap_region {
    U32 magic_value;		/* a pattern not likely to occur randomly */

    U32 sram_start_addr;	/* where to locate boot code (byte addr) */
    U32 code_len;		/* boot code length (in words) */
    U32 code_start_addr;	/* location of code on media (media byte addr) */
    U32 cksum;			/* 32-bit CRC */
} media_bootstrap_region_t;

/**************************************************************************
 *	media_code_offset_area_t
 ***************************************************************************/
#define MEDIA_MAX_LOAD_STAGES	8
typedef struct media_code_offset_region {
    U32 sram_start_addr;	/* where to locate code region (byte addr) */
    U32 code_len;		/* code region length (in words) */
    U32 code_start_addr;	/* location of code on media (media byte addr) */
} media_code_offset_region_t;

/***************************************************************************
 *	media_manufact_region_t
 *
 * Comments for HW_100/180 definition:
 *
 *        the pci_dev_id and pci_vend_id fields are included for situations
 *        where 3rd party vendors might want their own values.  These
 *        values, if non-zero, are copied by the pci_init stage to pci
 *        registers - before the pci bios gets around to looking at them.
 *        These values could be hardcoded into the pci_init boot stage.  I
 *        have no religious feeling about this, though I prefer having
 *        constants like these in a dedicated structure in eeprom, as it
 *        requires no change to actual code when the constants change.
 *
 *	  Note -1- 	Using a tg_macaddr_t for the serial number changed
 *			the alignment of the entire structure because it was
 *			a U64.  Either we had to change the type of the
 *			serial_number to something that doesn't require
 *			double-word alignment, or remove and re-program *all*
 *			serial eeproms currently in use!  We chose the 
 *			former.  It is recommended that the serial_number be
 *			copied into a tg_macaddr_t as soon as it is read
 *			out of serial eeprom.
 *
 * Comments for Genius product line:
 *
 ***************************************************************************/
#if defined(HW_TS) || defined(HW_110) || defined(HW_180) || defined(HW_NIC)
#define MANUF_REGION_REV 'B'
typedef struct media_manufact_region {
    char         boot_rev;         /* region revision */
    U8           reserved1;        /* reserved, must be 0 */
    U16          length;           /* length of this structure in bytes */
    U16          cpu_clk;          /* cpu clock speed in Mhz rounded to the */
                                   /*  nearest integer */
    U16          modelType;        /* type of part */
    char         serial_number[8]; /* -1- media access control address */
    char         sw_key[16];       /* software key, used to enable */
				   /*  disable certain features. */
				   /* first byte tells the OEM vendor ID */
				   /* see below for list */
    char         part_number[16];  /* nic/switch part number. Printable */
                                   /*  string ending in '\0'. */
    char 	 board_rev[2];     /* Board revision.  Two printable ascii */
                                   /*  characters. */
    U16          reserved3;        /* reserved, must be 0 */
    U16          pci_dev_id;       /* alternate pci device id */
    U16          pci_vend_id;      /* alternate pci vendor id */
    U16          pci_sub_id;       /* alternate pci subsystem id */
    U16          pci_sub_vend_id;  /* alternate pci subsystem vendor id */
    U32		 oem_area_p;	   /* pointer to 32 byte opaque OEM area */
    U32		 reserved4[3];     /* space for future expansion. 12 bytes */
                                   /*  makes the structure size an even 80 */
    U32 cksum;			   /* 32-bit CRC */
} media_manufact_region_t;
#elif defined(HW_GS)

#define MANUF_REGION_REV 'C'
#define MEDIA_PARTDATA_BYTE_SIZE	188  /* bootstrap+offsets+mfg+oem = 374 bytes */

typedef struct media_manufact_region {
    char  boot_rev;	    /* region revision letter (ascii)*/
    U8    reserved1;	    /* reserved, must be 0 */
    U16   length;	    /* length of this structure in bytes */
    U16   reserved2;
    U16   partType;	    /* type of part information */
    char  serial_number[8]; /* serial number or MAC address */
    char  partNumber[16];   /* part number. Printable string ending in '\0'. */
    char  partRev[2];	    /* part revision.  Two printable ascii char */
    U16   reserved3;
    char  manufDate[4];	    /* ascii manufact date, wwyy, \0 by next field */
    U32	  oem_area_p;	    /* pointer to 32 byte opaque OEM area or 0*/ 
    U8    partData[MEDIA_PARTDATA_BYTE_SIZE];/* part specific info region */
    U32   cksum;	    /* 32-bit CRC */
} media_manufact_region_t;

/*
 * part type 0x01xx - 0x0fxx are tigon specific
 *	     0x10xx - 0x1fxx are genuis specific 
 */

#else
#error Invalid HW_SYMBOL
#endif

typedef struct media_oem_region {
    U8 data[32];
} media_oem_region_t;

typedef struct media_diag_region {
    U8 data[16];
} media_diag_region_t;

#endif /* !defined(_IS_ASM_) */

/* OEM vendor ids */
#define OEM_UNIVERSAL   0x00	   /* support legacy cards */
#define OEM_ALTEON_1    0x01	   /* Alteon boards, type 1 */
#define OEM_ALTEON_2    0x02	   /* Alteon boards, type 2 */
#define OEM_PARTNER1    0x03	   /* Alteon OEM partner 1 boards */
#define OEM_PARTNER2    0x04	   /* Alteon OEM partner 2 boards */
#define OEM_PARTNER3    0x05	   /* Alteon OEM partner 3 boards */
#define OEM_PARTNER4    0x06	   /* Alteon OEM partner 4 boards */
#define OEM_PARTNER5    0x07	   /* Alteon OEM partner 5 boards */
#define OEM_PARTNER6    0x08	   /* Alteon OEM partner 6 boards */
#define OEM_PARTNER7    0x09	   /* Alteon OEM partner 7 boards */
#define OEM_PARTNER8    0x0a	   /* Alteon OEM partner 8 boards */
#define OEM_PARTNER9    0x0b	   /* Alteon OEM partner 9 boards */
#define OEM_PARTNER10   0x0c	   /* Alteon OEM partner 10 boards */
#define OEM_PARTNER11   0x0d	   /* Alteon OEM partner 11 boards */
#define OEM_PARTNER12   0x0e	   /* Alteon OEM partner 12 boards */
#define OEM_PARTNER13   0x0f	   /* Alteon OEM partner 13 boards */
#define OEM_PARTNER14   0x10	   /* Alteon OEM partner 14 boards */
#define OEM_PARTNER15   0x11	   /* Alteon OEM partner 15 boards */
#define OEM_PARTNER16   0x12	   /* Alteon OEM partner 16 boards */

/* PCI Device Id, Vendor Id, Subsystem Id, Subsystem Vendor Id... */
#define OEM_UNIVERSAL_PCI_DEV_ID		0x0001
#define OEM_UNIVERSAL_PCI_VEND_ID		0x12ae
#define OEM_UNIVERSAL_PCI_SUBSYS_ID		0x0001
#define OEM_UNIVERSAL_PCI_SUBSYS_VEND_ID	0x12ae

#define OEM_ALTEON_1_PCI_DEV_ID			0x0001
#define OEM_ALTEON_1_PCI_VEND_ID		0x12ae
#define OEM_ALTEON_1_PCI_SUBSYS_ID		0x0000
#define OEM_ALTEON_1_PCI_SUBSYS_VEND_ID		0x0000

#define OEM_ALTEON_2_PCI_DEV_ID			0x0001
#define OEM_ALTEON_2_PCI_VEND_ID		0x12ae
#define OEM_ALTEON_2_PCI_SUBSYS_ID		0x0000
#define OEM_ALTEON_2_PCI_SUBSYS_VEND_ID		0x0000

#define OEM_PARTNER1_PCI_DEV_ID			0x0001
#define OEM_PARTNER1_PCI_VEND_ID		0x12ae
#define OEM_PARTNER1_PCI_SUBSYS_ID		0x0000
#define OEM_PARTNER1_PCI_SUBSYS_VEND_ID		0x0000

#define OEM_PARTNER2_PCI_DEV_ID			0x0001
#define OEM_PARTNER2_PCI_VEND_ID		0x10b7
#define OEM_PARTNER2_PCI_SUBSYS_ID		0x0000
#define OEM_PARTNER2_PCI_SUBSYS_VEND_ID		0x0000

#define OEM_PARTNER3_PCI_DEV_ID			0x0001
#define OEM_PARTNER3_PCI_VEND_ID		0x12ae
#define OEM_PARTNER3_PCI_SUBSYS_ID		0x600a
#define OEM_PARTNER3_PCI_SUBSYS_VEND_ID		0x1011

#define OEM_PARTNER4_PCI_DEV_ID			0x0001
#define OEM_PARTNER4_PCI_VEND_ID		0x12ae
#define OEM_PARTNER4_PCI_SUBSYS_ID		0x106f
#define OEM_PARTNER4_PCI_SUBSYS_VEND_ID		0x103c

#define OEM_PARTNER5_PCI_DEV_ID			0x0001
#define OEM_PARTNER5_PCI_VEND_ID		0x12ae
#define OEM_PARTNER5_PCI_SUBSYS_ID		0x0000
#define OEM_PARTNER5_PCI_SUBSYS_VEND_ID		0x0000

#define OEM_PARTNER6_PCI_DEV_ID			0x0009
#define OEM_PARTNER6_PCI_VEND_ID		0x10a9
#define OEM_PARTNER6_PCI_SUBSYS_ID		0x0000
#define OEM_PARTNER6_PCI_SUBSYS_VEND_ID		0x0000

#define OEM_PARTNER7_PCI_DEV_ID			0x0001
#define OEM_PARTNER7_PCI_VEND_ID		0x12ae
#define OEM_PARTNER7_PCI_SUBSYS_ID		0x0000
#define OEM_PARTNER7_PCI_SUBSYS_VEND_ID		0x0000

#define OEM_PARTNER8_PCI_DEV_ID			undefined
#define OEM_PARTNER8_PCI_VEND_ID		undefined
#define OEM_PARTNER8_PCI_SUBSYS_ID		undefined
#define OEM_PARTNER8_PCI_SUBSYS_VEND_ID		undefined

#define OEM_PARTNER9_PCI_DEV_ID			undefined
#define OEM_PARTNER9_PCI_VEND_ID		undefined
#define OEM_PARTNER9_PCI_SUBSYS_ID		undefined
#define OEM_PARTNER9_PCI_SUBSYS_VEND_ID		undefined

#define OEM_PARTNER10_PCI_DEV_ID		undefined
#define OEM_PARTNER10_PCI_VEND_ID		undefined
#define OEM_PARTNER10_PCI_SUBSYS_ID		undefined
#define OEM_PARTNER10_PCI_SUBSYS_VEND_ID	undefined

#define OEM_PARTNER11_PCI_DEV_ID		undefined
#define OEM_PARTNER11_PCI_VEND_ID		undefined
#define OEM_PARTNER11_PCI_SUBSYS_ID		undefined
#define OEM_PARTNER11_PCI_SUBSYS_VEND_ID	undefined

#define OEM_PARTNER12_PCI_DEV_ID		undefined
#define OEM_PARTNER12_PCI_VEND_ID		undefined
#define OEM_PARTNER12_PCI_SUBSYS_ID		undefined
#define OEM_PARTNER12_PCI_SUBSYS_VEND_ID	undefined

#define OEM_PARTNER13_PCI_DEV_ID		undefined
#define OEM_PARTNER13_PCI_VEND_ID		undefined
#define OEM_PARTNER13_PCI_SUBSYS_ID		undefined
#define OEM_PARTNER13_PCI_SUBSYS_VEND_ID	undefined

#define OEM_PARTNER14_PCI_DEV_ID		0x620a
#define OEM_PARTNER14_PCI_VEND_ID		0x1385
#define OEM_PARTNER14_PCI_SUBSYS_ID		0x0001
#define OEM_PARTNER14_PCI_SUBSYS_VEND_ID	0x1385

#define OEM_PARTNER15_PCI_DEV_ID		undefined
#define OEM_PARTNER15_PCI_VEND_ID		undefined
#define OEM_PARTNER15_PCI_SUBSYS_ID		undefined
#define OEM_PARTNER15_PCI_SUBSYS_VEND_ID	undefined

#define OEM_PARTNER16_PCI_DEV_ID		0x0009
#define OEM_PARTNER16_PCI_VEND_ID		0x10a9
#define OEM_PARTNER16_PCI_SUBSYS_ID		0x8002
#define OEM_PARTNER16_PCI_SUBSYS_VEND_ID	0x10a9

#endif /* _MEDIA_REGIONS_H_ */
