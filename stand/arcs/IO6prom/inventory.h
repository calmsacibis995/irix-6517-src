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

#ifndef __IO6PROM_INVENTORY_H__
#define __IO6PROM_INVENTORY_H__

#ident "IO6prom/inventory.h : $Revision: 1.8 $"

#include <sys/SN/nvram.h>

#define MAX_BARCODE		12

#define NVOFF_INVHDR            NVOFF_PNT
#define NVLEN_INVHDR            NVLEN_PNT
#define NVOFF_INVREC            (NVOFF_INVHDR + 64)
#define NVLEN_INVREC		(NVLEN_PNT - 64)

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

/* The following inline will calculate the offset into the PNT or the
 * PNT overflow area, depending on the supplied count.
 */

static __inline ssize_t
calc_pnt_nvoff(int count) {
    size_t offset = NVOFF_INVREC + (count*sizeof(invrec_t));

    if (!VALID_PNT_NVOFF(offset+sizeof(invrec_t)-1)) {
	offset=NVOFF_PNT_OVERFLOW+((count-(NVLEN_INVREC/sizeof(invrec_t)))*
	    sizeof(invrec_t));
	if (offset+sizeof(invrec_t) >= NVOFF_SN0_HIGHFREE) {
	    printf("***Warning: NVRAM PNT area is full\n");
	    return(-1);
	}
    }
    return(offset);
}

#define CMAP_CPUA_SHIFT 	0
#define CMAP_CPUB_SHIFT 	(CMAP_CPUA_SHIFT + 1)
#define CMAP_MEMBNK_SHIFT 	(CMAP_CPUB_SHIFT + 1)
#define CMAP_PCI_SHIFT 		(CMAP_MEMBNK_SHIFT + MD_MEM_BANKS)

#endif /* __IO6PROM_INVENTORY_H__ */
