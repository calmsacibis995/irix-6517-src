/*********************************************************************
 *  Title :  mpk_test                                                *
 *********************************************************************
 *                                                                   *
 *  Description:                                                     *
 *  	Standalone mpkiller tests for IP30/SpeedRacer.               *
 *                                                                   *
 *  Compile Options:                                                 *
 *                                                                   *
 *  Special Notes:                                                   *
 *      Use 64-bit address mode.                                     *
 *                                                                   *
 *  (C) Copyright 1993 by MIPS Technology Inc.                       *
 *  All Rights Reserved.                                             *
 *********************************************************************/

/*********************************************************************
 $Header: /proj/irix6.5.7m/isms/stand/arcs/ide/IP30/mpkiller/RCS/mpk_template.s,v 1.2 1996/11/15 01:47:59 kkn Exp $
 *********************************************************************/
#include	<asm.h>
#include	"t5.h"

/*
 * Possible options for PAGE_MASK_SIZE (T5)
 PageMask_4KMsk
 PageMask_16KMsk
 PageMask_64KMsk
 PageMask_256KMsk
 PageMask_1MMsk
 PageMask_4MMsk
 PageMask_16MMsk
 */

/* MPKILLER defines */

#define INIT_CPU(CPU)

#define ALLOC_CPU_LOCK  0x9fffff00
#define LOCAL_LOCK      0x9ffffd00
#define SLAVE_LOCK      0x9ffffc00

#define PASS_LOC        0xbfb00000
#define FAIL_LOC        0xbfb00080

#define FETCH_OP		0x00
#define FETCH_INC_OP		0x08
#define FETCH_DEC_OP		0x10
#define FETCH_CLR_OP		0x18
#define INIT_OP			0x00
#define INC_OP			0x08
#define DEC_OP			0x10
#define AND_OP			0x18
#define OR_OP			0x20

#define PAGE_MASK_SIZE          PageMask_4KMsk

#define JUMP1OP(reg1,tableIndex,tableLabel)                            \
	LOADADR r21,tableIndex; /* load offset table base */           \
	UMAP_KERN_TO_PHYSICAL( r21 );                                  \
	lw      r22,-4(r21);    /* load index into table  */           \
	dadd    r23,r22,r21;    /* get effective address  */           \
	lw      r23,0(r23);     /* load offset into next table */      \
	dsll    r23,r23,MULT_FACTOR;    /* mult by 4 or 8*/            \
	dla r24,tableLabel; /* base of other table */                  \
	UMAP_KERN_TO_PHYSICAL( r24 );                                  \
	dadd    r23,r23,r24;    /* effective address we want */        \
	ld      r23,0(r23);     /* load offset into next table */      \
	dadd    r22,r22,4;      /* prepare to update first index */    \
	or      reg1,r0,r23;    /* store in register ready for jump */ \
	sw      r22,-4(r21);    /* update first index */

#define VA_MASK 0x1ffff000

#define customMT_EntryLo0_32(c, d, v, g)   \
	li      k0, 0xffffe000;            \
	li      t2, 0xffffffff;            \
	mfc0    t3,C0_PageMask;            \
	xor     t3,t3,t2;                  \
	and     k0,k0,t3;                  \
	and     k1,k1,k0;                  \
	srl     k1, k1, EntryLo_PFNShf;    \
	ori     k1, k1, (c << EntryLo_CAShf) | (d << EntryLo_DShf) | (v << EntryLo_VShf) | g; \
	mtc0    k1, C0_EntryLo0;

#define customMT_EntryLo1_32(c, d, v, g)   \
	li      k0, 0xffffe000; \
	li      t2, 0xffffffff; \
	mfc0    t3,C0_PageMask; \
	xor     t3,t3,t2;       \
	and     k0,k0,t3;       \
	and     k1,k1,k0;       \
	mfc0    t2,C0_PageMask; \
	srl     t2,t2,13;       \
	beq     t2,r0,1f;       \
	ori     t3,r0,12;       \
	srl     t2,t2,2;        \
	beq     t2,r0,1f;       \
	ori     t3,r0,14;       \
	srl     t2,t2,2;        \
	beq     t2,r0,1f;       \
	ori     t3,r0,16;       \
	srl     t2,t2,2;        \
	beq     t2,r0,1f;       \
	ori     t3,r0,18;       \
	srl     t2,t2,2;        \
	beq     t2,r0,1f;       \
	ori     t3,r0,20;       \
	srl     t2,t2,2;        \
	beq     t2,r0,1f;       \
	ori     t3,r0,22;       \
	srl     t2,t2,2;        \
	beq     t2,r0,1f;       \
	ori     t3,r0,24;       \
1:	li      k0,0x1;        \
	sll     k0,k0,t3;       \
	addu    k1,k1,k0;    \
	srl     k1, k1, EntryLo_PFNShf; \
	ori     k1, k1, (c << EntryLo_CAShf) | (d << EntryLo_DShf) | (v << EntryLo_VShf) | g; \
	mtc0    k1, C0_EntryLo1;

