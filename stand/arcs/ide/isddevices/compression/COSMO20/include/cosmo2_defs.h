
/*
 * cgi1_def.h 
 *
 * Cosmo2 gio 1 chip hardware definitions
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
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ifndef _CGI1_DEF_H_
#define _CGI1_DEF_H_

#include "sys/cgi1.h"
#include "sys/cgi1_fifos.h"
#include "sys/cosmo2_vc.h"
#include "sys/cosmo2_mcu_cmd.h"
#include "sys/cosmo2_cbar.h"
#include "sys/cosmo2_i2c.h"
#include "sys/cosmo2_cc1.h"
#include "cosmo2_dma_def.h"
#include "sys/cosmo2_asm.h"

#include "sys/types.h"
#include "ide_msg.h"
#include "uif.h"
#include "libsc.h"
#include <sgidefs.h>
#include "sys/cpu.h"
#include "sys/sbd.h"

#if defined(IP30)
#include "godzilla/include/d_bridge.h" 
#include "godzilla/include/d_godzilla.h"  
#include "sys/GIO/giobr.h"     /* for all the structure defs */
#include "sys/PCI/bridge.h"
#include "sys/xtalk/xbow.h"
#include "ide_xbow.h"

#define DEVICE_DMA_ATTRS_CMD                    ( \
        /* BRIDGE_DEV_VIRTUAL_EN */ 0           | \
        /* BRIDGE_DEV_RT */ 0                    | \
        /* BRIDGE_DEV_PREF */ 0                           | \
        /* BRIDGE_DEV_PRECISE */ 0                      | \
        BRIDGE_DEV_COH                          | \
        /* BRIDGE_DEV_BARRIER */ 0              | \
        BRIDGE_DEV_GBR                          )

/*
 * DMA Dirmap attributes for Data channel
 */
#define DEVICE_DMA_ATTRS_DATA                   ( \
        /* BRIDGE_DEV_VIRTUAL_EN */ 0           | \
        BRIDGE_DEV_RT                     | \
        BRIDGE_DEV_PREF                            | \
        /* BRIDGE_DEV_PRECISE */ 0                      | \
        BRIDGE_DEV_COH                          | \
        /* BRIDGE_DEV_BARRIER */ 0              | \
        BRIDGE_DEV_GBR                          )

#define GIOBR_ARB_RING_BRIDGE   (BRIDGE_ARB_REQ_WAIT_TICK(2) | \
                                 BRIDGE_ARB_REQ_WAIT_EN(0xFF) | \
                                 BRIDGE_ARB_HPRI_RING_B0 )

#define GIOBR_DEV_CONFIG(flgs, dev)     ( (flgs) | (dev) )
#define LSBIT(word)             ((word) &~ ((word)-1))

#endif


typedef unsigned long long ULL ;
typedef unsigned short UWORD;
typedef unsigned char UBYTE;

#if (defined(IP28) || defined(IP30))
#define __paddr_t       __uint64_t
#else
#define __paddr_t       __uint32_t
#endif


/*
#ifndef PHYS_TO_K1
#define PHYS_TO_K1(x)   ((unsigned)(x)|0xA0000000)
#endif 
*/

#define CHANNEL_0	0
#define CHANNEL_1	1
#define CHANNEL_2	2
#define CHANNEL_3	3

#define FROM_GIO 0
#define TO_GIO 	1

#define SIZEOF_FIFOS	512	
#define CMD_SYNC	0x55aa
#define _3bit_mask      0x7
#define _4bit_mask      0xf
#define _5bit_mask      0x1f
#define _6bit_mask      0x3f
#define _7bit_mask      0x7f
#define _8bit_mask      0xff
#define _9BIT_MASK 	0x1ff 
#define _13bit_mask 	0x1fff 
#define _16bit_mask 	0xffff 
#define _32bit_mask 	0xffffffff 
#define _64bit_mask 	(0xffffffffffffffffLL)
#define _BCR_mask   	0x7ffe 
#define DATA_FIFO_MASK  (0x01ff01ff01ff01ffLL)

#define WALK_64_SIZE    128
#define WALK_32_SIZE    64
#define WALK_16_SIZE    32
#define WALK_8_SIZE     16

