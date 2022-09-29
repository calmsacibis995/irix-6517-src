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

#define Traditional_sync
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

/* Galileo video hardware type */

#define GVType_unknown	0x0000		/* not initialized yet */
#define GVType_gr2	0x0001		/* GR2 graphics */
#define GVType_ng1	0x0002		/* NG1 graphics */

int gv_r1(struct gr2_hw *, int, int, int);
int gv_r4(struct gr2_hw *, int, int, int);
int gv_w1(struct gr2_hw *, int, int, int, int);
int gv_w4(struct gr2_hw *, int, int, int, int);

extern int GVType;