#define customMT_EntryLo0_64(c, d, v, g)   \
	dli      k0, 0x0fffffffffffe000;        \
	li       t2, 0xffffffff;        \
	dmfc0    t3,C0_PageMask;        \
	xor      t3,t3,t2;      \
	and      k0,k0,t3;      \
	and      k1,k1,k0;      \
	dsrl     k1, k1, EntryLo_PFNShf; \
	ori      k1, k1, (c << EntryLo_CAShf) | (d << EntryLo_DShf) | (v << EntryLo_VShf) | g; \
	dmtc0    k1, C0_EntryLo0;

#define customMT_EntryLo1_64(c, d, v, g)   \
	dli      k0, 0x0fffffffffffe000;        \
	li       t2, 0xffffffff;        \
	dmfc0    t3,C0_PageMask;        \
	xor      t3,t3,t2;      \
	and      k0,k0,t3;      \
	and      k1,k1,k0;      \
	dmfc0    t2,C0_PageMask;        \
	srl      t2,t2,13;      \
	beq      t2,r0,1f;      \
	ori      t3,r0,12;      \
	srl      t2,t2,2;       \
	beq      t2,r0,1f;      \
	ori      t3,r0,14;      \
	srl      t2,t2,2;       \
	beq      t2,r0,1f;      \
	ori      t3,r0,16;      \
	srl      t2,t2,2;       \
	beq      t2,r0,1f;      \
	ori      t3,r0,18;      \
	srl      t2,t2,2;       \
	beq      t2,r0,1f;      \
	ori      t3,r0,20;      \
	srl      t2,t2,2;       \
	beq      t2,r0,1f;      \
	ori      t3,r0,22;      \
	srl      t2,t2,2;       \
	beq      t2,r0,1f;      \
	ori      t3,r0,24;      \
1:	li       k0,0x1;        \
	dsll     k0,k0,t3;      \
	daddu    k1,k1,k0;    \
	dsrl     k1, k1, EntryLo_PFNShf; \
	ori      k1, k1, (c << EntryLo_CAShf) | (d << EntryLo_DShf) | (v << EntryLo_VShf) | g; \
	dmtc0    k1, C0_EntryLo1;


/*****************************************************************************
 * MT_ENLO0_32:
 *    Create an Entry Lo from the label, c, d, v, and g given for a 32b address.
 *    KSeg0/1 are detected and if in those regions the upper 3 bits of the
 *    address are masked.
 *****************************************************************************/
#define MT_ENLO0_32LI(addr , c, d, v, g)                          \
	li      r2, addr;                                        \
	li      r1, 0x1ffff000;  /* get the PFN */                \
	and     r2, r2, r1;                                       \
	srl     r2, r2, EntryLo_PFNShf;                           \
	ori     r2, r2, (c << EntryLo_CAShf) | (d << EntryLo_DShf) | (v << EntryLo_VShf) | g; \
	mtc0    r2, C0_EntryLo0;

/******************************************************************************
 * MT_ENLO1_32:
 *    Create an Entry Lo from the label, c, d, v, and g given for a 32b address.
 *    KSeg0/1 are detected and if in those regions the upper 3 bits of the
 *    address are masked.
 *****************************************************************************/
#define MT_ENLO1_32LI(addr , c, d, v, g)                              \
	li      r2, addr;                                        \
	li      r1, 0x1ffff000;  /* get the PFN */                \
	and     r2, r2, r1;                                       \
	srl     r2, r2, EntryLo_PFNShf;                           \
	ori     r2, r2, (c << EntryLo_CAShf) | (d << EntryLo_DShf) | (v << EntryLo_VShf) | g; \
	mtc0    r2, C0_EntryLo1;

#define MT_EntryLo0_32(c, d, v, g)   \
	mfc0    k0,C0_BadVAddr;     \
	li      k1, 0x1fffe000;             \
	and     k0, k0, k1;           \
	srl     k0, k0, EntryLo_PFNShf; \
	ori     k0, k0, (c << EntryLo_CAShf) | (d << EntryLo_DShf) | (v << EntryLo_VShf) | g; \
	mtc0    k0, C0_EntryLo0;
#define MT_EntryLo1_32(c, d, v, g)   \
	mfc0    k0,C0_BadVAddr;     \
	li      k1, 0x1fffe000;             \
	and     k0, k0, k1;           \
	li      k1, 0x00001000;        \
	add     k0,k0,k1;    \
	srl     k0, k0, EntryLo_PFNShf; \
	ori     k0, k0, (c << EntryLo_CAShf) | (d << EntryLo_DShf) | (v << EntryLo_VShf) | g; \
	mtc0    k0, C0_EntryLo1;

#define DINCLOCK(address,count)                         \
	dsubu	sp,sp,48;				\
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	dli     r1,address;                             \
1:	lld     r2,0(r1);                               \
	daddiu  r2,r2,1;                                \
	scd     r2,0(r1);                               \
	beq     r2,r0,1b;                               \
	nop;                                            \
	ld      r1,0(sp);                               \
	ld      r2,8(sp);				\
	daddu	sp,sp,48

