/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __IO6PROM_ED_H__
#define __IO6PROM_ED_H__

#ident "IO6prom/ed.h : $Revision: 1.7 $"

#include <sys/SN/nvram.h>

#define NVOFF_EDHDR		NVOFF_INVENTORY
#define NVLEN_EDHDR		NVLEN_INVENTORY
#define NVOFF_EDREC		(NVOFF_INVENTORY + 64) 

#define NVEDTAB_MAGIC		0x39
#define NVEDTAB_VALID    	0x1
#define NVEDTAB_INVALID  	0

#define NVEDREC_VALID    	0x1
#define NVEDREC_BRDDIS          0x2

#define NVEDRSTATE_DISABLE	0x1
#define NVEDRSTATE_ENABLE	0x2

typedef struct edtabhdr_s {
	unsigned char	magic ;
	unsigned char 	valid ;
	int		cnt ;
	nic_t		nic ;
} edtabhdr_t ;

typedef struct edrec_s {
	unsigned char 	flag ;
	moduleid_t	modid ;
	slotid_t	slotid ;
	unsigned char	state[MAX_COMPTS_PER_BRD] ;
} edrec_t ;

/* ------------------- Inventory stuff ------------------------- */

#define MAX_BARCODE		12

#define NVOFF_INVHDR            NVOFF_PNT
#define NVLEN_INVHDR            NVLEN_PNT
#define NVOFF_INVREC            (NVOFF_INVHDR + 64)

#define NVINV_MAGIC           	0x39
#define NVINV_VALID           	0x1
#define NVINV_INVALID         	0

#define NVINVREC_VALID          0x1		/* record valid ? */
#define NVINVREC_DAUGHTER       0x2		/* Is it a daughter brd ? */
#define NVINVREC_COMPONENT      0x4		/* Is it a component ? */

typedef struct invtabhdr_s {
    unsigned char	magic ;
    unsigned char 	valid ;
    unsigned char	tver ;
    int			cnt ;
    nic_t		nic ;
    unsigned long long  modmap ;
} invtabhdr_t ;

/* The size of the record is 32 bytes and the 4k area can hold
   126 records. This is barely sufficient for typical 128p system.
   64 node + 32 routers + 8 meta + 16 io boards 
   To add more fields or more records, reserve a 2nd inventory area
   before current NVLOWMEM, Add parallel structs etc...
*/

typedef struct invrec_s {
    unsigned char	flag ;
    unsigned char	btype ;
    unsigned char	modnum ;
    unsigned char	slotnum ;
    unsigned char	diagval  ;
    unsigned char	diagparm ;
    unsigned char 	physid ;
    unsigned char 	pad1 ;
    nic_t		nic ;
    unsigned int	cmap ;
    char		barcode[MAX_BARCODE] ;
} invrec_t ;

#define CMAP_CPUA_SHIFT 	0
#define CMAP_CPUB_SHIFT 	(CMAP_CPUA_SHIFT + 1)
#define CMAP_MEMBNK_SHIFT 	(CMAP_CPUB_SHIFT + 1)
#define CMAP_PCI_SHIFT 		(CMAP_MEMBNK_SHIFT + MD_MEM_BANKS)

/*
 * Defines for command line parsing.
 */

#ifdef __ED_C__

#define ENABLE 			1
#define DISABLE 		0

#endif

#define UNKNOWN_MODE		0
#define SLOT_MODE		2
#define COMPT_MODE		3

#define MAX_CMD_STRING_LENGTH	32

#define MODE_UNKNOWN		0x0
#define MODE_DEBUG		0x1
#define MODE_SLOT		0x2
#define MODE_COMPT		0x4
#define MODE_CPU_MSK		0x8

#define ED_MODE_UNKNOWN(_ci)	(_ci->cmdmde&MODE_UNKNOWN)
#define ED_MODE_DEBUG(_ci)	(_ci->cmdmde&MODE_DEBUG)
#define ED_MODE_SLOT(_ci)	(_ci->cmdmde&MODE_SLOT)
#define ED_MODE_COMPT(_ci)	(_ci->cmptyp)

/* Warning: Do not change the order of these commands or
 * their absolute values for now.
 */
typedef enum {
    CMD_ENABLE,
    CMD_DISABLE
} CMD_TYPES ;

typedef __uint32_t		edflag_t ;
typedef __uint32_t		edmask_t ;

#define CMSK_CPUA_SHIFT 	0
#define CMSK_CPUA		(1 << CMSK_CPUA_SHIFT)
#define CMSK_CPUB_SHIFT 	(CMSK_CPUA_SHIFT + 1)
#define CMSK_CPUB		(1 << CMSK_CPUB_SHIFT)
#define CMSK_MEMBNK_SHIFT 	(CMSK_CPUB_SHIFT + 1)
#define CMSK_PCI_SHIFT 		(CMSK_MEMBNK_SHIFT + MD_MEM_BANKS)
#define CMSK_NEW_SHIFT 		(CMSK_PCI_SHIFT + MAX_PCI_DEVS)

#define CMSK_CPU		((edmask_t)(~((edmask_t)-1 << CMSK_MEMBNK_SHIFT)))
#define CMSK_MEM		((edmask_t)(~((edmask_t)-1 << CMSK_PCI_SHIFT))) &\
				~CMSK_CPU
#define CMSK_PCI		((edmask_t)(~((edmask_t)-1 << CMSK_NEW_SHIFT))) &\
				~(CMSK_CPU|CMSK_MEM)

/*
 * info collected from the command line.
 */

typedef struct cmdinf_s {
    int		cmdtyp ;
    char	cmdstr[MAX_CMD_STRING_LENGTH] ;
    moduleid_t	mod    ;
    int		slt    ;
    char	sltstr[MAX_CMD_STRING_LENGTH] ;
    uchar	cmptyp ;
    edmask_t	cmpmsk ;
    edflag_t	cmdmde ;
    lboard_t	*lbd   ;
    edrec_t	*edr   ;
} cmdinf_t ;

#ifdef __ED1_C__

#define ENABLE(_ci)	(_ci->cmdtyp == CMD_ENABLE)
#define DISABLE(_ci)	(_ci->cmdtyp == CMD_DISABLE)

#endif

/*
 * This enum struct is tied to the edfunc array defined
 * in ed.c. If you add a new func to edfunc you also
 * should add a code at the right place here.
 * NOTE: Enable and disable functions should go in pairs
 * with enable coming first.
 */

typedef enum {
    CMD_E_NODE,
    CMD_D_NODE,
    CMD_E_IO,
    CMD_D_IO,
    CMD_E_MB,
    CMD_D_MB
} CMD_FUNC_CODES ;

extern int	enable_node(cmdinf_t *) ;
extern int	disable_node(cmdinf_t *) ;
extern int	enable_io(cmdinf_t *) ;
extern int	disable_io(cmdinf_t *) ;
extern int	enable_mb(cmdinf_t *) ;
extern int	disable_mb(cmdinf_t *) ;

typedef struct memlog_s {
    char 	*str ,
		*sze ,
		*rsn ;
    edmask_t	msk  ;
} memlog_t ;

typedef struct edrinf_s {
    int		inv,
		mtc,
		end,
		use ;
    int		cnt ;
} edrinf_t ;

#define MASK_BIT	((edmask_t)1)

#define MAX_HWG_PATH_LEN	128

#endif /* __IO6PROM_ED_H__ */

