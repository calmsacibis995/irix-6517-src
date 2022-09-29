/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
/*
 * klhwinit.h - Macros, defines and structs used during IO6prom init.
 *		Some of the stuff in here may later get put into
 *	 	other files.
 */
#ifndef __SYS_SN_SN0_KLHWINIT_H__
#define __SYS_SN_SN0_KLHWINIT_H__

#include <sys/SN/SN0/klhwinit_gfx.h>  /* gfx hw - part & widget numbers */

typedef __uint32_t      reg32_t ;
typedef __uint64_t      reg64_t ;
#define         uchar   unsigned char

#define LD(x) (*(volatile reg64_t  *)(x))
#define LD32(x) (*(volatile reg32_t  *)(x))
#define LDU(x) (*(volatile unsigned long  *) (x))

#define SD(x,v) (LDU(x) = (unsigned long ) (v))
#define SD32(x,v) (LD32(x) = (reg32_t) (v))

#define GET_FIELD32(addr, mask, shift) \
		(((*(volatile reg32_t *)(addr)) >> shift) & mask)

#define GET_FIELD64(addr, mask, shift) \
		(((*(volatile reg64_t *)(addr&~((__psunsigned_t)7))) \
					>> shift) & mask)


#define PUT_FIELD32(addr, mask, shift, value) \
	 	*(volatile reg32_t *)addr =   \
		((*(volatile reg32_t *)(addr)) & (~(mask<<shift))) | \
		((value&mask)<<shift)

#define PUT_FIELD64(addr, mask, shift, value) \
	 	*(volatile reg64_t *)addr =   \
		((*(volatile reg64_t *)(addr)) & (~(mask<<shift))) | \
		((value&mask)<<shift)

#define BRIDGE_PROM_MAGIC 0xfeed


typedef	struct bridge_promhdr_s {
	__uint32_t	magic ;
	__uint32_t	board_type  ;
	unsigned char   name[32]  ;
	__uint32_t	hdr_length ;
	__uint32_t	prom_type ;
	__uint32_t	revision ;
	__uint32_t	compression ;
	__uint32_t	capability ;
} bridge_promhdr_t ;

typedef struct bridge_promdrv_s { /* driver header */
	__uint32_t	driver_type ;
	__uint32_t	code_type ;
	__uint32_t	revision ;
	__uint32_t	offset ;
	__uint32_t	length ;
} bridge_promdrv_t ;


#define BRIDGE_NIC_FIELD_LEN	32

typedef struct bridge_nichdr_s {
	struct bridge_nichdr_s 	*next ;
	__psunsigned_t	board_partnum ;
	__psunsigned_t	prom_offset ;
	__psunsigned_t	bridge_base ;
	char 		Part[BRIDGE_NIC_FIELD_LEN] ;
	char 		Name[BRIDGE_NIC_FIELD_LEN] ;
	char 		Serial[BRIDGE_NIC_FIELD_LEN] ;
	char 		Revision[BRIDGE_NIC_FIELD_LEN] ;
	char 		Group[BRIDGE_NIC_FIELD_LEN] ;
	char 		Capability[BRIDGE_NIC_FIELD_LEN] ;
	char 		Variety[BRIDGE_NIC_FIELD_LEN] ;
	char 		Laser[BRIDGE_NIC_FIELD_LEN] ;
	char 		*nic_string ;
} bridge_nichdr_t ;

typedef bridge_nichdr_t nichdr_t ;