#define DINCLOCK_L(address,count)                       \
	dsubu	sp,sp,48;				\
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	dla     r1,address;                             \
1:	lld     r2,0(r1);                               \
	daddiu  r2,r2,1;                                \
	beq     r2,r0,1b;                               \
	nop;                                            \
	ld      r1,0(sp);                               \
	ld      r2,8(sp);				\
	daddu	sp,sp,48

#define FUNC_FETCH_INC_OP(address,count)                \
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	dli     r1,address;                             \
1:	ld      r2,FETCH_INC_OP(r1);                    \
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	daddu   sp,sp,48

#define FUNC_FETCH_INC_OP_L(address,count)              \
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	sd      r3,16(sp);                              \
	sd      r4,24(sp);                              \
	dla     r1,address;                             \
	dli	r3,0x000000002fffffff;			\
	dli	r4,0x9400000000000000;			\
	and	r1,r1,r3;				\
	or	r1,r1,r4;				\
1:	ld      r2,FETCH_INC_OP(r1);                    \
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	ld      r3,16(sp);                              \
	ld      r4,24(sp);                              \
	daddu   sp,sp,48


#define FUNC_FETCH_DEC_OP(address,count)                \
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	dli     r1,address;                             \
1:	ld      r2,FETCH_DEC_OP(r1);                    \
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	daddu   sp,sp,48

#define FUNC_FETCH_DEC_OP_L(address,count)              \
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	sd      r3,16(sp);                              \
	sd      r4,24(sp);                              \
	dla     r1,address;                             \
	dli	r3,0x000000002fffffff;			\
	dli	r4,0x9400000000000000;			\
	and	r1,r1,r3;				\
	or	r1,r1,r4;				\
1:	ld      r2,FETCH_DEC_OP(r1);                    \
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	ld      r3,16(sp);                              \
	ld      r4,24(sp);                              \
	daddu   sp,sp,48

#define FUNC_DEC_OP(address,count)			\
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	dli     r1,address;                             \
	dli     r2,count;                               \
1:	sd      r2,DEC_OP(r1);				\
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	daddu   sp,sp,48

#define FUNC_DEC_OP_L(address,count)			\
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	sd      r3,16(sp);                              \
	sd      r4,24(sp);                              \
	dla     r1,address;                             \
	dli	r3,0x000000002fffffff;			\
	dli	r4,0x9400000000000000;			\
	and	r1,r1,r3;				\
	or	r1,r1,r4;				\
1:	sd      r2,DEC_OP(r1);				\
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	ld      r3,16(sp);                              \
	ld      r4,24(sp);                              \
	daddu   sp,sp,48

#define FUNC_FETCH_CLR_OP(address,count)                \
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	dli     r1,address;                             \
1:	ld      r2,FETCH_CLR_OP(r1);                    \
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	daddu   sp,sp,48

#define FUNC_FETCH_CLR_OP_L(address,count)              \
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	sd      r3,16(sp);                              \
	sd      r4,24(sp);                              \
	dla     r1,address;                             \
	dli	r3,0x000000002fffffff;			\
	dli	r4,0x9400000000000000;			\
	and	r1,r1,r3;				\
	or	r1,r1,r4;				\
1:	ld      r2,FETCH_CLR_OP(r1);                    \
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	ld      r3,16(sp);                              \
	ld      r4,24(sp);                              \
	daddu   sp,sp,48


#define FUNC_INC_OP(address,count)			\
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	dli     r1,address;                             \
1:	sd      r2,INC_OP(r1);				\
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	daddu   sp,sp,48

#define FUNC_INC_OP_L(address,count)			\
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	sd      r3,16(sp);                              \
	sd      r4,24(sp);                              \
	dla     r1,address;                             \
	dli	r3,0x000000002fffffff;			\
	dli	r4,0x9400000000000000;			\
	and	r1,r1,r3;				\
	or	r1,r1,r4;				\
1:	sd      r2,INC_OP(r1);				\
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	ld      r3,16(sp);                              \
	ld      r4,24(sp);                              \
	daddu   sp,sp,48

#define FUNC_AND_OP(address,count)			\
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	dli     r1,address;                             \
	dli	r2,count;				\
1:	sd      r2,AND_OP(r1);				\
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	daddu   sp,sp,48

#define FUNC_AND_OP_L(address,count)			\
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	sd      r3,16(sp);                              \
	sd      r4,24(sp);                              \
	dla     r1,address;                             \
	dli	r3,0x000000002fffffff;			\
	dli	r4,0x9400000000000000;			\
	and	r1,r1,r3;				\
	or	r1,r1,r4;				\
	dla     r1,address;                             \
	dli	r2,count;				\
1:	sd      r2,AND_OP(r1);				\
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	ld      r3,16(sp);                              \
	ld      r4,24(sp);                              \
	daddu   sp,sp,48

#define FUNC_OR_OP(address,count)			\
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	dli     r1,address;                             \
	dli	r2,count;				\
1:	sd      r2,OR_OP(r1);				\
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	daddu   sp,sp,48

#define FUNC_OR_OP_L(address,count)			\
	dsubu   sp,sp,48;                               \
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	sd      r3,16(sp);                              \
	sd      r4,24(sp);                              \
	dla     r1,address;                             \
	dli	r3,0x000000002fffffff;			\
	dli	r4,0x9400000000000000;			\
	and	r1,r1,r3;				\
	or	r1,r1,r4;				\
	dla     r1,address;                             \
	dli	r2,count;				\
