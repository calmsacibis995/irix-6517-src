/*********************************************************************
 *                                                                   *
 *  Title :  rad_utils.h					     *
 *  Author:  Jimmy Acosta                                            *
 *  Date  :  04.01.96                                                *
 *                                                                   *
 *********************************************************************
 *                                                                   *
 *  Description:                                                     *
 *  	Header file that defines RAD memory address for different    * 
 *      hardware platforms.  Macros for PIO access into RAD.         *
 *                                                                   *
 *  (C) Copyright 1996 by Silicon Graphics, Inc.                     *	
 *  All Rights Reserved.                                             *
 *********************************************************************/

#define Map_to_K1(x)   ((x) | 0xa0000000)

#define	RAD_CFG_WRITE(addr,val)	{*(((uint_t*)Map_to_K1(PCI_CFG_BASE|(int)addr))) = (val);}
#define	RAD_WRITE(addr,val)	{*(((uint_t*)Map_to_K1(PCI_MEM_BASE|(int)addr))) = (val);}
#define	RAD_CFG_READ(addr)	*(((uint_t*)Map_to_K1(PCI_CFG_BASE|(int)addr)))
#define	RAD_READ(addr)		*(((uint_t*)Map_to_K1(PCI_MEM_BASE|(int)addr)))

#define RAD_TOTAL_DMA  9
#define RAD_NUMBER_DESCRIPTORS 15
#define RAD_NUMBER_WORK_REGISTERS 9*3
#define CACHE_LINE_WORD_SIZE 32 

#define RAD_RESET_VALUE 0x1FFFE
#define RAD_MISC_VALUE 0x580

#define SETBITS(var, msk, val, shift)		\
    var = (var & (~(msk << shift))) | (val << shift)
#define Phys_to_PCI(x)  ((x) | 0x80000000)      /* physical to PCI addr  */

typedef enum _dmaType {RAD_ADAT_RX,
		       RAD_AES_RX,
		       RAD_ATOD,
		       RAD_ADAT_SUB_RX,
		       RAD_AES_SUB_RX,
		       RAD_ADAT_TX,
		       RAD_AES_TX,
		       RAD_DTOA,
		       RAD_STATUS} dmaType;

typedef enum _RadReference { RAD_OSC0,
			     RAD_OSC1,
			     RAD_MPLL0,
			     RAD_MPLL1,
			     RAD_AUX0,
			     RAD_AUX1 } RadReference;
    

/* Function Prototypes */

int RadSetup(void); 
int RadSetupDMA(int , int* , int , int ); 
int RadSetupCG(uint_t, uint_t, RadReference, uint_t* );
int RadSetClockIn(dmaType, uint_t);
void RadStartStatusDMA(uint_t, uint_t);
int RadStartDMA(dmaType, uint_t*);
int RadStopDMA(dmaType);
int RadUnresetInterfaces(dmaType, uint_t*);
int RadSetStatusMask(dmaType, uint_t*);

/* Sine wave generation functions */
int RadOscillatorInit(uint_t);
int RadOscillator(int *, uint_t, uint_t, uint_t, uint_t); 

/* Extern declarations */

extern void flushbus(void);
extern void invalidate_caches(void);
extern void* get_chunk(size_t );

/* Timer functions */
unsigned long RadGetRelativeTime(void); 
void RadWait(uint_t); 

