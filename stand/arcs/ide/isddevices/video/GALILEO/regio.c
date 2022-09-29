#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "sys/gfx.h"
#include "sys/gr2.h"
#include "sys/gr2hw.h"
#include "sys/gr2_if.h"
#include "sys/ng1hw.h"
#include "regio.h"

#include "libsk.h"

extern int	GVType;

static int	dcb_setup;
static int	dcb_hold;
static int	dcb_width;
static int	dcb_inited;

void gvio_init (struct gr2_hw *base)
{
	if (GVType == GVType_unknown) {
		if (badaddr(&base->hq.mystery, 4) ||
		    base->hq.mystery != 0xdeadbeef) {
			GVType = GVType_ng1;
		} else {
			GVType = GVType_gr2;
		}
	}
	dcb_setup = 2;
	dcb_hold = 1;
	dcb_width = 2;
	dcb_inited = 1;
}

int gv_r1 (struct gr2_hw *base, int reg_set, int sync, int reg_offset)
{
	int		val, mode;

	if (dcb_inited == 0) gvio_init (base);

	if (GVType & GVType_ng1) {

		volatile struct rex3chip *rex = (volatile struct rex3chip *)
			((unsigned long) base + 0x000f0000);

		mode = reg_set |
			(dcb_setup << DCB_CSSETUP_SHIFT) |
			(dcb_hold << DCB_CSHOLD_SHIFT) |
			(dcb_width << DCB_CSWIDTH_SHIFT) |
			sync |
			(reg_offset << DCB_CRS_SHIFT) |
			DCB_DATAWIDTH_1;
		rex->set.dcbmode = mode;
		val = rex->set.dcbdata0.bybyte.b3;
	} else {
		volatile char *regbase;

		regbase = (reg_set == DCB_VCC1 ?
			(char *) &base->cc1 :
			(char *) &base->ab1);
		val = regbase [reg_offset << 2];
	}
	return (val);
}

int gv_r4 (struct gr2_hw *base, int reg_set, int sync, int reg_offset)
{
	int		val, mode;

	if (dcb_inited == 0) gvio_init (base);

	if (GVType & GVType_ng1) {

		volatile struct rex3chip *rex = (volatile struct rex3chip *)
			((unsigned long) base + 0x000f0000);

		mode = reg_set |
			(dcb_setup << DCB_CSSETUP_SHIFT) |
			(dcb_hold << DCB_CSHOLD_SHIFT) |
			(dcb_width << DCB_CSWIDTH_SHIFT) |
			sync |
			(reg_offset << DCB_CRS_SHIFT) |
			DCB_DATAWIDTH_4;
		rex->set.dcbmode = mode;
		val = rex->set.dcbdata0.byword;
	} else {
		volatile int *regbase;

		regbase = (reg_set == DCB_VCC1 ?
			(int *) &base->cc1 :
			(int *) &base->ab1);
		val = regbase [reg_offset];
	}
	return (val);
}

int
gv_w1 (struct gr2_hw *base, int reg_set, int sync, int reg_offset, int val)
{
	if (dcb_inited == 0) gvio_init (base);

	if (GVType & GVType_ng1) {

		volatile struct rex3chip *rex = (volatile struct rex3chip *)
			((unsigned long) base + 0x000f0000);
		rex->set.dcbmode = 
			reg_set |
			(dcb_setup << DCB_CSSETUP_SHIFT) |
			(dcb_hold << DCB_CSHOLD_SHIFT) |
			(dcb_width << DCB_CSWIDTH_SHIFT) |
			sync |
			(reg_offset << DCB_CRS_SHIFT) |
			DCB_DATAWIDTH_1;
		rex->set.dcbdata0.bybyte.b3 = val;
	} else {
		volatile char *regbase;

		regbase = (reg_set == DCB_VCC1 ?
			(char *) &base->cc1 :
			(char *) &base->ab1);
		regbase [reg_offset << 2] = val;
	}

	return 0;
}

int
gv_w4 (struct gr2_hw *base, int reg_set, int sync, int reg_offset, int val)
{
	if (dcb_inited == 0) gvio_init (base);

	if (GVType & GVType_ng1) {

		volatile struct rex3chip *rex = (volatile struct rex3chip *)
			((unsigned long) base + 0x000f0000);

		rex->set.dcbmode = 
			reg_set |
			(dcb_setup << DCB_CSSETUP_SHIFT) |
			(dcb_hold << DCB_CSHOLD_SHIFT) |
			(dcb_width << DCB_CSWIDTH_SHIFT) |
			sync |
			(reg_offset << DCB_CRS_SHIFT) |
			DCB_DATAWIDTH_4;
		rex->set.dcbdata0.byword = val;
	} else {
		volatile int *regbase;

		regbase = (reg_set == DCB_VCC1 ?
			(int *) &base->cc1 :
			(int *) &base->ab1);
		regbase [reg_offset] = val;
	}

	return 0;
}