1:	sd      r2,OR_OP(r1);				\
	ld      r1,0(sp);                               \
	ld      r2,8(sp);                               \
	ld      r3,16(sp);                              \
	ld      r4,24(sp);                              \
	daddu   sp,sp,48


#define FUNC_INIT_ADDRESS_OP(address,data)		\
	li	r1,1;					\
	bne	r1,r7,1f;				\
	nop;						\
	dli     r1,address;                             \
	dli     r3,0x00ffffffffffffff;                  \
	dli     r2,0x9600000000000000;                  \
	and     r3,r3,r1;				\
	or      r3,r2,r3;				\
	daddiu	r2,r0,-1;				\
	sd      r2,0(r3);				\
	dli	r2,data;				\
	sd      r2,0(r1);				\
1:	nop

#define FUNC_INIT_ADDRESS_OP_L(address,data)		\
	li	r1,1;					\
	bne	r1,r7,1f;				\
	nop;						\
	dla     r1,address;                             \
	daddiu	r2,r0,-1;				\
	sd      r2,0(r1);				\
	dli	r2,data;				\
	sd      r2,8(r1);				\
1:	nop

#define INIT_ADDRESS_L(address,data)			\
	li      r1,1;                                   \
	bne     r1,r7,1f;                               \
	nop;                                            \
	dla     r1,address;                             \
	dli     r2,data;                                \
	sd      r2,0(r1);                               \
1:	nop

#define INIT_ADDRESS(address,data)                     \
	li      r1,1;                                   \
	bne     r1,r7,1f;                               \
	nop;                                            \
	dli     r1,address;                             \
	dli     r2,data;                                \
	sd      r2,0(r1);                               \
1:	nop


#define CHECK_OROP_L(address,data)			\
	dla     r1,address;                             \
	dli	r2,data;				\
	ld      r2,0(r2);				\
	bne	r2,r1,fail;				\
	nop


#define BARRIER(baseBarrierAddr,offset,refAddr)         \
	dsubu	sp,sp,64;				\
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	sd      r3,16(sp);                              \
	sd      r4,24(sp);                              \
	dla    r1,baseBarrierAddr;                      \
	daddiu	r1,r1,offset;				\
1:	ll     r2,0(r1);                                \
	addiu  r2,r2,1;                                 \
	sc     r2,0(r1);                                \
	beq     r2,r0,1b;                               \
	nop;						\
	dla	r3,refAddr;				\
	lw	r4,0(r3);				\
1:	lw	r2,(r1);				\
	bne	r2,r4,1b;				\
	nop;						\
	ld      r1,0(sp);                               \
	ld      r2,8(sp);				\
	ld      r3,16(sp);				\
	ld      r4,24(sp);				\
	daddu	sp,sp,64

#define FUNC_BARRIER_FETCH_OP(baseBarrierAddr,refAddr)	\
	dsubu	sp,sp,64;				\
	sd      r1,0(sp);                               \
	sd      r2,8(sp);                               \
	sd      r3,16(sp);                              \
	sd      r4,24(sp);                              \
	dla    r1,baseBarrierAddr;                      \
	ld	r2,FETCH_INC_OP(r1);			\
	daddiu	r2,r2,1;				\
	dla	r3,refAddr;				\
	ld	r4,0(r3);				\
	ld	r2,(r1);				\
	bne	r2,r4,1f;				\
	nop;						\
	ld	r2,FETCH_CLR_OP(R1);			\
	bne	r2,r4,fail;				\
	nop;						\
	beq	r0,r0,2f;				\
	nop;						\
1:	ld	r2,FETCH_OP(r1);			\
	bne	r2,r0,1b;				\
2:	nop;						\
	ld      r1,0(sp);                               \
	ld      r2,8(sp);				\
	ld      r3,16(sp);				\
	ld      r4,24(sp);				\
	daddu	sp,sp,64

/* converion from R4000 -> T5 */

#define		C0_COMPARE	C0_Compare
#define		C0_COUNT	C0_Count
#define		PM_4K		PageMask_4KMsk
#define		PM_256K		PageMask_256KMsk
#define		MT_ENHI_32	MT_EntryHi_32
#define		MT_PMASK	MT_PageMask
#define		MT_INDEX	MT_Index
#define		OFFSET256K	Offset256K
#define         VA_TO_PA_MASK   0x1fffffff
#define		SIGN_EXTEND_REG r30
#define		Hit_Writeback_Inv_SD Hit_Writeback_Inv_S
#define		Index_Writeback_Inv_SD Index_Writeback_Inv_S
#define JR_MASK                 0xfc00003f
#define JR_INST                 0x8
#define JALR_INST               0x9
#define ARCH_NOPS()

#define		MIN_PAGE_USE		 10
#define		NEG_MIN_PAGE_USE	-10
#define		NEG_MAX_PAGE_USE	-30


/*********************************************************************
 *      Code Segment.                                                *
 *********************************************************************/

