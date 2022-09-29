/* error reporting code */

#define GR2_ERR_REPORT		1

#include "gr2_loc.h"
#include "vb2_loc.h"
#include "ide_msg.h"
#include "uif.h"
#include "xmap.h"

extern int GBL_GRVers;

void
print_GR2_loc(unsigned int unum)
{
	msg_printf(VRB,"Failure board:  	GR2\n");
	msg_printf(VRB,"Failure component:	U%d\n",unum);
}

void
print_GR3_loc(unsigned int unum)
{
	msg_printf(VRB,"Failure board:  	GR3-003\n");
	msg_printf(VRB,"Failure component:	U%d\n",unum);
}

void
print_GU1_loc(unsigned int unum)
{
	msg_printf(VRB,"Failure board:  	GU1-003\n");
	msg_printf(VRB,"Failure component:	U%d\n",unum);
}

void
print_RU1_loc(unsigned int unum)
{
	msg_printf(VRB,"Failure board:  	RU1-002\n");
	msg_printf(VRB,"Failure component:	U%d\n",unum);
}

void
print_VB2_loc(unsigned int unum)
{
	msg_printf(VRB,"Failure board:  	VB2-004\n");
	msg_printf(VRB,"Failure component:	U%d\n",unum);
}

void
print_xmap_loc(unsigned int unum)
{
     if (GBL_GRVers < 4) {
	msg_printf(VRB,"Failure board:  	VB1\n");
	msg_printf(VRB,"Failure component:	U%d\n",GR2_XMAP_U[unum]);
     } else {
	msg_printf(VRB,"Failure board:  	VB2-004\n");
#if IP22
	if(is_indyelan())
		msg_printf(VRB,"Failure component:	U%d\n",VB2_XMAP_U_INDY[unum]);
	else
		msg_printf(VRB,"Failure component:	U%d\n",VB2_XMAP_U[unum]);
#endif
     }


}

void
print_VM2_loc(unsigned int slot, unsigned int unum)
{
     if (GBL_GRVers < 4) {
	msg_printf(VRB,"Failure board:  	VM2\n");
	msg_printf(VRB,"Failure component: Slot:%d,	U%d\n",slot,unum);
     } 
}

void
print_ZB4_loc(unsigned int unum)
{
     if (GBL_GRVers < 4) {
	msg_printf(VRB,"Failure board:  	ZB4\n");
	msg_printf(VRB,"Failure component: 	U%d\n",unum);
     } 
}