#define PATRNS_64_SIZE 5+WALK_64_SIZE 
#define PATRNS_32_SIZE 5+WALK_32_SIZE 
#define PATRNS_16_SIZE 5+WALK_16_SIZE 
#define PATRNS_8_SIZE 5+WALK_8_SIZE 

#define MAX_XTALK_PORT_INFO_INDEX 7

#undef REPORT_PASS_OR_FAIL

#define REPORT_PASS_OR_FAIL(Test, errors)\
        if (!errors) {\
                msg_printf(SUM, "---- %s passed. ---- \n" ,Test->TestStr);\
                return (1);\
        } else {\
                msg_printf(ERR, " %s failed. ErrCode %s Errors %d\n", Test->TestStr, Test->ErrStr, errors);\
                return (0);\
	} 

#define MAXBUFSIZE	640*480

#define  RDONLY 	0	/* Read Only */
#define  WRONLY 	1	/* Write Only */
#define  RDWR 		2	/* Read/Write */


#define CGI1_PRODUCT_ID_DEF  		(cgi1_cosmo_id_alone | cgi1_cosmo_id_valid | cgi1_cosmo_id_gio_64)

#define CGI1_BCR_MASK		0x8fff
#define CGI1_BCR_DEF		0x00000000
#define CGI1_CH0_TBLPTR_DEF	0x00000000
#define CGI1_CH1_TBLPTR_DEF	0x00000000
#define CGI1_CH2_TBLPTR_DEF	0x00000000
#define CGI1_CH3_TBLPTR_DEF	0x00000000

#define CGI1_DMA_CONTROL_DEF		0x00000000
#define CGI1_INTERRUPT_STAT_DEF		0x00000000
#define CGI1_INTERRUPT_MASK_DEF		0xffffffff
#define CGI1_DMA_OFF_TIME_DEF		0x0000
#define CGI1_DMA_STAT_DEF		0x0000
#define CGI1_FIFO_RW_0_DEF		0x0000
#define CGI1_FIFO_RW_1_DEF		0x0000
#define CGI1_FIFO_RW_2_DEF		0x0000
#define CGI1_FIFO_RW_3_DEF		0x0000
#define CGI1_FIFO_FLAGS_RW__DEF		0x08

#define SIZE_OF_HUFF_TABLE                      420
#define SIZE_OF_QUANT_TABLE                     134
#define SIZE_OF_COM                             64 



#define C2_CGI_ENABLE_PIO_MODE(value) {						\
		*(cgi1base+value) |= (cgi1_EnablePIO & cgi1_EnablePIOMask) ;    \
		msg_printf(DBG, "enabled PIO \n") ;				\
}

#define C2_CGI1_PUT_REG(offset, value, mask) { 							\
	*(cgi1base + offset) = value & mask ;							\
	msg_printf(DBG,"C2_CGI1_PUT_REG: offset %x value %x , mask %x \n",				\
					*(cgi1base + (offset)), value , mask   );			\
}

#if 0
#define C2_CGI1_GET_REG( rdreg, offset, mask) {							\
	msg_printf(DBG,"C2_CGI1_GET_REG: register %x  \n", *(cgi1base + offset)  );		\
	rdreg = ( *(cgi1base + offset) & mask) ;						\
}
#endif

#define CGI1_COMPARE(str, offset, recv, expt, mask) {						\
		msg_printf(VRB, "CGI1_COMPARE : %s addr %x rcv %x exp %x \n", str, offset, (recv & mask), (expt & mask) ); \
	if ((recv & mask) != (expt & mask)) { 							\
		msg_printf(ERR, "CGI1_COMPARE_INT ERROR : %s addr %x rcv %x exp %x \n", str, offset, (recv & mask), (expt & mask) ); \
		G_errors++;	return (0);								\
	}											\
}


#define CGI1_COMPARE_LL(str, offset, recv, expt, mask) {						\
		msg_printf(VRB, "CGI1_COMPARE : %s addr 0x%x rcv %llx exp %llx \n", str, offset, (recv & mask), (expt & mask) ); \
	if ((recv & mask) != (expt & mask)) { 							\
		msg_printf(ERR, "CGI1_COMPARE ERROR : %s addr %x rcv %llx exp %llx \n", str, offset, (recv & mask), (expt & mask) ); \
		G_errors++;	return (0);								\
	}											\
}