#define CACHE_ALG	CA_CachCohS	/* CA_CachCohE or CA_CachCohS */

	#
	# mpk_test(return_status *)
	#
	.extern	MPK_Krnl_Data_Table
	.globl	Exit_MPK

	.text
	.set	noreorder
  	.set	noat

LEAF(mpk_test)
	# signal master slave is alive and 
	# to install exception handlers
	ld	v0, (a0)
	dli	v1, 0xdeadbeef1
	dli	t0, 0xbeefbeef0
	bne	v0, v1, 1f
	nop
	dli	t1, 0xbeef00001
	# inform master slave is alive
	sd	t1, (a0)
	# wait for master signal to continue
2:	ld	t0, (a0)
	beq	t0, t1, 2b
	nop
1:
	sd	t0, (a0)

	/* for hang cases, kick start the LA state machine */
	dla	t0, LA_trigger
	dsll	t0, 5
	dsrl	t0, 5
	dli	t1, 0x9000000000000000
	or	t0, t1
	ld	r0, (t0)	# reads 0xbeefcafefeedbeef

	dsubu	sp,sp,96
	sd	s0,0(sp)
	sd	s1,8(sp)
	sd	s2,16(sp)
	sd	s3,24(sp)
	sd	s4,32(sp)
	sd	s5,40(sp)
	sd	s6,48(sp)
	sd	s7,56(sp)
	sd	gp,64(sp)
	sd	s8,72(sp)
	sd	ra,80(sp)
	sd	a0,88(sp)	# return status block pointer
  # save sp in r25
	move	r25,sp

/*************************************************************
 *    Set the coherency attribute to Coherent Exclusive
 ************************************************************/

	mfc0    r10, C0_Config
	li      r11, ~Config_K0Msk
	and     r10, r10, r11
	or      r10, r10, CACHE_ALG
	mtc0    r10, C0_Config


	mtc0	r0, C0_Index
	dla     r8, MPK_Krnl_Data_Table
	dla     r9, localHandler
	sd      r9, TLB_HndlrV_Offs(r8)         # setup local handler
	sd      r9, XTLB_HndlrV_Offs(r8)        # setup local handler
	sd      r9, CacheError_HndlrV_Offs(r8)  # setup local handler
	sd      r9, Others_HndlrV_Offs(r8)      # setup local handler

	li      r9, 0xffff                      # do EPC + 4
	sw      r9, OK_Exceptions_SKIP_Offs(r8)
	li      r9, 0xffff
	sw      r9, OK_Exceptions_NOSK_Offs(r8)

	mfc0	r3, C0_SR
	li	r22, ~SR_BEV
	and	r3, r22
	# use 64 bit addressing
	li      r22, SR_CU1|SR_FR|SR_IE|SR_IM2|SR_IM3|SR_IM4|SR_IM5|SR_IM6|SR_KX|SR_SX|SR_UX
	or	r22, r22,r3
	mtc0    r22, C0_SR
	li      r3, FP_RN             # clear enables,stickies,
	ctc1    r3, C1_SR             # condition, round nearest
	nop
	nop

	# make sure we switch addressing modes by doing an eret
	# forward
	dla	r1, 1f
	dmtc0	r1, C0_EPC
	eret
1:
	or	r1,r0,r0
	or	r2,r0,r0
	or	r3,r0,r0
	or	r4,r0,r0
	or	r5,r0,r0
	or	r6,r0,r0
	or	r7,r0,r0
	or	r8,r0,r0
	or	r9,r0,r0
	or	r10,r0,r0
	or	r11,r0,r0
	or	r12,r0,r0
	or	r13,r0,r0
	or	r14,r0,r0
	or	r15,r0,r0
	or	r16,r0,r0
	or	r17,r0,r0
	or	r18,r0,r0
	or	r19,r0,r0
	or	r20,r0,r0
	or	r21,r0,r0
	or	r22,r0,r0
	or	r23,r0,r0
	or	r24,r0,r0
  # save sp in r25
  #	or	r25,sp,r0
	or	r26,r0,r0
	or	r27,r0,r0
	or	r28,r0,r0
	or	sp,r0,r0
	or	r30,r0,r0
  # don't clobber return address
  #	or	r31,r0,r0

    /*
	Increase the number of outstanding I/O transactions
	Init_Heart_MP
     */

CODE_INSERT_POINT

	.align 6
Exit_MPK:
	# do a trigger point for LA
	# if a cpu has mis-compare the upper 32 bits has the address
	# of failing block, otherwise it will be 0x0.
	dla	t1, LA_trigger
	dsll	t1, 5
	dsrl	t1, 5
	dli	t2, 0x9000000000000000
	or	t1, t2
	dsll	t2, v0, 32
	dli	t0, 0x00000000deadbeef
	or	t2, t0
	dsll	t3, r29, 32
	or	t3, t0
	sd	t2, 0x00(t1)
	sd	t3, 0x08(t1)
	sd	r30, 0x10(t1)

	# check if we get here due to watchdogTimeout
	li	t1, HUNG
	bne	t1, v1, 1f
	nop
	li	r30, 0
	li	r29, 0
	dli	v0, 0xdeadbeefdeadbeef
