/*
#include "sys/mgrashw.h"
#include "sys/mgras.h"
*/


/*these are the CC1 and AB1 DCB addresses for the early version
  of Indigo2 Video board*/


#define DCB_ADDR_SHIFT   	7
#define DCB_VCC1_PXHACK         (0xC << DCB_ADDR_SHIFT) 
#define DCB_VAB1_PXHACK         (0xA << DCB_ADDR_SHIFT)
struct ev1_info {
	unsigned char BoardRev;
	unsigned char CC1Rev;
	unsigned char AB1Rev;
	unsigned char CC1dRamSize;
	unsigned char AB1RRev;
        unsigned char AB1GRev;
        unsigned char AB1BRev;
        unsigned char AB1ARev;
};

/* 
#define MGV1_CHANNEL_ID(channel, RGBChannel) \
{\
	switch(channel) { \
		case RED: \
			sprintf(RGBChannel, "RED CHANNEL"); \
			break;\
		case GREEN:\
			sprintf(RGBChannel, "GREEN CHANNEL"); \
			break;\
		case BLUE:\
			sprintf(RGBChannel, "BLUE CHANNEL"); \
			break;\
		case ALPHA: \
			sprintf(RGBChannel, "ALPHA CHANNEL"); \
			break;\
	}\
}
*/


/* Macros for reading and writing EV registers on the AB1 and CC1
 *
 */


/* AB1 Direct register numbers */

#define	V_AB_dRam			0
#define	V_AB_intrnl_reg		1
#define	V_AB_tst_reg		2
#define	V_AB_low_addr		3
#define	V_AB_high_addr		4
#define	V_AB_sysctl		5
#define	V_AB_rgb_sel		6

/* CC1 Direct register numbers */

#define V_CC_sysctl		0
#define V_CC_indrct_addreg	1
#define V_CC_indrct_datareg	2
#define V_CC_fbctl		3
#define V_CC_fbsel		4
#define V_CC_fbdata		5
#define V_CC_I2C_bus		6
#define V_CC_I2C_data		7
#define DCB_NOACK		0x0
#define Traditional_sync
#define HQ3

#ifdef HQ3
#define AB_R1(base, reg)	gv_r1 (base, DCB_VAB1_PXHACK, 2, \
					V_AB_##reg)
#define AB_R4(base, reg)	gv_r4 (base, DCB_VAB1_PXHACK, 2, \
					V_AB_##reg)
#define AB_W1(base, reg, val)	gv_w1 (base, DCB_VAB1_PXHACK, 2, \
					V_AB_##reg, val)
#define AB_W4(base, reg, val)	gv_w4 (base, DCB_VAB1_PXHACK, 2, \
					V_AB_##reg, val)


#define CC_R1(base, reg)	gv_r1 (base, DCB_VCC1_PXHACK, DCB_NOACK, \
					V_CC_##reg)
#define CC_R4(base, reg)	gv_r4 (base, DCB_VCC1_PXHACK, DCB_NOACK, \
					V_CC_##reg)
#define CC_W1(base, reg, val)	gv_w1 (base, DCB_VCC1_PXHACK, DCB_NOACK, \
					V_CC_##reg, val)
#define CC_W4(base, reg, val)	gv_w4 (base, DCB_VCC1_PXHACK, DCB_NOACK, \
					V_CC_##reg, val)

#elif PX_REX3

#ifdef Traditional_sync