#define IP27_PART_NUM     "030-0733-"
#define MIDPLANE_PART_NUM "013-1547-"
#define BASEIO_PART_NUM "030-0734-"
#define BASEIO_SERVER_PART_NUM "030-1124-"
#define MSCSI_PART_NUM  "030-0872-"
#define MSCSI_B_PART_NUM  "030-1243-"
#define MSCSI_B_O_PART_NUM  "030-1281-"
#define MENET_PART_NUM  "030-0873-"
#define MIO_PART_NUM    "030-0880-"
#define FC_PART_NUM     "030-0927-"
#define HAROLD_PART_NUM "030-1062-"	/* O2K PCI Shoebox */
#define XTALK_PCI_PART_NUM "030-1275-"	/* PCI Shoehorn */
#define LINC_PART_NUM   "030-9999-"  /* XXX Dummy number, needs to be changed */
#define SN00_PART_NUM	"030-1025-"
#define SN00A_PART_NUM	"030-1244-" /* IP29 board from after 5/1997 */
#define SN00B_PART_NUM	"030-1291-" 
#define SN00C_PART_NUM	"030-1308-" 
#define SN00D_PART_NUM	"030-1389-"
#define HIPPI_S_PART_NUM	"030-0968-"
#define HPCEX_PART_NUM	"030-0956-"

/* Part numbers corresponding to the speedo xbox */
#define XBOX_BRIDGE_PART_NUM 		"030-1250-"
#define XBOX_MIDPLANE_PART_NUM		"030-1251-"
#define XBOX_XTOWN_PART_NUM		"030-1252-"
#define SN00_XTOWN_PART_NUM		"030-1264-"

#define WIDGET_ID_MASK   0xf

#define SGI_IOC3		0x310a9
#define	SGI_LINC		0x210a9
#define	SGI_HPCEX   		0x610a9
#define SGI_RAD			0x510a9
#define QLOGIC_SCSI		0x10201077
#define SABLE_DISK		0x0300fafe
#define UNIVERSE_VME		0x10e3 
#define FIBERCHANNEL		0x11609004
#define PSITECH_RAD1            0x00093d3d

#define PCI_CFG_BASE_ADD_REGS 6

typedef struct pcicfg_s {
	unsigned short 	vendor_id ;
	unsigned short 	device_id ;
	unsigned short 	command ;
	unsigned short 	status ;
	__uint32_t      class_rev ;
	unsigned char   cache_line_size ;
	unsigned char   latency_timer ;
	unsigned char   header_type ;
	unsigned char   bist ;
	__uint32_t 	base_addr_regs[PCI_CFG_BASE_ADD_REGS] ;
	__uint32_t 	cardbus_cis_ptr ;
	unsigned short 	sub_sys_vendor_id ;
	unsigned short	sub_sys_id ;
	__uint32_t	expansion_rom ;
	__uint32_t	reserved1 ;
	__uint32_t	reserved2 ;
	unsigned char 	interrupt_line ;
	unsigned char 	interrupt_pin ;
	unsigned char 	min_grant ;
	unsigned char 	max_latency ;
} pcicfg_t ;

/* this is for devices that do not support byte reads of their
   config space  like ioc3
*/

typedef struct pcicfg32_s {
	__uint32_t 	pci_id ;	
	__uint32_t 	pci_scr ;	
	__uint32_t 	pci_rev ;	
	__uint32_t 	pci_lat ;	
	__uint32_t 	pci_addr ;	
	__uint32_t	pad_0[11] ;
	__uint32_t 	pci_erraddrl ;	
	__uint32_t 	pci_erraddrh ;	
} pcicfg32_t ;

struct sa_pci_ident {
	u_int32_t	vendor_id;		/* Vendor ID */
	u_int32_t	device_id;		/* Device ID */
	__int32_t	revision_id;		/* Revision ID */
        int             (*inst_fptr)() ;        /* Install function ptr */
};
typedef struct sa_pci_ident sa_pci_ident_t;

typedef struct xwidget_id_s {
	__uint32_t	rev_num:4,
			part_num:16,
			mfg_num:12 ;
} xwidget_id_t ;

#define IOC3_DEV_SEL_0 0x80000

#define IOC3_NVRAM_OFFSET IOC3_DEV_SEL_0


/*
 * These defines are here till SABLE is fixed with support for
 * all hub registers and bridge, xbow etc.
 */

#define SABLE_BRIDGE_WID  8
#define SABLE_IP27_SLOTID 2

#endif /* __SYS_SN_SN0_KLHWINIT_H__ */