1:
	ld	t0, 88(r25)		# return status pointer
	# always return the last loop count
	dla	t1, iterationCount	# remaining iterations
	lw	t1, (t1)
	dla	t2, hwLoopCount		# total iterations to run
	lw	t2, (t2)
	subu	t1, t2, t1

	# store status in return-status block if failed
	beq	v0, zero, 1f
	sd	t1, 0x08(t0)		# iteration count
	sd	v0, 0x10(t0)		# failing point if mis-compare
	sd	r30,0x18(t0)		# expected xor actual
	sd	r29,0x20(t0)		# reg# of actual
1:
	dli	t1, 0xbeefbeef0
	ld	t2, (t0)
	beq	t1, t2, 1f
	nop

	# wait for master to restore exception handlers first
	dli	v1, 0xcafebeef1
2:	ld	t2, (t0)
	bne	v1, t2, 2b
	nop
	dli	t1, 0xbeefbeef1
	sd	t1, (t0)
1:
	move	sp,r25
	ld	s0,0(sp)
	ld	s1,8(sp)
	ld	s2,16(sp)
	ld	s3,24(sp)
	ld	s4,32(sp)
	ld	s5,40(sp)
	ld	s6,48(sp)
	ld	s7,56(sp)
	ld	gp,64(sp)
	ld	s8,72(sp)
	ld	ra,80(sp)
	daddu	sp,sp,96

	j	ra
	nop

	#
	# Exception handler
	#
localHandler:
	dsubu	sp,sp,64
	sd	r1,0(sp)
	sd	r2,8(sp)
	sd	t0,16(sp)
	sd	t1,24(sp)
	sd	t2,32(sp)
	sd	t3,40(sp)
	sd	k0,48(sp)
	sd	k1,56(sp)

	mfc0    k0, C0_Cause               # Check Cause.
	li	k1, 0x7c
	and     k0, k0, k1
	li      k1,0x8
	beq     k1,k0,handleTLB
	nop
	li      k1,0xc
	beq     k1,k0,handleTLB
	nop

	beq	r0,k0,interrupt		 # jump to our interrupt handler
	nop

	mfc0	k0,C0_Cause		# load from cause register
	dli	k1,Cause_BD		# check to see if bds set
	and	k0,k1,k0
	beq	k0,r0,notBDSorTLB	# if BDS not set continue
	nop

	dmfc0	k0,C0_EPC		# get current epc which is really

 # we may need ifdefs for different
 # processors here, since all may not subtract 4

	lw	k0,0(k0)		# get the instruction
	dla	k1,trueBranch		#
	sw	k0,0(k1)		# save it for future

	li	k1,JR_MASK
	and	k1,k0,k1		# mask off bits for jalr and jr detection
	xori	k1,k1,JR_INST		# check for JR
	beq	r0,k1,processJumps	# if 0 means same
	nop
	li	k1,JR_MASK		# always executed
	and	k1,k0,k1		# mask off bits for jalr and jr detection
	xori	k1,k1,JALR_INST		# check for JR
	beq	r0,k1,processJumps	# if 0 means same
	nop				# otherwise keep going
	srl	k0,k0,16		# get upper 16 bits (opcode + registers fields)
	sll	k0,k0,16		#

	addiu	k0,k0,5			# create new branch instruction with new offset
	dla	k1,branchEmulate
	sw	k0,0(k1)		# write new branch instruction

					# write back to mainmemory
	cache   Hit_Writeback_Inv_S,0(k1)

	dla	k1,trueBranch		# get instruction back again
	lw	k0,0(k1)		#
	dsll	k0,k0,48		#
	dsra	k0,k0,48-2		# get lower 16 bits, mult by 4, sign extend

	dmfc0	k1,C0_EPC
	dmtc0	k1,C0_EPC		# force serialization on T5 (cacheop done by now)
	daddu	k0,k0,k1		# effective target address in k0
	daddiu  k0,k0,4                 # add 4 since address is relative to BDS not branch

	# k1  points to instruction afer BDS
	daddiu	k1,k1,8

 # at this point we have written the new instruction out,
 # k1 is addr(branch)+8, k0 is the effective branch

	beq	r0,r0,branchEmulate
	nop

interrupt:
	/* clear interrupts */
.int_0:
.data_0_0:
.mask_0_1f:
.target_0_0:
.dly_0_0:
.done_0:

	li	k0,0xf0			# wait for all interrupts to be cleared
1:
	mfc0	k1,C0_Cause
	and	k1,k0,k1
	bnez	k1,1b
	nop

	ld	r1,0(sp)
	ld	r2,8(sp)
	ld	t0,16(sp)
	ld	t1,24(sp)
	ld	t2,32(sp)
	ld	t3,40(sp)
	ld	k0,48(sp)
	ld	k1,56(sp)
	daddu	sp,sp,64
	eret
	nop

handleTLB:

#define	MASK_64_ADDR_DECODE	0xf000000000000000
#define	MASK_32_ADDR_DECODE	0xfffffffff0000000
#define MASK_ADDR_XKSSEG	0x4000000000000000
#define MASK_ADDR_XKSEG		0xc000000000000000