#define AB_R1(base, reg)	(GVType == GVType_gr2 ? \
					base->ab1.reg : \
					gv_r1 (base, DCB_VAB1_PXHACK, DCB_ENASYNCACK, \
					V_AB_##reg))
#define AB_R4(base, reg)	(GVType == GVType_gr2 ? \
					base->ab1.reg : \
					gv_r4 (base, DCB_VAB1_PXHACK, DCB_ENASYNCACK, \
					V_AB_##reg))
#define AB_W1(base, reg, val)	(GVType == GVType_gr2 ? \
					(base->ab1.reg = (val)) : \
					gv_w1 (base, DCB_VAB1_PXHACK, DCB_ENASYNCACK, \
					V_AB_##reg, val))
#define AB_W4(base, reg, val)	(GVType == GVType_gr2 ? \
					(base->ab1.reg = (val)) : \
					gv_w4 (base, DCB_VAB1_PXHACK, DCB_ENASYNCACK, \
					V_AB_##reg, val))

#else

Something is desparately wrong.

#define AB_R1(base, reg)	(GVType == GVType_gr2 ? \
					base->ab1.reg : \
					gv_r1 (base, DCB_VAB1_PXHACK, DCB_NOACK, \
					V_AB_##reg))
#define AB_R4(base, reg)	(GVType == GVType_gr2 ? \
					base->ab1.reg : \
					gv_r4 (base, DCB_VAB1_PXHACK, DCB_NOACK, \
					V_AB_##reg))
#define AB_W1(base, reg, val)	(GVType == GVType_gr2 ? \
					(base->ab1.reg = (val)) : \
					gv_w1 (base, DCB_VAB1_PXHACK, DCB_NOACK, \
					V_AB_##reg, val))
#define AB_W4(base, reg, val)	(GVType == GVType_gr2 ? \
					(base->ab1.reg = (val)) : \
					gv_w4 (base, DCB_VAB1_PXHACK, DCB_NOACK, \
					V_AB_##reg, val))


#endif

#define CC_R1(base, reg)	(GVType == GVType_gr2 ? \
					base->cc1.reg : \
					gv_r1 (base, DCB_VCC1_PXHACK, DCB_NOACK, \
					V_CC_##reg))
#define CC_R4(base, reg)	(GVType == GVType_gr2 ? \
					base->cc1.reg : \
					gv_r4 (base, DCB_VCC1_PXHACK, DCB_NOACK, \
					V_CC_##reg))
#define CC_W1(base, reg, val)	(GVType == GVType_gr2 ? \
					(base->cc1.reg = (val)) : \
					gv_w1 (base, DCB_VCC1_PXHACK, DCB_NOACK, \
					V_CC_##reg, val))
#define CC_W4(base, reg, val)	(GVType == GVType_gr2 ? \
					(base->cc1.reg = (val)) : \
					gv_w4 (base, DCB_VCC1_PXHACK, DCB_NOACK, \
					V_CC_##reg, val))


#else

#ifdef Traditional_sync
#define AB_R1(base, reg)	(GVType == GVType_gr2 ? \
					base->ab1.reg : \
					gv_r1 (base, DCB_VAB1, DCB_ENASYNCACK, \
					V_AB_##reg))
#define AB_R4(base, reg)	(GVType == GVType_gr2 ? \
					base->ab1.reg : \
					gv_r4 (base, DCB_VAB1, DCB_ENASYNCACK, \
					V_AB_##reg))
#define AB_W1(base, reg, val)	(GVType == GVType_gr2 ? \
					(base->ab1.reg = (val)) : \
					gv_w1 (base, DCB_VAB1, DCB_ENASYNCACK, \
					V_AB_##reg, (int)val))
#define AB_W4(base, reg, val)	(GVType == GVType_gr2 ? \
					(base->ab1.reg = (val)) : \
					gv_w4 (base, DCB_VAB1, DCB_ENASYNCACK, \
					V_AB_##reg, (int)val))

#else

Something is desparately wrong.

#define AB_R1(base, reg)	(GVType == GVType_gr2 ? \
					base->ab1.reg : \
					gv_r1 (base, DCB_VAB1, DCB_ENSYNCACK, \
					V_AB_##reg))
#define AB_R4(base, reg)	(GVType == GVType_gr2 ? \
					base->ab1.reg : \
					gv_r4 (base, DCB_VAB1, DCB_ENSYNCACK, \
					V_AB_##reg))
#define AB_W1(base, reg, val)	(GVType == GVType_gr2 ? \
					(base->ab1.reg = (val)) : \
					gv_w1 (base, DCB_VAB1, DCB_ENSYNCACK, \
					V_AB_##reg, val))
#define AB_W4(base, reg, val)	(GVType == GVType_gr2 ? \
					(base->ab1.reg = (val)) : \
					gv_w4 (base, DCB_VAB1, DCB_ENSYNCACK, \
					V_AB_##reg, val))


#endif

#define CC_R1(base, reg)	(GVType == GVType_gr2 ? \
					base->cc1.reg : \
					gv_r1 (base, DCB_VCC1, DCB_ENSYNCACK, \
					V_CC_##reg))
#define CC_R4(base, reg)	(GVType == GVType_gr2 ? \
					base->cc1.reg : \
					gv_r4 (base, DCB_VCC1, DCB_ENSYNCACK, \
					V_CC_##reg))
#define CC_W1(base, reg, val)	(GVType == GVType_gr2 ? \
					(base->cc1.reg = (val)) : \
					gv_w1 (base, DCB_VCC1, DCB_ENSYNCACK, \
					V_CC_##reg, (int)val))
#define CC_W4(base, reg, val)	(GVType == GVType_gr2 ? \
					(base->cc1.reg = (val)) : \
					gv_w4 (base, DCB_VCC1, DCB_ENSYNCACK, \
					V_CC_##reg, (int)val))
#endif
/* Galileo video hardware type */

#define GVType_unknown	0x0000		/* not initialized yet */
#define GVType_gr2	0x0001		/* GR2 graphics */
#define GVType_ng1	0x0002		/* NG1 graphics */

/*int gv_r1 (mgras_hw *mgbase, int reg_set, int sync, int reg_offset);
int gv_r4 (mgras_hw *mgbase, int reg_set, int sync, int reg_offset);
int gv_w1 (mgras_hw *mgbase, int reg_set, int sync, int reg_offset, int val);
int gv_w4 (mgras_hw *mgbase, int reg_set, int sync, int reg_offset, unsigned int val);*/

extern int gv_r1(mgras_hw *base, int, int, int);
extern int gv_w1(mgras_hw *base, int, int, int, int);
extern int gv_r4(mgras_hw *base, int, int, int);
extern int gv_w4(mgras_hw *base, int, int, int, unsigned int);

extern int GVType;

/* from libsk.h and libsc.h -- we don't include them because they wantss STATUS 
   as a typdef, but re4reg.h uses STATUS as a register define
 */
void us_delay(uint);
extern void     bzero(void *, size_t);
int is_fullhouse(void);
extern int sprintf(char *, const char *, ...);
extern int      atoi(const char *);
extern void     atohex(const char *, int *);
extern char *   atobu(const char *, unsigned *);
extern void     bcopy(const void *, void *, size_t);
void flush_cache(void);

/* in mgv1_paths.c */
extern void v2ga(void);
extern void g2va(void);

/* in mgv1_ev1_init.c */
extern void mgras_init_dcbctrl(void);
extern __int32_t mgv1_i2c_initalize(int, mgras_hw *, struct ev1_info *);
extern void sv2ga(void);
extern void slave(void);

/* ab1_memtest.c */
void mgv1_freeze_all_ABs(void);