#define PTR_TO_REGISTER_OFFSET(list, offset) {			\
		while (list->value != offset) {			\
		msg_printf(SUM, "passed offset is %d register offset %d \n", offset, list->value ) ;	\
		list++;						\
	}							\
}

#define PTR_TO_REGISTER_NAME(list, name) {			\
		while (strcpy (list->name, name) != 0) list++ ;	\
}

#define DelayFor(cnt) { int i = 0; while (i++ < cnt)  ; }

typedef struct Info_on_Test_{
                char *TestStr;
                char *ErrStr;
		__uint32_t SeqNo;
} _CTest_Info;

typedef struct cgi1 {
        char            *name;                  /* Name of the register */
        __uint32_t      value;                  /* Value of the register */
        __uint32_t      size;                   /* size of the register in bytes */
        __uint64_t      mask;                   /* register mask */
        __uint32_t      mode;                   /* mode : RDONLY/WRONLY/RDWR  */
        __uint32_t      def;                    /* default values */
} cgi1_t;

#if defined(IP30)
typedef struct {
        int slot_id;
        unsigned long long base_addr;
} gio_slot_t;
#else
typedef struct {
        int slot_id;
        unsigned long long *base_addr;
} gio_slot_t;
#endif
/*
 * Extern Declarations
*/
extern cgi1_t cgi1_reg_list[] ;
extern _CTest_Info cgi1_info[]  ;
extern __uint32_t     G_errors ;
extern __uint32_t walk1s0s_32[WALK_32_SIZE] ;
extern __uint64_t walk1s0s_64[WALK_64_SIZE] ;
extern UWORD walk1s0s_16[WALK_16_SIZE] ;
extern unsigned char walk1s0s_8[WALK_8_SIZE] ;
extern __uint64_t  *cgi1base  ;
/* extern UWORD buf[]; */

extern __uint32_t GioSlot , board, cmd_seq ;
extern gio_slot_t gio_slot[3];
extern volatile __uint64_t *cosmobase;
extern cgi1_reg_t *cgi1ptr;

extern __uint64_t patrn64[PATRNS_64_SIZE];
extern __uint32_t patrn32[PATRNS_32_SIZE];
extern UWORD patrn16[PATRNS_16_SIZE];
extern UBYTE patrn8[PATRNS_8_SIZE];

extern void putCMD (UWORD *) ;
extern UBYTE  getCMD (UWORD *) ;

extern void mcu_write_cmd (UWORD, UWORD, __uint32_t, UWORD, UBYTE *) ;
extern void mcu_read_cmd (UBYTE, UWORD, __uint32_t, UWORD );

#define cgi1_fifos_read_mcu_fg_ae(base) cosmo2_asm_load_longlong(cgi1_addr_MCU_FIFO_FG_AE(base))
#define cgi1_fifos_read_mcu_fg_af(base) cosmo2_asm_load_longlong(cgi1_addr_MCU_FIFO_FG_AF(base))
#define cgi1_fifos_read_mcu_tg_ae(base) cosmo2_asm_load_longlong(cgi1_addr_MCU_FIFO_TG_AE(base))
#define cgi1_fifos_read_mcu_tg_af(base) cosmo2_asm_load_longlong(cgi1_addr_MCU_FIFO_TG_AF(base))

void 	cc1(void);
UBYTE 	CheckInterrupt(void);
void 	cosmo2_init (void);
void 	reset_chnl0(void);
void 	unreset_chnl0(void);
void 	reset_chnl1(void);
void 	unreset_chnl1(void);
void 	reset_chnl2(void);
void 	unreset_chnl2(void);
void 	reset_chnl3(void);
void 	unreset_chnl3(void);
int		cosmo2_upc_setup (__uint32_t  , UBYTE , UBYTE ,
                    UWORD , UWORD , UWORD );

extern UWORD mcu_READ_WORD(__uint32_t);
extern UBYTE mcu_READ(__uint32_t);
extern void mcu_WRITE(__uint32_t, UBYTE);
extern void mcu_WRITE_WORD (__uint32_t, UWORD);

#if defined(IP30)
extern _mg_xbow_portinfo_t mg_xbow_portinfo_t[6];
#endif

#endif