#define MASK_ADDR_KSSEG_1	0xffffffffc0000000
#define MASK_ADDR_KSSEG_2	0xffffffffd0000000
#define MASK_ADDR_KSSEG3_1	0xffffffffe0000000
#define MASK_ADDR_KSSEG3_2	0xfffffffff0000000

	dmfc0   k0,C0_BadVAddr			# get VA of TLB miss
	or	t0,k0,r0			# save in t0 for future use

	mfc0	k1,C0_SR			# see if we are in 64
	dli	t1,SR_KX			# or 32 bit mode
	and	k1,k1,t1			#
	beq	k1,r0,modeAddr32		# if 0, then 32 bit mode
	nop

	# ok so we are in 64 bit mode

	dli	k1,MASK_64_ADDR_DECODE		# check out the upper byte
	and	k1,k1,k0
	beq	k1,r0,xkusegAddr		# if 0 then it is xksuseq
	nop

	dli	k0,MASK_ADDR_XKSSEG		# check to see if xksseg
	beq	k1,k0,xkssegAddr
	nop

	dli	k0,MASK_ADDR_XKSEG		# check to see if xkseg
	beq	k1,k0,xksegAddr
	nop

	# fall through for compatibility areas of 32 bit

	# here we do VA->PA mapping K1 has PA at the end
modeAddr32:

	or	k0,t0,r0			# get badVA back
	dli	k1,0x0000000080000000		# distinguish between 7xxxxxxx and
	and	k1,k0,k1			# and D/Exxxxxxx
	bne	k1,r0,handleKseg32
	nop

	dla	t1,kuseg32Xlat
	ld	k1,0(t1)
	and	k0,k1,k0
	ld	k1,8(t1)
	or	k1,k1,k0
	beq	r0,r0,1f
	nop

handleKseg32:

	dla	t1,kseg32Xlat
	ld	k1,0(t1)
	and	k0,k1,k0
	ld	k1,8(t1)
	or	k1,k1,k0
	beq	r0,r0,1f
	nop

xkusegAddr:

	or	k0,t0,r0			# get badVA back
	dli	t2,0xffffffffc0000000		# mask for cksseg and ckseg3
	and	t3,t2,k0			# check value
	beq	t3,t2,1f			# ok address is 0xffffffffcxxxxxx
	nop
	dla	t1,xkusegXlat			# dont masks anything
	beq	r0,r0,2f
	nop
1:
	dla	t1,kuseg32Xlat
2:
	ld	k1,0(t1)
	and	k0,k1,k0
	ld	k1,8(t1)
	or	k1,k1,k0
	beq	r0,r0,1f
	nop

xkssegAddr:

	or	k0,t0,r0			# get badVA back
	dla	t1,xkssegXlat
	ld	k1,0(t1)
	and	k0,k1,k0
	ld	k1,8(t1)
	or	k1,k1,k0
	beq	r0,r0,1f
	nop

xksegAddr:

	or	k0,t0,r0			# get badVA back
	dla	t1,xksegXlat
	ld	k1,0(t1)
	and	k0,k1,k0
	ld	k1,8(t1)
	or	k1,k1,k0
	beq	r0,r0,1f
	nop

1:
	mfc0	t1,C0_SR			# double check to see if we are 64/32
	li	k0,SR_KX			#
	and	t1,t1,k0			#
	beq	t1,r0,tlb32Refill		# if 0, then 32 bit mode
	nop

tlb64Refill:
	li      t3,PAGE_MASK_SIZE
	mtc0    t3,C0_PageMask
	or	k0,t0,r0
						# at this point k0 has the VA
						# and K1 has PA  to be used
	dsra	k0,k0,13			# divide by 2, move over by 12
	dsll	k0,k0,EntryHi_VPN2Shf		# then place correctly in Entry Hi
	dmtc0   k0,C0_EntryHi
	mfc0    k0,C0_Index
	addi    k0,k0,1
	andi	k0,k0,0x3f			# force index wrap
	mtc0    k0,C0_Index
	or	t0,k0,r0
	or	t1,k1,r0
	customMT_EntryLo0_64(CACHE_ALG, 1, 1, 1 )
	or	k0,t0,r0
	or	k1,t1,r0
	customMT_EntryLo1_64(CACHE_ALG, 1, 1, 1 )
	tlbwi
	mfc0    k0,C0_SR
	mfc0    k0,C0_EPC

	ld	r1,0(sp)
	ld	r2,8(sp)
	ld	t0,16(sp)
	ld	t1,24(sp)
	ld	t2,32(sp)
	ld	t3,40(sp)
	ld	k0,48(sp)
	ld	k1,56(sp)
	daddu	sp,sp,64

	ARCH_NOPS()
	eret


