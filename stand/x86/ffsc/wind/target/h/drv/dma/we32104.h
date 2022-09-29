/* we32104.h - Western Electric 32104 DMA controller header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01f,22sep92,rrr  added support for c++
01e,26may92,rrr  the tree shuffle
01d,26may92,ajm  got rid of HOST_DEC def's (new compiler)
                  updated copyright
01c,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01b,28sep91,ajm  ifdef'd HOST_DEC for compiler problem
01e,02Aug91,rfs  created
*/

#ifndef __INCwe32104h
#define __INCwe32104h

#ifdef __cplusplus
extern "C" {
#endif


typedef struct we32104_chan
    {
    volatile u_int SourceAdd;
    volatile u_int DestAdd;
    u_int Dummy1;
    volatile u_int BaseAdd;
    volatile u_int TranCount;
    u_int Dummy2;
    volatile u_int IntVect;
    u_int Dummy3;
    volatile u_int StatCont;
    u_int Mode;
    u_int DevControl;
    u_int Fill [21];
    } WE32104_CHAN;

typedef struct we32104
    {
    WE32104_CHAN Ch0;
    WE32104_CHAN Ch1;
    WE32104_CHAN Ch2;
    WE32104_CHAN Ch3;
    u_int Dummy1 [4];
    u_int Mask;
    u_int Dummy5 [123];
    volatile u_int DataBuf0 [8];
    u_int Dummy6 [24];
    volatile u_int DataBuf1 [8];
    u_int Dummy7 [24];
    volatile u_int DataBuf2 [8];
    u_int Dummy8 [24];
    volatile u_int DataBuf3 [8];
    } WE32104;


/***** mode register bit defs *****/

#define MODE_IE			0x0004			/* interrupt enable */

#define MODE_DS_MASK	0x00e0			/* system buss data size bits */
#define MODE_DS_CHAR	0x00e0			/* size is   8 bits */
#define MODE_DS_SHORT	0x00c0			/* size is  16 bits */
#define MODE_DS_LONG	0x0080			/* size is  32 bits */
#define MODE_DS_DOUBLE	0x00a0			/* size is  64 bits */
#define MODE_DS_QUAD	0x0000			/* size is 128 bits */

#define	MODE_DAC		0x0200			/* dest addr constant */
#define	MODE_SAC		0x0800			/* src addr constant */

#define	MODE_TT_MASK	0x6000			/* transfer type bits */
#define	MODE_TT_MM		0x0000			/* memory to memory */
#define	MODE_TT_PM		0x2000			/* peripheral to memory */
#define	MODE_TT_MP		0x4000			/* memory to peripheral */
#define MODE_TT_MF		0x6000			/* memory fill */

#define MODE_BR			0x8000			/* burst mode */


/***** device control register bit defs *****/

#define DCR_PA_MASK		0x007c			/* peripheral buss address bits.
										 * there are 32 possible addresses,
										 * too many to give indidual defs.
										 */

#define DCR_SF_MASK		0x0380			/* stretch field bits, in sync mode */
#define DCR_SF_0		0x0000			/* insert 0 wait state */
#define DCR_SF_1		0x0010			/* insert 1 wait state, etc. */
#define DCR_SF_2		0x0100
#define DCR_SF_4		0x0180
#define DCR_SF_8		0x0200
#define DCR_SF_16		0x0280
#define DCR_SF_32		0x0300
#define DCR_SF_64		0x0380

#define DCR_SA_MASK		0x0400			/* sync/async bit mask */
#define DCR_SYNC		0x0400			/* sync mode, use SF field */
#define DCR_ASYNC		0x0000			/* async mode */

#define DCR_BL			0x2000			/* burst length */

#define DCR_CS_MASK		0xc000			/* peripheral buss chip select bits */
#define DCR_CS_0		0x0000			/* chip select 0 */
#define DCR_CS_1		0x4000			/* chip select 1 */
#define DCR_CS_2		0x8000			/* chip select 2 */
#define DCR_CS_3		0xc000			/* chip select 3 */


/***** status and control register bit defs *****/

#define SCR_NT			0x0001			/* normal transfer complete */

#define SCR_ERR_MASK	0x0037			/* error bits mask */
#define SCR_ERR_NONE	0x0000
#define SCR_ERR_BERR	0x0002			/* buss error */
#define SCR_ERR_CS		0x0004			/* CS pin active when master */
#define SCR_ERR_SA		0x0008			/* software abort issued */
#define SCR_ERR_TIME	0x0020			/* peripheral buss time out */

#define SCR_ACT			0x0100			/* channel is active status */
#define SCR_SA			0x0200			/* software abort cmd */
#define SCR_HB			0x0400			/* channel halt cmd */
#define SCR_STR			0x0800			/* channel start cmd */
#define SCR_CH			0x1000			/* use chaining cmd */


/***** mask register bit defs *****/

#define MASK_CH0	0x1
#define MASK_CH1	0x2
#define MASK_CH2	0x4
#define MASK_CH3	0x8


#ifdef __cplusplus
}
#endif

#endif /* __INCwe32104h */