tlb32Refill:

	li      t3,PAGE_MASK_SIZE
	mtc0    t3,C0_PageMask

	# ok we assume we left k0 with the VA we wanted

	# at this point k0 has the VA and K1 has PA  to be used
	or	k0,t0,r0
	sra	k0,k0,13			# divide by 2, move over by 12
	sll	k0,k0,EntryHi_VPN2Shf		# then place correctly in Entry Hi
	mtc0    k0,C0_EntryHi
	mfc0    k0,C0_Index
	addi    k0,k0,1
	andi	k0,k0,0x3f
	mtc0    k0,C0_Index
	or	t0,k0,r0
	or	t1,k1,r0
	customMT_EntryLo0_32(CACHE_ALG, 1, 1, 1)
	or	k0,t0,r0
	or	k1,t1,r0
	customMT_EntryLo1_32(CACHE_ALG, 1, 1, 1)
	tlbwi
	mfc0    k0,C0_SR
	mfc0    k0,C0_EPC

	ld	r1,0(sp)
	ld	r2,8(sp)
	ld	t0,16(sp)
	ld	t1,24(sp)
	ld	t2,32(sp)
	ld	t3,40(sp)
	ld	k0,48(sp)
	ld	k1,56(sp)
	daddu	sp,sp,64

	ARCH_NOPS()
	eret


processJumps:

	dla	k1,branchEmulate		# get instruction back again
	sw	k0,0(k1)			# write new branch instruction
	cache   Hit_Writeback_Inv_S,0(k1)
	dmtc0	k1,C0_EPC			# here we eret to emulated jump
	ARCH_NOPS()
	beq	r0,r0,branchEmulate
	nop

notBDSorTLB:

	dmfc0	k0,C0_EPC
	daddiu	k0,k0,4
	dmtc0	k0,C0_EPC

	ld	r1,0(sp)
	ld	r2,8(sp)
	ld	t0,16(sp)
	ld	t1,24(sp)
	ld	t2,32(sp)
	ld	t3,40(sp)
	ld	k0,48(sp)
	ld	k1,56(sp)
	daddu	sp,sp,64
	ARCH_NOPS()
	eret
	nop


	.text
	.align	8

branchEmulate:

	# true opcode + reg |concat with| new branch offsets to jr k0, jr k1
	.word	0x0
	 nop				# replaced BDS which causes no exception
	 dmtc0	k1,C0_EPC		# branch fail case
	 nop
	 eret
	 nop
	 dmtc0	k0,C0_EPC		# branch pass case
	 nop
	 eret
	 nop

 # store temp data here, could use a stack too

	.data

trueBranch:
	.word	0x0		# tmp store for instruction

	        .align 4

xkusegXlat:
	        .dword  0xffffffffffffffff
	        .dword  0x0
xkssegXlat:
	        .dword  0x0fffffffffffffff
	        .dword  0x0
xksegXlat:
	        .dword  0x0fffffffffffffff
	        .dword  0x0
kseg32Xlat:
	        .dword  0x000000001fffffff
	        .dword  0x0
kuseg32Xlat:
	        .dword  0x00000000ffffffff
	        .dword  0x0


	        .align 3

fpAddData:
	        .float  0x1.03fff8H0
	        .float  0x1.000008H0

fpMpyData:
	        .float  0x1.123456H0x78
	        .float  0x1.9abcdeH0xf0

fpDivSData:

	        .float 0x1.123456H0x78
	        .float 0x1.9abcdeH0xf0

	        .align 4
fpDivDData:

	        .double 0x1.123456789abcdH0xef
	        .double 0x1.0123456789abcH0xde

	        .align 4

saveReg:
	.dword	0x0
	.dword	0x0
	.dword	0x0
	.dword	0x0
	.dword	0x0
	.dword	0x0
	.dword	0x0
	.dword	0x0
	.dword	0x0
	.dword	0x0
	.dword	0x0
	.dword	0x0
	.dword	0x0


	.data
	.align 6
InitMaster:
	.dword   0

	.align 6
AllocCPULock:
	.dword   0

	.align 6
NumCPU:
	.dword   0

	.align 6
Beef:	.dword 0xcafebeefcafebeef
	.dword 0xbeefcafebeefcafe

	.align 6
	# processors write their ID here uncached
cpuIDWriteArea:
	.repeat 32
	.dword 0x0
	.endr

	.align 6
sharedLegoAddr:

	.repeat 8096
	.dword  0x0
	.endr
	.word   0


	.align 6
barrierLock:
	.repeat 8096
	.dword  0x0
	.endr
	.word   0

	.align 6
legoBarrierLock:
	.repeat 8096
	.dword  0x0
	.endr
	.word   0

	# mult be none-zero
SlaveLock:
	.word   0xa5a5a5a5

	.align 6
LA_trigger:	# sync point: write 0xdeadbeef to the lower 32 bits,
	.dword	0xbeefcafefeedbeef
	.dword	0xbeefcafefeedbeef
	.dword	0xbeefcafefeedbeef

	.align 12
     # OK a bunch of stack area, stack grows in a negative address space way
	.repeat 8096
	.dword  0x0
	.endr
stack:
	.repeat 64
	.dword  0x0
	.endr

	.globl branchEmulate
	.globl localHandler
	.globl handleTLB
      	.globl numberOfCPU
        .globl  hwLoopCount
        .globl iterationCount

        .globl InitMaster
        .globl AllocCPULock
        .globl NumCPU
        .globl SlaveLock
        .globl Beef
        .globl stack

        .globl fail
        .globl EndOfPass
	.globl LA_trigger
END(mpk_test)
