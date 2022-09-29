/**************************************************************************
 *									  *
 * 			Copyright (C) 1986, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"io/tpsc.c: $Revision: 3.268 $"

#include "sys/debug.h"
#include "sys/sysmacros.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/cpu.h"
#include "sys/buf.h"
#include "sys/conf.h"
#include "sys/edt.h"
#include "sys/cmn_err.h"
#include "sys/signal.h"
#include "sys/sema.h"
#include "sys/errno.h"
#include "sys/sbd.h"
#include "ksys/vfile.h"
#include "sys/mtio.h"
#include "sys/scsi.h"
#include "sys/tpsc.h"
#include "sys/dump.h"
#include "sys/systm.h"
#include "sys/invent.h"
#include "sys/immu.h"	/* for btod */
#include "sys/iograph.h"
#include "sys/kmem.h"
#include "sys/uio.h"
#include "sys/cred.h"
#include "sys/conf.h"
#include "sys/ddi.h"
#include "sys/mload.h"
#include <bstring.h>
#include <fcntl.h>
#include <string.h>
#include "sys/iograph.h"
#include "sys/driver.h"
#include "sys/kabi.h"

int tpscdevflag = D_MP;
char *tpscmversion = M_VERSION;


/*
 * tpsc.c
 *  This module is the driver for all SCSI tapes, including jaguar
 *  VME scsi, since we now go through the switch layer.
 *
 *  Reads and writes for the byte swapped device require byte swapping
 *  in software, so are somewhat slower (about 1 or 2 %).
 *
 *  If you want debugging you must define SCSICT_DEBUG.
 *  This can also be done via symmon or dbx -k.  Current
 *  values uses are 0 (none) through 4 (most verbose).  The
 *  debug stuff is enabled (but ctdebug is 0) if compiled -DDDEBUG
 *
 *  A fairly major change is to no longer rely on passing b_error around
 *  via the assorted bp structs.  ctcmd now returns the error, if any,
 *  and the various routines now pass back error codes.  This actually
 *  simplifies things in a number of cases, and the logic is certainly
 *  much easier to follow.  (rev 3.115, when I folded in the dmedia
 *  changes from bvd, and did a fair amount of other cleanup as well).
 *  Olson, 11/92
*/

/* prototypes for extern functions not in header files */
extern void dki_dcache_wb(void *, int);
extern void dki_dcache_inval(void *, int);

extern void (*scsi_strategy)(struct buf *);

/* prototypes for all functions defined in the driver */

/* entry points via devsw */

int  tpscattach(vertex_hdl_t);
int  tpscdetach(vertex_hdl_t);
	
void tpscinit();
int tpsctapetype(ct_g0inq_data_t *, struct tpsc_types *);
int tpscread(dev_t,uio_t *,cred_t *);
int tpscwrite(dev_t,uio_t *,cred_t *);
extern int tpsc_immediate_rewind; /* in master.d/tpsc */
extern int tpsc_print_ili;
int tpscclose(dev_t, int, int, cred_t *);
int tpscioctl(dev_t, uint, caddr_t, int, cred_t *, int *);
int tpscopen(dev_t *, int, int, cred_t *);

/* OK; so I should just declare ctstrat the way uiophysio prototypes
 * the function argument, but I feel that is simply the wrong way
 * to do it, since the function is in fact void, and the *physio
 * functions are documented as ignoring the return value.  Stupid
 * SVR4 declarations... */
typedef int (*physiofunc)(buf_t *);

/* called directly from with the driver */

static void ctstrategy(buf_t *);
static void ctdrivesense(ctinfo_t *);
static int ctchkerr(ctinfo_t *, short);
static ctinfo_t *ctsetup(dev_t, int *, int);
static int ctgetblklen(ctinfo_t *);
static void ct_badili(ctinfo_t *, int);
static void ct_shortili(ctinfo_t *, int, int);
static int ct_fake_eod(ctinfo_t *);
static int ctloaded(ctinfo_t *);
static int ctgetblkno(ctinfo_t *);
static void ctconvabi( irix5_mtscsi_rdlog_t *, mtscsi_rdlog_t * );
static void ctsave(ctinfo_t *, ctsvinfo_t *);
static void ctrestore(ctinfo_t *, ctsvinfo_t *);
static int ctresidOK(ctinfo_t *);
static int ctisstatdev(dev_t dev);

/* now returns error value */
static int ctcmd(ctinfo_t *, void (*)(), __psunsigned_t, int,
	__psunsigned_t, int);

/*	The rest of these are called only indirectly from ctcmd.
	Note that these commands should generally NOT clear any of the
	CTPOS bits on success or error, since ctchkerr already took
	care of that; if they may move the tape, CTPOS bits should
	usually be cleared in the setup.
*/
static void ctmodesel(ctinfo_t *, int, __psunsigned_t);
static void cttstrdy(ctinfo_t *, int, __psunsigned_t, __psunsigned_t);
static void ctmodesens(ctinfo_t *, int, __psunsigned_t, __psunsigned_t);
static void ctrewind(ctinfo_t *, int, __psunsigned_t);
static void ctspace(ctinfo_t *, int, __psunsigned_t, __psunsigned_t);
static void ctxfer(ctinfo_t *, int, __psunsigned_t, __psunsigned_t);
static void ctwfm(ctinfo_t *, int, __psunsigned_t, __psunsigned_t);
static void ctload(ctinfo_t *, int, __psunsigned_t);
static void ctprevmedremov(ctinfo_t *, int, __psunsigned_t);
static void ctrdlog(ctinfo_t *, int, __psunsigned_t, __psunsigned_t);
static void ctblklimit(ctinfo_t *, int, __psunsigned_t);
static void ctreqaddr(ctinfo_t *, int, __psunsigned_t);
static void ctreadposn(ctinfo_t *, int, __psunsigned_t);
static void ctseekblk(ctinfo_t *, int, __psunsigned_t, __psunsigned_t);
static int fuj_drrp(ctinfo_t *ctinfo);
static void ctfpmsg(ctinfo_t *, int, __psunsigned_t, __psunsigned_t);
static void ctgetpos(ctinfo_t *, int, __psunsigned_t);
static void ctsetpos(ctinfo_t *, int, __psunsigned_t);

static void ctreport(ctinfo_t *, char *fmt, ...);
static void ctalert(ctinfo_t *, char *fmt, ...);	/* for system monitor UI */

static void cterase(ctinfo_t *, int, uint);

static void swapbytes(u_char *ptr, unsigned n);

static void irix5_mt_to_mt(int, void *, void *);


/* set if causes motion; MUST be updated if new commands are added */
static char ctmotion[] = {
	/* tst rdy */	0,
	/* rewind */	1,
	/* get blkaddr */	0,
	/* reqsense */	0,
	/* cmd 4  */	0,
	/* blklimits */	0,
	/* cmd 6  */	0,
	/* cmd 7  */	0,
	/* read */	1,
	/* cmd 9  */	0,
	/* write */	1,
	/* cmd b */	0,
	/* seek */	1,
	/* cmd d */	0,
	/* setpos */	1,
	/* read rev */	1,
	/* wfm */	1,
	/* space */	1,
	/* inquiry */	0,
	/* verify */	1,
	/* recoverdata */	0,
	/* modesel */	0,
	/* reserve */	0,
	/* release */	0,
	/* copy */	1,
	/* erase */	1,
	/* modesense */	0,
	/* load */	1,
	/* rec diag */	0,
	/* send diag */	1,
	/* prevremov */	0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0,
	/* locate */ 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	/* readpos */ 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0,
	/* change def */ 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0,
	/* log sel */ 0,
	/* log sense */ 0
};


#if defined(DEBUG) && !defined(SCSICT_DEBUG)
#define SCSICT_DEBUG 0 /* debugs enabled, but off by default */
#endif
#ifdef SCSICT_DEBUG
/*	note that you may need braces around if(x) ct_dbg() ; else stuff
	because otherwise the else will go with the debug stuff... */
#define	ct_dbg(a)	if(ctdebug) cmn_err a
#define	ct_xdbg(lev, a)	if(ctdebug>=lev) cmn_err a
#define	ct_cxdbg(cond, lev, a)	if((cond) && ctdebug>=lev) cmn_err a
#define C CE_CONT	/* so lines aren't so long! */
static int ctdebug = SCSICT_DEBUG;	/* can be set off/on from debugger */

/* a place holder for undefined or unused cmds */
static char _ct_unk[] = "";

static char *ct_cmds[] = {
	"tst rdy",
	"rewind",
	"getblkaddr",
	"reqsense",
	"cmd 4 ",
	"blklimits",
	"cmd 6 ",
	"cmd 7 ",
	"read",
	"cmd 9 ",
	"write",
	"cmd b",
	"seek",
	"cmd d",
	"setpos",
	"read rev",
	"wfm",
	"space",
	"inquiry",
	"verify",
	"recoverdata",
	"modesel",
	"reserve",
	"release",
	"copy",
	"erase",
	"modesense",
	"load",
	"rec diag",
	"send diag",
	"prevremov",
	_ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk,
	_ct_unk, _ct_unk, _ct_unk, _ct_unk,
	"locate",
	_ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk,
	"readpos",
	_ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk,
	_ct_unk, _ct_unk, _ct_unk,
	"change def",
	_ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk, _ct_unk,
	_ct_unk, _ct_unk, _ct_unk,
	"log sel",
	"log sense"	/* 0x4D; see cmdnm */
};

/* types of space actions */
static char *ctspctyp[] = { 
	"blks",
	"fm",
	"seq fm",
	"eom",
	"setmk"
};

static char *
cmdnm(int x)
{
	static char cbuf[40];
	if((uint)x <= 0x4D && *ct_cmds[x])	/* 0x4D is last cmd in table */
		return ct_cmds[x];
	sprintf(cbuf, "cmd 0x%x not in table", x);
	return cbuf;
}

#else
#define	ct_dbg(a)
#define	ct_xdbg(lev, a)
#define	ct_cxdbg(cond, lev, a)
#endif

#define	ABS(x)	( (x > 0) ? (x) : (-x) )

/* used to convert resid from scsi reqsense into an int */
#define MKINT(x) (((x)[0] << 24) + ((x)[1] << 16) + ((x)[2] << 8) + (x)[3])
/* used mainly for putting counts into scsi command form */
#define	HI8(x)	((u_char)((x)>>16))
#define	MD8(x)	((u_char)((x)>>8))
#define	LO8(x)	((u_char)(x))

extern int tpsc_extra_delay, tpsc_mindelay;	/* from lboot */
static int ctsuppress;
/* some useful macro definitions */

#define dev_to_modevhdl(_dev)	(hwgraph_connectpt_get(_dev))	

#define TLI_UNIT_INFO(p)	(p->tli_unit_info)	
#define ADAP(p) 		(SUI_ADAP(((scsi_unit_info_t *)TLI_UNIT_INFO(p))))
#define TARG(p)			(SUI_TARG(((scsi_unit_info_t *)TLI_UNIT_INFO(p))))
#define LUN(p)			(SUI_LUN(((scsi_unit_info_t *)TLI_UNIT_INFO(p))))
#define LUN_VHDL(p)		(SUI_LUN_VHDL(((scsi_unit_info_t *)TLI_UNIT_INFO(p))))

#define TLI_INFO(dev)		(tpsc_local_info_get(dev_to_vhdl(dev)))

#define TLI_OPENSEMA(dev)	(SUI_OPENSEMA(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(dev)))))
#define TLI_CTINFO_DEV(dev)	((ctinfo_t *)(SUI_CTINFO(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(dev))))))
#define TLI_CTINFO_DEV_L(dev)	(SUI_CTINFO(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(dev)))))
#define TLI_CTINFO_VHDL(vhdl)	((ctinfo_t *)(SUI_CTINFO(scsi_unit_info_get(vhdl))))


/* scsi interface function pointers. these are a part of
 * the info hanging of the appropriate controller 
 * vertex
 */

#define TLI_ALLOC(dev)		(*(SCI_ALLOC(STI_CTLR_INFO(SLI_TARG_INFO(SUI_LUN_INFO(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(dev)))))))))
                                                        /* alloc function pointer from disk info */
#define TLI_FREE(dev)		(*(SCI_FREE(STI_CTLR_INFO(SLI_TARG_INFO(SUI_LUN_INFO(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(dev)))))))))
                                                        /* free function pointer from disk info */
#define TLI_DUMP(dev)		(*(SCI_DUMP(STI_CTLR_INFO(SLI_TARG_INFO(SUI_LUN_INFO(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(dev)))))))))
                                                        /* dump function pointer from disk info */
#define TLI_INQ(dev)		(*(SCI_INQ(STI_CTLR_INFO(SLI_TARG_INFO(SUI_LUN_INFO(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(dev)))))))))
                                                        /* info function pointer from disk info */
#define TLI_IOCTL(dev)		(*(SCI_IOCTL(STI_CTLR_INFO(SLI_TARG_INFO(SUI_LUN_INFO(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(dev)))))))))
                                                        /* ioctl function pointer from disk info */
#define TLI_COMMAND(dev)	(*(SCI_COMMAND(STI_CTLR_INFO(SLI_TARG_INFO(SUI_LUN_INFO(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(dev)))))))))

#define TLI_ABORT(dev)		(*(SCI_ABORT(STI_CTLR_INFO(SLI_TARG_INFO(SUI_LUN_INFO(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(dev)))))))))

#define TLI_LUN_VHDL(dev)	( SLI_LUN_VHDL(SUI_LUN_INFO(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(dev))))))
#define TLI_UNIT_VHDL(dev)	( SUI_UNIT_VHDL(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(dev)))))




#define	NEW(_p)		(_p = kmem_alloc(sizeof(*_p),KM_SLEEP))


/* statemachine stuff in the hardware graph */

/* hang the tpsc unit specfic info off its vertex */
void tpsc_local_info_put(vertex_hdl_t 		tpsc_vhdl,
			 tpsc_local_info_t	*info)
{
	(void)hwgraph_fastinfo_set(tpsc_vhdl,(arbitrary_info_t)info);
}

/* read the info hanging off the tpsc vertex */
tpsc_local_info_t *
tpsc_local_info_get(vertex_hdl_t tpsc_vhdl)
{
	tpsc_local_info_t	*info;

	info = (tpsc_local_info_t *)hwgraph_fastinfo_get(tpsc_vhdl);
	ASSERT(info);
	return info;
}


/* ARGSUSED */

/* find max. number of possible densities given a 
 * particular tape type 
 */
int
ct_sm_num_dens(struct tpsc_types *ttp)
{
	int 	num_dens;

	if (ttp->tp_capablity & (MTCAN_COMPRESS | MTCAN_SETDEN))
	  num_dens = ttp->tp_dens_count;
	else
	  num_dens = 1;
	return num_dens;
}

/* create and edge name for a vaild density value 
 * given the tape type 
*/
char *
ct_sm_dens_name_create(int			dens,
		       struct tpsc_types	*ttp,
		       char			*name)
{
	ASSERT((ttp->tp_capablity & (MTCAN_COMPRESS | MTCAN_SETDEN) && 
		dens < ttp->tp_dens_count) || 
	       (dens == 0));
	sprintf(name, ttp->tp_hwg_dens_names[dens]);
	return name;
}
/* create a statemachine vertex corr. to an instance
 * of the tape property values
 */
vertex_hdl_t
ct_sm_vertex_create(tape_prop_t 	prop,
		    vertex_hdl_t 	known_unit_vhdl,
		    struct tpsc_types	*ttp)	
		    
{
	vertex_hdl_t		new_vertex;
	/* REFERENCED */
	graph_error_t		rv;
	tpsc_local_info_t	*info;

	rv = hwgraph_vertex_create(&new_vertex);
	ASSERT(rv == GRAPH_SUCCESS);

	/* hang off the info associated with this state machine vertex */
	NEW(info);
#if defined(DEBUG) && defined(HWG_DEBUG)
	printf("ct_sm_vertex_create : vhdl = %x , info = 0x%llx\n",new_vertex,info); 
#endif 
	info->tli_unit_info	= scsi_unit_info_get(known_unit_vhdl);/* pointer to  generic 
								       * tape info 
								       */
	info->tli_isstat	= 0;
	info->tli_ttp  		= ttp;		/* pointer to the tape properties
						 * which represent this tape vertex
						 */
	info->tli_prop 		= prop;		/* pointer to the state machine 
						 * specific properties which form
						 * the basis for forming the vertices
						 * in the state machine
						 * basically rewinding,swap,var,compress
						 * density & speed
						 */


	/* hang the info off the new state machine vertex */
	if (!hwgraph_fastinfo_get(new_vertex)) {
		tpsc_local_info_put(new_vertex,info);
	}

	
	return (new_vertex);
}
/* add a transition edge between two state machine vertices */
void
ct_sm_edge_add(vertex_hdl_t 	from_sm_vertex,
	       vertex_hdl_t	to_sm_vertex,
	       char		*edge_name)
{
	/* REFERENCED */
	graph_error_t	rv;

	rv = hwgraph_edge_add(from_sm_vertex,to_sm_vertex,edge_name);
	ASSERT(rv == GRAPH_SUCCESS);
}

/* 
 * check if we have a valid state of the state machine 
 */
int
ct_sm_state_valid(ct_sm_state_t	state,struct tpsc_types *ttp)
{
	/* Make sure that the compress bit of the  state is being
	 * not used since density encodes the compression also.
	 */
	return (((TP_DENSITY(state) 	< ct_sm_num_dens(ttp)) 		&&
		 (TP_VAR(state) 	< NUM_BLKSIZE_MODES(ttp)) 	&&
		 (TP_SPEED(state) 	< NUM_SPEED_MODES(ttp)) 	&&
		 (!TP_COMPRESS(state))));

}

/* get the vertex handle of the vertex corresponding to
 * a particular instance of the tape properties 
 */
vertex_hdl_t
ct_sm_vertex_get(vertex_hdl_t		*vertex,
		 tape_prop_t 		search_prop,
		 struct tpsc_types	*ttp)
{
	ct_sm_state_t		state;
	tape_prop_t		vertex_prop;

	for(state = 0 ; state < MAX_STATES; state++) {
		if (ct_sm_state_valid(state,ttp) && 
		    (vertex[state] != GRAPH_VERTEX_NONE)) {
			vertex_prop 	= tpsc_local_info_get(vertex[state])->tli_prop;
			if (vertex_prop == search_prop)
				return vertex[state];
		}
	}
	return GRAPH_VERTEX_NONE;
}
/* create all the outgoing edges from a given vertex
 * in the state machine
 */
void
ct_sm_edges_from_vertex_create(vertex_hdl_t 		*vertex,
			       vertex_hdl_t		from_vertex,
			       struct tpsc_types	*ttp)
{
	tape_prop_t	vprop,prop;
	vertex_hdl_t	to_vertex;
	int		density;

	vprop 		= tpsc_local_info_get(from_vertex)->tli_prop;

	/* create the edge corresponding to the other possible rewinding mode */
	prop 		= TP_FLIP_BIT(vprop,TP_REWIND,TP_REWIND_SHIFT);
	to_vertex 	= ct_sm_vertex_get(vertex,prop,ttp);
	ASSERT(to_vertex != GRAPH_VERTEX_NONE);
	ct_sm_edge_add(from_vertex,
		       to_vertex,
		       TP_REWIND(prop) ? CT_SM_NOREWIND : CT_SM_REWIND);

	/* create the edge corresponding to the other possible swapping mode */
	prop 		= TP_FLIP_BIT(vprop,TP_SWAP,TP_SWAP_SHIFT);
	to_vertex 	= ct_sm_vertex_get(vertex,prop,ttp);
	ASSERT(to_vertex != GRAPH_VERTEX_NONE);
	ct_sm_edge_add(from_vertex,
		       to_vertex,
		       TP_SWAP(prop) ? CT_SM_SWAP : CT_SM_NONSWAP);

	/* create the other edge corresponding to the blocksize mode if possible */
	
	if (NUM_BLKSIZE_MODES(ttp) == 2) {
		prop 		= TP_FLIP_BIT(vprop,TP_VAR,TP_VAR_SHIFT);
		to_vertex 	= ct_sm_vertex_get(vertex,prop,ttp);
		ASSERT(to_vertex != GRAPH_VERTEX_NONE);
		ct_sm_edge_add(from_vertex,
			       to_vertex,
			       TP_VAR(prop) ? CT_SM_VAR : CT_SM_FIXED);
	}
	/* create edges for the other density property values */
	for (density = 0; density < ct_sm_num_dens(ttp) ; density++){
		char edge_name[50];

		if (density != TP_DENSITY(vprop)) {
			prop = TP_DENSITY_SET(vprop,density);
			to_vertex = ct_sm_vertex_get(vertex,prop,ttp);
			ASSERT(to_vertex != GRAPH_VERTEX_NONE);
			ct_sm_dens_name_create(density,ttp,edge_name);
			ct_sm_edge_add(from_vertex,to_vertex,edge_name);
		}
	}

	/* create the other edge corresponding to the speed mode if possible */
	if (NUM_SPEED_MODES(ttp) == 2) { 
		prop 		= TP_FLIP_BIT(vprop,TP_SPEED,TP_SPEED_SHIFT);
		to_vertex 	= ct_sm_vertex_get(vertex,prop,ttp);
		ASSERT(to_vertex != GRAPH_VERTEX_NONE);
		ct_sm_edge_add(from_vertex,
			       to_vertex,
			       TP_SPEED(prop) ? CT_SM_SPEED1 : CT_SM_SPEED0);
	}
}

/*
 * initialize the list of created densities
 */
void
ct_sm_created_dens_init(int *created_dens)
{
	int i;

	for(i = 0 ; i < MAX_DENSITIES; i++)
		created_dens[i] = -1;
}
/*
 * does the density exist in the list of created densities 
 * if it doesn't return 0 and put in the list otherwise
 * return 1
 */
int
ct_sm_created_dens_exist(int *created_dens,int dens) 
{
	int i;

	for(i = 0 ; i < MAX_DENSITIES ; i++) {
		if (created_dens[i] == dens)
			return 1;
		if (created_dens[i] == -1) {
			created_dens[i] = dens;
			return 0;
		}
	}
	return 0;
}
/*
 * add the device
 */
void
ct_sm_device_add(vertex_hdl_t	vertex)
{
	vertex_hdl_t		vhdl;

	hwgraph_char_device_add(vertex,EDGE_LBL_CHAR,"tpsc",&vhdl);
	hwgraph_chmod(vhdl, HWGRAPH_PERM_TPSC);
	tpsc_local_info_put(vhdl,tpsc_local_info_get(vertex));
	hwgraph_vertex_unref(vhdl);
}
/*
 * remove the device
 */
void
ct_sm_device_remove(vertex_hdl_t	vertex)
{
	vertex_hdl_t		vhdl;
	tpsc_local_info_t	*info = NULL;

	hwgraph_edge_remove(vertex,EDGE_LBL_CHAR,&vhdl);
	hwgraph_cdevsw_set(vhdl,NULL);
	hwgraph_info_remove_LBL(vhdl,INFO_LBL_PERMISSIONS,NULL);
	info = tpsc_local_info_get(vertex);
	if (info)
		kern_free(info);
	hwgraph_vertex_unref(vhdl);
	hwgraph_vertex_destroy(vhdl);
}
#define ADMIN_LBL_SM_VLIST	"statemachine"

/* construct a state machine where vertices correspond
 * to various modes in which the tape can be opened and
 * the edges correspond to a particular property value
 * eg :- a state S1 (non-rewinding,byte-swappable,fixed-blocks,density=8500,non-compress,speed=100)
 *       an edge E1  8200bpi
 *       a state S2 (non-rewinding,byte-swappable,fixed-blocks,density=8200,non-compress,speed=100)
 * basic idea is that when we take the edge E1(in hwgfs a link ) in state S1(in hwgfs a directory)
 * we reach state S2 (another directory in hwgfs)
 */
void
ct_sm_create(vertex_hdl_t 	known_unit_vhdl,
	     struct tpsc_types	*ttp)
{
	vertex_hdl_t	*vertex;
	ct_sm_state_t	state;

	/* REFERENCED */
	graph_error_t	rv;

	/* Allocate memory for the state-array */
	vertex = (vertex_hdl_t *)kern_calloc(MAX_STATES,sizeof(vertex_hdl_t));
	/* create the vertices corresponding to the valid states */
	for ( state = 0 ; state < MAX_STATES; state++)
		if (ct_sm_state_valid(state,ttp))
			vertex[state] = ct_sm_vertex_create(state,known_unit_vhdl,ttp);
		else	
			vertex[state] = GRAPH_VERTEX_NONE;
	
	/* create all the non-looping edges from each vertex corresponding to a valid state */
	for (state = 0 ; state < MAX_STATES ; state++) {
		if (ct_sm_state_valid(state,ttp)) {

			ct_sm_edges_from_vertex_create(vertex,vertex[state],ttp);
			ct_sm_device_add(vertex[state]);


		}
	}

	/* connect point to vertex[0] has already been set to the first vertex in the 
	 * state machine from which there is an edge to this. so explicitly set the
	 * connect point to 0 so that vertex corresponding to the scsi tape can be the
	 * connect point to vertex[0]
	 */
	rv = hwgraph_connectpt_set(vertex[0],GRAPH_VERTEX_NONE);
	ASSERT(rv == GRAPH_SUCCESS);
	/* create a default edge from the unit vhdl to the default
	 * mode of the device 
	 */
	ct_sm_edge_add(known_unit_vhdl,vertex[0],TPSC_DEFAULT);
	/* Store the state-vertex list on the base tape vertex */
	device_admin_info_set(known_unit_vhdl,ADMIN_LBL_SM_VLIST,
			      (char *)vertex);
}

/* ARGSUSED */
/* build the name space of hardware graph vertices for this specific
 * tape type 
 */
void
ct_hwg_namespace_build(vertex_hdl_t tape_vhdl,struct tpsc_types *ttp)
{
        int			*sm_created;		
	tpsc_local_info_t	*info;
	vertex_hdl_t		stat_vhdl;
	/* REFERENCED */
	graph_error_t		rv;
	/* REFERENCED */
	scsi_unit_info_t	*unit_info;

	unit_info = scsi_unit_info_get(tape_vhdl);
	sm_created = &SUI_SM_CREATED(unit_info);
	ASSERT(sm_created);
	if (!*sm_created) {
		/* create the stat device vertex */
		
		rv = hwgraph_char_device_add(tape_vhdl,TPSC_STAT,TPSC_PREFIX,&stat_vhdl);
		ASSERT(rv == GRAPH_SUCCESS);
		hwgraph_vertex_unref(stat_vhdl);
		/* hang the info associated with this stat device */
		NEW(info);
		info->tli_unit_info 	= scsi_unit_info_get(tape_vhdl);
		info->tli_isstat 	= 1;
		info->tli_ttp		= ttp;
		info->tli_prop		= STAT_PROP;	/* open mode properties of a stat device */

		tpsc_local_info_put(stat_vhdl,info);
		/* set the default permissions for the stat device */
		hwgraph_chmod(stat_vhdl,HWGRAPH_PERM_TPSC);
		/* create the state machine */
		ct_sm_create(tape_vhdl,ttp);
		*sm_created = 1;
	}
}
/* build the namespace of various modes in which this 
 * tape device can be opened if it matches one of the
 * valid tpsc types
 */
int
tpscattach(vertex_hdl_t tape_vhdl)
{
	struct tpsc_types		*tpt;
	scsi_unit_info_t		*unit_info;
	u_char				*vid,*pid;
	int				i;
	ct_g0inq_data_t			*inv;
	extern struct tpsc_types 	tpsc_types[];
	extern struct tpsc_types 	tpsc_generic;
	extern int 			tpsc_numtypes;
	char				*str;

	/* Store the fact the device has been (re)attached */
	str = (char *)kern_calloc(1,sizeof(4));
	sprintf(str,"no");
	device_admin_info_set(tape_vhdl,ADMIN_LBL_DETACH,str);

	/* get the inquiry info for this tape */
	unit_info 	= scsi_unit_info_get(tape_vhdl);
	inv		= (ct_g0inq_data_t *)SUI_INV(unit_info);

	/* extract the vendor and the product ids */
	vid		= inv->id_vid;
	pid		= inv->id_pid;
	tpt 		= tpsc_types;

	/* search for the (vendor-id,product-id) pair in the exhaustive
	 * list of tpsc types 
	 */
	for(i=0; i<tpsc_numtypes; i++, tpt++) {
		if(strncmp((char *)tpt->tp_vendor, (char *)vid, tpt->tp_vlen) == 0 &&
		   strncmp((char *)tpt->tp_product, (char *)pid, tpt->tp_plen) == 0)
			break;
	}


	if(i == tpsc_numtypes) {
		/* try to match with the generic tpsc type */
		tpt = &tpsc_generic;
		/* if not found we have a problem */
		if(strncmp((char *)tpt->tp_vendor, (char *)vid, tpt->tp_vlen) ||
		   strncmp((char *)tpt->tp_product, (char *)pid, tpt->tp_plen))
			return(-1);
	}

	/* found . build the name space for this specific tpsc type */
	ct_hwg_namespace_build(tape_vhdl,tpt);
	return(0);
}
/* Tear down the state machine built for the tape device
 */
void
ct_hwg_namespace_remove(vertex_hdl_t tape_vhdl)
{
	graph_edge_place_t	place;
	char			edge_name[100];
	vertex_hdl_t		parent,child;
	vertex_hdl_t		*vertex;
	int			state;
	vertex_hdl_t		stat_vhdl;
	tpsc_local_info_t	*info = NULL;

	/* Remove the edge corr. to  the stat device */
	hwgraph_edge_remove(tape_vhdl,TPSC_STAT,&stat_vhdl);
	/* Remove the info hanging off the stat vertex */
	hwgraph_info_remove_LBL(stat_vhdl,INFO_LBL_PERMISSIONS,NULL);
	info = tpsc_local_info_get(stat_vhdl);
	if (info)
		kern_free(info);
	
	/* Delete the vertex itself after decrementing
	 * the ref. count which increased due to the previous reference 
	 */
	hwgraph_vertex_unref(stat_vhdl);
	hwgraph_vertex_destroy(stat_vhdl);
	/* Remove the default edge */
	hwgraph_edge_remove(tape_vhdl,TPSC_DEFAULT,NULL);

	if (!(vertex = 
	      (vertex_hdl_t *)device_admin_info_get(tape_vhdl,
						    ADMIN_LBL_SM_VLIST)))
		return;
	/* Remove all the edges in the state machine */
	for (state = 0; state < MAX_STATES; state++) {
		parent = vertex[state];
		if (parent == GRAPH_VERTEX_NONE)
			continue;

		/* Remove device associated with this vertex */
		ct_sm_device_remove(parent);

		/* Remove all the edges underneath parent vertex */
		place = EDGE_PLACE_WANT_REAL_EDGES;
		while (hwgraph_edge_get_next(parent,edge_name,
					     &child,&place) == GRAPH_SUCCESS) {
			hwgraph_edge_remove(parent,edge_name,NULL);
			hwgraph_vertex_unref(child);
			place = EDGE_PLACE_WANT_REAL_EDGES;
		}
	}
	/* Remove all the vertices in the state machine */
	for (state = 0 ; state < MAX_STATES; state++) {
		parent = vertex[state];
		if (parent == GRAPH_VERTEX_NONE)
			continue;
		hwgraph_vertex_destroy(parent);
	}
	hwgraph_info_remove_LBL(tape_vhdl,ADMIN_LBL_DETACH,NULL);
	hwgraph_info_remove_LBL(tape_vhdl,ADMIN_LBL_SM_VLIST,NULL);
	kern_free(vertex);
}

/* free all the device specific data maintained
 * by the driver
 */
int
tpscdetach(vertex_hdl_t	tape_vhdl)
{
	scsi_unit_info_t		*unit_info;
	ctinfo_t			*ctinfo;
	extern void			ct_alias_remove(vertex_hdl_t);
	char				*str;

	/* Find out if this device has already been detached. No
	 * need to redo the work in that case. Just a safety check.
	 */
	str = device_admin_info_get(tape_vhdl,ADMIN_LBL_DETACH);
	if (str == NULL) {
		return 0;
	}
	if (strncmp(str,"yes",3) == 0)
	       return(0);
	/* If this is the first time remember the fact that this device
	 * has been detached.
	 */
	str = (char *)kern_calloc(1,4);
	sprintf(str,"yes");
	device_admin_info_set(tape_vhdl,ADMIN_LBL_DETACH,str);
	
	/* Remove the aliases in the /hw/tape directory */
	ct_alias_remove(tape_vhdl);
	/* Clean up the driver specific info maintained for this device */
	unit_info 	= scsi_unit_info_get(tape_vhdl);
	ctinfo		= (ctinfo_t *)SUI_CTINFO(unit_info);
	if (ctinfo)
		kern_free(ctinfo);
	SUI_CTINFO(unit_info) = 0;
	/* Remove the state machine for this tape device */
	ct_hwg_namespace_remove(tape_vhdl);
	/* Remember the fact that the state machine has been
	 * removed 
	 */
	SUI_SM_CREATED(unit_info) = 0;
	return(0);
}	


/* register the tpsc driver */
int
tpscreg(){
#if defined( DEBUG)  && defined (HWG_DEBUG)
	printf("registering tpsc driver\n");
#endif
	scsi_driver_register(SCSI_TAPE_TYPE,TPSC_PREFIX);
	return 0;
}

/* unregister the tpsc driver */
int
tpscunreg()
{
	scsi_driver_unregister(TPSC_PREFIX);
	return 0;
}


void
tpscinit()
{
	static int tpscinit_done;

	
	if(tpscinit_done)
		return;
	ct_dbg((C, "tpscinit (load). "));
	scsi_strategy = ctstrategy;
	tpscinit_done = 1;
}


/* for unloading the driver */
int
tpscunload()
{
	scsi_strategy = NULL;
	return 0;
}


void
tpscnotify(struct scsi_request *req)
{
	ctinfo_t       *ctinfo;

	ctinfo = TLI_CTINFO_VHDL(req->sr_dev_vhdl);
	vsema(&ctinfo->ct_sema);
}


/*	probe to see if the drive exists, and if so what type it is.
	Do the inquiry on each open, since it is possible
	to insert and remove drives in the IP6 while the system
	is up.  If we haven't yet allocated ctinfo yet,
	(very first open on this HA/target), allocate it.
	Allocate the scsi channel structure and initialize it.
	dev is remapped dev.
	Obtains semaphore, if error is >= 0, caller must release the semaphore.
*/
static ctinfo_t *
ctsetup(dev_t dev, int *error, int exclusive)
{
	ctinfo_t	*ctinfo;
	struct scsi_request	*req;
	struct scsi_target_info	*info;
	ct_g0inq_data_t		*inq;
	int err;
	uint_t *ctopens;
	extern struct tpsc_types tpsc_generic;
	mutex_t *opensema;

	opensema = &TLI_OPENSEMA(dev);
	mutex_lock(opensema, PRIBIO);

	ctinfo = TLI_CTINFO_DEV(dev);

	/* if not currently open; do setup stuff.  Do inquiry stuff
	 * even if ctinfo already allocated, as tape drive may have
	 * been hot plugged, or the like. */
	if(!ctinfo || (!ctinfo->ct_regopens && !ctinfo->ct_statopens)) {
		int extrabusy = ctinfo ? (int)ctinfo->ct_extrabusy : 0;
		int aerr;

		/* Use 'scsi_alloc' to keep the device from going away
		 * during the (potentially long) time we're in this loop.
		 * If the alloc fails, don't worry about it.
		 */
		aerr = TLI_ALLOC(dev)(TLI_LUN_VHDL(dev), 1, NULL);
		ct_dbg ((C, "Did %ssuccessful temp scsi_alloc on ctsetup.\n",
			 aerr == SCSIALLOCOK ? "" : "un"));

		while((info = (TLI_INQ(dev)(TLI_LUN_VHDL(dev))))
			== SCSIDRIVER_NULL) {
			/* the scsi_info functions return failure on BUSY status,
			 * so we deal with this by assuming that if we had opened 
			 * OK in the past, and done an immediate mode cmd most recently
			 * that we must have a rewind or the like in progress, and retry
			 * just as ctcmd does.  Won't help after reset, or the like, but
			 * that's OK to fail.
			*/
			if(extrabusy>0) {
				ct_dbg((C, "extrabusy %d, retry info. ", extrabusy));
				delay(BUSY_INTERVAL);
				extrabusy -= BUSY_INTERVAL;
				continue;
			}
			/* If above we succeeded in doing 'scsi_alloc', now
			 * do 'scsi_free' to reverse the effect.
			 */
			ct_dbg ((C, "Will %sTLI_FREE on ctsetup failure.\n",
				 aerr == SCSIALLOCOK ? "" : "not "));
			if (aerr == SCSIALLOCOK)
			  TLI_FREE(dev)(TLI_LUN_VHDL(dev), NULL);
			*error = ENODEV;
			return NULL;
		}

		/* If above we succeeded in doing 'scsi_alloc', now do
		 * 'scsi_free' to reverse the effect.
		 */
		ct_dbg ((C, "Will %sTLI_FREE after inquiry loop.\n",
			 aerr == SCSIALLOCOK ? "" : "not "));
		if (aerr == SCSIALLOCOK)
		  TLI_FREE(dev)(TLI_LUN_VHDL(dev), NULL);

		/*
		 * Verify that this device is really a tape.  Check:
		 *     peripheral device type bit(s)
		 *     removeable media bit(s)
		 *     ansi standard bit(s)
		 */
		inq = (ct_g0inq_data_t *) info->si_inq;
		if (inq->id_pdt != INQ_PDT_CT || !inq->id_rmb || !inq->id_ansi) {
			ct_dbg((C,"%d not a tape drive",dev_to_vhdl(dev)));
			*error = ENODEV;
			return NULL;
		}
	}

	*error = 0;
	if(ctinfo == NULL) {
		ct_dbg((C, "allocate info "));

		ctinfo = (ctinfo_t *)kern_calloc(1, sizeof(*ctinfo));
		TLI_CTINFO_DEV_L(dev) 	= ctinfo;

		/* allocate the buffer headers */
		ctinfo->ct_bp = (buf_t *)kern_calloc(1, sizeof(buf_t));

		/* allocate the region which may be DMAed into */
		ctinfo->ct_bufarea = (union ct_bufarea *)
			kmem_alloc(sizeof(union ct_bufarea), KM_CACHEALIGN);
		ctinfo->ct_es_data = (ct_g0es_data_t *)
			/* zero now, since we now dump the whole thing back to
			 * the user on an MTSCSI_SENSE */
			kmem_zalloc(MAX_SENSE_DATA, KM_CACHEALIGN);

		/*
		 * init the various locks and semaphores
		 *   exclusive use semaphore
		 *   buffer header semaphores
		 *   command complete semaphore
		 */
		init_sema(&ctinfo->ct_bp->b_lock, 1, "ct_lock", minor(dev));
		init_sema(&ctinfo->ct_bp->b_iodonesema, 0,
			 "ct_done", minor(dev));
		init_sema(&ctinfo->ct_sema, 0, "ct_sem", minor(dev));
		init_mutex(&ctinfo->ct_cmdsem, MUTEX_DEFAULT,
		          "ct_cmd", minor(dev));

		/* so ctblklimit() will set them */
		ctinfo->blkinfo.minblksz = 0xffffffff;
		ctinfo->blkinfo.maxblksz = 0;

		/* for timeouts, etc. */
		bcopy(&tpsc_generic, &ctinfo->ct_typ, sizeof(tpsc_generic));
		ctinfo->ct_typ.tp_rewtimeo *= HZ;
		ctinfo->ct_typ.tp_spacetimeo *= HZ;
		ctinfo->ct_typ.tp_mintimeo *= HZ;
		ctinfo->ct_typ.tp_maxtimeo *= HZ;

		req = kmem_alloc(sizeof(struct scsi_request), KM_SLEEP);
		ctinfo->ct_req = req;
		bzero(req, sizeof(*req));
		req->sr_lun_vhdl  	= TLI_LUN_VHDL(dev);
		req->sr_dev_vhdl 	= TLI_UNIT_VHDL(dev);
		req->sr_ctlr		= ADAP(TLI_INFO(dev));
		req->sr_target		= TARG(TLI_INFO(dev));
		req->sr_lun		= LUN(TLI_INFO(dev));
		req->sr_command = (u_char *) &ctinfo->ct_cmd;
		req->sr_sense = (u_char *) ctinfo->ct_es_data;
		req->sr_senselen = MAX_SENSE_DATA;
		req->sr_notify = tpscnotify;
	}

	/* always reset this, in case the driver was unloaded and reloaded */
	ctinfo->ct_req->sr_notify = tpscnotify;

	if(exclusive)
		ctopens = &ctinfo->ct_regopens;
	else
		ctopens = &ctinfo->ct_statopens;
	if(++*ctopens == 1) {
		/* Call the host adaptor driver to initialize a connection,
		 * once per each open type per device.
		 * If the call fails, one of the args in incorrect, or the device is
		 * already in use by another driver, or it doesn't exist/work.
		 * if first open is stat, and then open as regular, we can't
		 * do exclusive.  In practice, this isn't really a problem, and
		 * we have been operating this way since stat was first added.
		 * it does mean we are open to "tricks" like opening the stat device
		 * and sleeping forever, then opening via both tpsc and devscsi.
		 * That's life.
		 */
		int aflag = (exclusive && !ctinfo->ct_statopens) ? exclusive|1 : 1;
		if((err=(TLI_ALLOC(dev)(TLI_LUN_VHDL(dev),
					  aflag, NULL))) != SCSIALLOCOK) {
			ct_dbg((C, "scsialloc fails (excl=%d, aflag=%x, st=%x) ",
				exclusive,aflag, ctinfo->ct_state ));
			*error = err ? err : ENODEV;
			--*ctopens;
			return NULL;
		}
	}
	else if(exclusive) {
		ct_dbg((C, "fail: excl && OPEN; regop=%d statop=%d. ", ctinfo->ct_regopens,ctinfo->ct_statopens));
		*error = EBUSY;
		--*ctopens;
		return NULL;
	}

	if((ctinfo->ct_regopens+ctinfo->ct_statopens) == 1) {
		ctinfo->ct_state |= CT_OPEN;
		ctinfo->ct_bp->b_edev = dev;
		/* otherwise already have, and didn't do scsi_info above */
		ctinfo->ct_info = info;
		ctinfo->ct_scsiv = inq->id_ansi;
		ctinfo->ct_cansync = (inq->id_respfmt == 2) && inq->id_sync;

		/* Find out what kind of device we're using */
		(void) tpsctapetype(inq, &ctinfo->ct_typ);
		/* multiply these here, so we don't need to do it everywhere they
		 * are used */
		ctinfo->ct_typ.tp_rewtimeo *= HZ;
		ctinfo->ct_typ.tp_spacetimeo *= HZ;
		ctinfo->ct_typ.tp_mintimeo *= HZ;
		ctinfo->ct_typ.tp_maxtimeo *= HZ;
		ctinfo->blkinfo.recblksz = ctinfo->ct_typ.tp_recblksz;
		if(!ctinfo->ct_fixblksz) {
			/* only on first open; ioctl settings persist after that */
			if(IS_CIPHER(ctinfo))
				ctinfo->ct_cipherrec = 1;
			if(ctinfo->ct_typ.tp_capablity & MTCAN_SETSP)
				/* default to high speed on first open */
				ctinfo->ct_speed = 1;
			if(!(ctinfo->ct_fixblksz = ctinfo->ct_typ.tp_dfltblksz))
				ctinfo->ct_fixblksz = 1; /* defend against bad data;
					we use this for some value setting later, and could
					get divide by zero if bogus */
		}
	}
	else if(exclusive)	/* if first open was to "stat" device */
		ctinfo->ct_bp->b_edev = dev; /* change it to "regular" */

	/* CT_CHG may be set here.
	 * It is only checked in the next block.
	 * If tstrdy successful, CT_ONL will be set; otherwise ONL gets
	 * set later via ctloaded.
	 * An EAGAIN return indicates drive not ready; i.e., no
	 * tape in drive (or tape not loaded on some drives).
	 * Note that the cipher will return ready even
	 * if a tape isn't in the drive...  Note that if the drive is
	 * re-opened while a previous immediate mode cmd is still running,
	 * this is where we block until it finishes. */
	err = ctcmd(ctinfo, cttstrdy, NULL, SHORT_TIMEO, 0, 1);
	if(err && err != EAGAIN) {
		ct_dbg((C, "TST RDY fails.  "));
		*error = err;
		return ctinfo;
	}

	/* Do this on not ready also, because the tape must
	 * have been unloaded (or device reset) if we aren't
	 * ready.  See this particularly on 9 track drives */
	if((ctinfo->ct_state&CT_CHG) || !(ctinfo->ct_state&CT_ONL)) {
		/* zero the read/write counts */
		ctinfo->ct_readcnt = ctinfo->ct_writecnt = 0;
		/* so ctblklimit() will set them */
		ctinfo->blkinfo.minblksz = 0xffffffff;
		ctinfo->blkinfo.maxblksz = ctinfo->blkinfo.curblksz = 0;
		if(ctinfo->ct_typ.tp_capablity & MTCAN_SILI)
			/* default to reporting ili on a media change */
			ctinfo->ct_sili = 0;
		ctinfo->ct_state &= ~CT_CHG;
	}

        if(exclusive || (ctinfo->ct_statopens == 1 && !ctinfo->ct_regopens)) {
	if(ISVAR_DEV(dev))
		ctinfo->ct_blksz = 1;
	else if(!ctisstatdev(dev))
		/* preserves SETFIX'ed value if done.  We need to do this
		 * on every fixed open, in case we were in variable mode
		 * the last time, otherwise blocksize is 1, and things
		 * don't work too well.  Don't set curblksz, because for
		 * drives that support both fixed and variable mode, we
		 * only want to set the blocksize after a succesful read,
		 * since the media might still be variable, and we only
		 * call getblklen if we haven't yet done it on the BLKLIMITS
		 * ioctl. We zero curblksz on tape changes, so that will catch
		 * a change of media type in the case where we are only
		 * reading.
		 */
		ctinfo->ct_blksz = ctinfo->ct_fixblksz;
	if(!ctinfo->ct_blksz)
		ctinfo->ct_blksz = 1;	/* first open and statdev */

	/* Note that both the HP and the Kennedy 9 track drives
	 * will continue to write at the previous density,
	 * if we don't force an error when the drive is opened at a different
	 * density, and we aren't at BOT and have done some i/o.
	 * It seems reasonable to do this, since both drives do autodensity
	 * selection on read anyway.
	 * We need to allow an 'mt rew' to work even if it was a different
	 * density device.  The HP seems to show the last selected density,
	 * rather than actual, so that is confusing.  We may want to change
	 * this to allow ioctls, but not i/o, but for now, we remove the old
	 * check that caused an open to fail in this case.  This issue came up
	 * while doing qual work on the HP 9 track drive.  Now we just always
	 * set the density here, if drive supports it, and worry about
	 * whether to actually send the modeselect in ctmodesel.
	 */
	if(ctinfo->ct_typ.tp_capablity & MTCAN_SETDEN)
		ctinfo->ct_density =
			ctinfo->ct_typ.tp_density[DENSITY_DEV(dev)];

	/* The DLT drive supports compression.  The DLT7000 also
	 * supports 2 density modes. To get this combination to work,
	 * we fill the first two bytes of tp_density with the code for
	 * one density, the second two bytes with the code for the
	 * other density and turn on compression if bit 0 of the dev_t
	 * density bits is set. The latter mechanism doesn't break
	 * anything because currently compression is turned on by
	 * selecting density code 1 and turned off by selecting
	 * density code 0.
	 */
	if (ctinfo->ct_typ.tp_capablity & MTCAN_COMPRESS) {
	    /* compression: Diana-1 option; standard on Diana-2 and -3 */
		if (IS_FUJD1(ctinfo)) {
			if (ISCOMPRESS_DEV(dev))
				ctinfo->ct_state |= CT_COMPRESS;
		else
				ctinfo->ct_state &= ~CT_COMPRESS;
		}
		else {
		if (DENSITY_DEV(dev)%2)
			ctinfo->ct_state |= CT_COMPRESS;
			else
				ctinfo->ct_state &= ~CT_COMPRESS;
	}
	}

	/* Do a modeselect to set density, drive specific options,
	 * etc.  Do on each open, since no reason not to do so.
	 * OK if fails on some drives, (e.g., Kennedy and Cipher) because the
	 * modeselect will be re-done when the tape is loaded,
	 * which happens only if the user gives us a command that
	 * would move the tape (or requires the tape to be at a
	 * known place).  Therefore we always ignore errors from this
	 * (who knows what tapes will be hooked up by our customers...).
	 * Always do a sense first, so things like audio vs non-audio don't
	 * get reset by a blind modeselect!
	 */
	(void)ctcmd(ctinfo, ctmodesens, MAX_MODE_SEL_DATA, SHORT_TIMEO, 0, 1);
	/* Don't do the mode select on statdev or DST opens */
	if(!ctisstatdev(dev) && !IS_DST(ctinfo))
		(void)ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0, 1);
	}

	if ((IS_FUJD1(ctinfo) || IS_FUJD2(ctinfo)) && !ctisstatdev(dev))
	{
		*error = fuj_drrp(ctinfo);
		if (*error)
			return ctinfo;
	}

	/* do on open in case tape was already loaded; but not for stat device */
	if(exclusive && (ctinfo->ct_typ.tp_capablity&MTCAN_PREV))
		/* turn on the front panel LED; disable tape removal for those
		 * with software controllable eject */
		ctcmd(ctinfo, ctprevmedremov, CT_PREV_REMOV, SHORT_TIMEO, 0, 1);
		
	return ctinfo;
}


/* Do a mode sense to chk state of drive, write protect,
 * etc.  Also deal with the QIC24 stuff.
 * Called only if online (ready).  If offline, opens and
 * some commands (ioctls) can still be done.
*/
static
ctchkstate(ctinfo_t *ctinfo)
{
	int error;

	ct_dbg((C, "chkstate "));
	if(error = ctcmd(ctinfo, ctmodesens, MAX_MODE_SEL_DATA, SHORT_TIMEO, 0, 1))
		return error;

	/* If open for writing, HAVE to forget
	 * about QIC24 and QIC120, since the VIPER150 sense is for
	 * TAPE format, not CARTRIDGE format.  Otherwise if we read
	 * a 600* tape that was written in QIC24 format, then try to
	 * write it without removing the tape, we get an error.  Has
	 * to be done AFTER the modesense.  Fortunately mt
	 * opens it readonly, except when writing fm's, etc., so
	 * mt status will report 'correctly'.  
	 * Rev1.5 and earilier firmware on VIPER150 drives WILL NOT
	 * write a tape written in QIC24 format, regardless of cartridge
	 * type, if it has first been read.  The only remedy is to pop
	 * the cartridge out and then re-insert it; rev1.6 fixes it.
	 * Olson, 1/89 */
	if((ctinfo->ct_openflags & FWRITE) &&
		(ctinfo->ct_state & (CT_QIC24|CT_QIC120)))
		ctinfo->ct_state &= ~(CT_QIC24|CT_QIC120);
	return 0;
}


/* ARGSUSED */
int
tpscopen(dev_t *devp, int flags, int otyp, cred_t *crp)
{
	ctinfo_t *ctinfo;
	int error = 0;
	int isstat = ctisstatdev(*devp);
	dev_t remapdev;

#ifdef BCOPYTEST
int bcopytest(void);
bcopytest();
#endif
	if (!dev_is_vertex(*devp)) {
		ct_dbg((C,"tpscopen : old dev type = %x\n",*devp));
		return EINVAL;
	}
	/* reset minor number to our internal mapping; everything the entire
	 * driver does, except for right here, uses the internal minor values */
	remapdev = *devp;	/* corresponds to a vertex handle !! */
	ct_dbg((C, "tpopen(dev=%x:%x ", *devp, remapdev));

	/* verify dev, chk for device existance, and allocate
	 * any needed data structures. */
	ctinfo = ctsetup(remapdev, &error, isstat ? 0 : SCSIALLOC_EXCLUSIVE);
	if(ctinfo == NULL) {
		if(error < 0) {	/* no semaphore, bad ha/targ, just return err */
			ct_dbg((C, "setup err=%d, return\n", -error));
			return -error;
		}
		if(!error) { /* kern_calloc failed ? */
			ct_dbg((C, "setup err=zero. "));
			error = ENOMEM;
		}
		goto erroropen;
	}

	if(!isstat)	/* don't change the flags on stat device opens, or can
		* mess up some other checks later in driver */
		ctinfo->ct_openflags = flags;

	/* chk states, etc if online */
	if((ctinfo->ct_state & CT_ONL) && (error=ctchkstate(ctinfo)))
		goto erroropen;

	/* get the block limits for use in ctstrategy, etc */
	(void)ctcmd(ctinfo, ctblklimit, 0, SHORT_TIMEO, 0, 1);

	/* See if the drive needs cleaning (but not on extremely frequent
	 * mediad opens!)  Also, don't keep warning unless it keeps getting
	 * set by the drive */
	if(!isstat && (ctinfo->ct_state & CT_CLEANHEADS)) {
		ctalert(ctinfo, "requires cleaning");
		ctinfo->ct_state &= ~CT_CLEANHEADS;
	}

	*devp = remapdev;
	ct_xdbg(3, (C,"%x open OK,", remapdev));
	ct_xdbg(4, (C,"rop=%d sop=%d. ", ctinfo->ct_regopens, ctinfo->ct_statopens));
	mutex_unlock(&TLI_OPENSEMA(remapdev));
	return 0;

erroropen:
	if(ctinfo) {
		int freeit=0;
		if(isstat) {
			if(!--ctinfo->ct_statopens)
				freeit = 1;
		}
		else if(!--ctinfo->ct_regopens)
			freeit = 1;
		ctinfo->ct_state &= CTKEEP;
		if(freeit) {
			ct_dbg((C, "scsi_free on type=%s open err . ", isstat?"stat":"normal"));
			TLI_FREE(remapdev)(TLI_LUN_VHDL(remapdev), NULL);
		}
		else ct_dbg((C, "NO scsi_free on type=%s open err st=%x, rego=%d stato=%d", isstat?"stat":"normal", ctinfo->ct_state, ctinfo->ct_regopens, ctinfo->ct_statopens));
	}
	else ct_dbg((C, "NO cleanup on err=%d, ctinfo=NULL. ", error));

	mutex_unlock(&TLI_OPENSEMA(remapdev));
	return error;
}


/*ARGSUSED1*/
int
tpscclose(dev_t dev, int flags, int otyp, cred_t *crp)
{
	ctinfo_t *ctinfo;
	int			fmcnt;
	mutex_t			*opensema;
	ulong ostate;	/* state before close tape motions */
	int err, error = 0, didprev = 0;
	int isstatdev = ctisstatdev(dev);

	opensema = &TLI_OPENSEMA(dev);
	mutex_lock(opensema, PRIBIO); /* ensure no opens while closing,
		 * so state isn't inconsistent, and so we don't miss writing
		 * FM's, etc. */
	ct_dbg((C, "tpclose(%x) ", dev));
	ctinfo = TLI_CTINFO_DEV(dev);
	if(!ctinfo) { /* be paranoid */
		ct_dbg((C, "but not open... "));
		mutex_unlock(opensema);
		return EBADF;
	}
	ct_xdbg(3,(C, "(%s) stato=%d rego=%d;", isstatdev?"stat":"reg",
			ctinfo->ct_statopens, ctinfo->ct_regopens));

	/* if we are the stat device don't do any of the tape motion stuff,
	 * nor do we clear any state, unless we are "true" last close  */
	if(isstatdev) {
		if(ctinfo->ct_statopens == 0) {
			/* I've seen this one happen; looks like a specfs problem */
			error = EBADF;
			ct_dbg((C, "BOGUS: extra stat close, statcnt==0\n"));
			goto notopen;
		}
		ctinfo->ct_statopens = 0;
		goto free_done;
	}

	ASSERT(ctinfo->ct_regopens == 1);
	ctinfo->ct_regopens = 0;	/* unlike stat; it's always 0 or 1 */

	ct_dbg((C, "after %d/%d r/w st=%x. ",ctinfo->ct_readcnt,
		ctinfo->ct_writecnt, ctinfo->ct_state));

	/*
	** if tape is/was off-line, give up early.  Info is based on last
	** issued command, (read, write, ioctl); similarly if we are the
	** stat device.
	*/
	ostate = ctinfo->ct_state;
	if(!(ostate & CT_ONL)) {
		ct_xdbg(3,(C, "close notonline (st=%x);", ctinfo->ct_state));
		if(ostate&CT_MOTION)	/* only if they tried to move tape */
			error = EIO;
		goto errorclose;
	}

	/* Do not write a FM at EW because multi-volume backups the SGI way
	 * assume that a failed read indicates the end of a volume, but a
	 * short read (as you would see at a FM) indicates a failure when
	 * reading.  We still try to flush any buffered data to tape, since
	 * successful status will have been returned to the program.
	 * If ANSI is set, we must write filemarks even at EW to comply with
	 * the ANSI multi-tape standards.  None of the SGI programs that do
	 * multivol (tar, cpio, bru) will set ANSI, so this works out OK.
	 * Note that for audio tapes, no fm's exist. If any of these fail,
	 * make sure we return some error from close */
	if((ostate & (CT_AUDIO|CT_AUD_MED|CT_WRITE|CT_DIDIO|CT_WRP|CT_QIC24))==
	   (CT_WRITE|CT_DIDIO)) {
		fmcnt=((ostate & (CT_EW|CT_ANSI)) == CT_EW && !ctresidOK(ctinfo))?0:1;
		if(fmcnt && (ctinfo->ct_typ.tp_capablity&MTCAN_LEOD)) {
			/* write 2 fm's, then if !rewind, bsf over one
			 * to be compatible with 1/2" tape drives,
			 * and xm in particular.  Also so MTAFILE will
			 * work.  This is needed because 9 track and similar
			 * drives don't have a well defined EOD, since they
			 * can overwrite.  Instead one has a 'logical' EOD,
			 * defined as 2 sequential FM's. Do as 2 seperate calls,
			 * in case past CT_EW; in this state only one can be
			 * written at a time, but we still need 2 to be ansi
			 * (and defacto standard) compatible. */
			err = error = ctcmd(ctinfo, ctwfm, 1, SHORT_TIMEO, 0, 1);
			error = ctcmd(ctinfo, ctwfm, 1, SHORT_TIMEO, 0, 1);
			if(error) err = error;
			if(!ISTPSCREWIND_DEV(dev) && !error) {
				error = ctcmd(ctinfo, ctspace, SPACE_FM_CODE,
					SPACE_TIMEO, (uint)-1, 1);
			}
			if(err && !error) error = err;
		}
		else {
                       error = ctcmd(ctinfo, ctwfm, fmcnt, SHORT_TIMEO, 0, 1);
                        if(!error && !ISTPSCREWIND_DEV(dev) && IS_EXABYTE(ctinfo))
 {
                                /* EXABYTE won't write out it's EOD marker unless
                                 * we do a reposition after writing the filemark.
*/
                                /*
                                   At least some EXABYTE drives fail on the
                                   followind SEEKS, and therefore to preserve
                                   the workaround, we stop caring if the 
                                   commands involved fail.
                                   Return codes below are only used to keep
                                   the compiler happy.
                                */

                                err = ctcmd(ctinfo, ctspace, SPACE_BLKS_CODE,
                                        XFER_TIMEO(ctinfo->blkinfo.maxblksz), (uint)-1, 1);
                                err = ctcmd(ctinfo, ctspace, SPACE_BLKS_CODE,
                                        XFER_TIMEO(ctinfo->blkinfo.maxblksz), 1, 1);
			}
		}
	}

	/* START HISTORY:
	 * Since we are seeing more and more drives where unload actually
	 * does something, and there have been complaints that the mt status
	 * command returns "not ready" (because we did an unload) we now
	 * NEVER do an unload, but just a rewind if appropriate.  This entire
	 * comment is left so that future maintainers understand the history
	 * of this!  Dave Olson
	 * If ISTPSCREWIND device, then unload/rewind; even if no motion,
	 * since we no longer load/rewind the tape on opens.
	 * However, if it is a Kennedy, DAT, or an Exabyte, just rewind
	 * because unloading actually unloads the tape, and makes
	 * the drive go offline, and next access takes a long time.
	 * Don't unload the Cipher either, since it does so many
	 * stupid things when you do.  The only rationale for it was
	 * that it would turn the light off, but Cipher refused to
	 * change their firmware...
	 * END HISTORY.
	 *
	 * If immediate rewinds are enabled, we have to allow removal first,
	 * so cmd doesn't fail due to busy. It may block anyway, if there was
	 * already an immediate mode cmd running, as would the rewind in that
	 * case (for some drives; DAT allows * the prevremoval while the
	 * immediate cmd is running, QIC does not).  Since all the immediate
	 * mode cmds except seek return the drive to BOT, and one isn't likely
 	 * to do a seek and close on the rewind device anyway, it seems to
	 * be basicly a non-issue, except that if you do something like
	 * 'mt -t /dev/tape rewind', you will wait here until the ioctl
	 * rewind completes; again, not much of a * problem. */
	if(ISTPSCREWIND_DEV(dev)) {
		err = error;
	    if(tpsc_immediate_rewind &&
		(ctinfo->ct_typ.tp_capablity&(MTCAN_PREV|MTCANT_IMM)) == MTCAN_PREV) {
			(void)ctcmd(ctinfo, ctprevmedremov, CT_ALLOW_REMOV, SHORT_TIMEO, 0, 1);
			didprev = 1;
		}
	    error = ctcmd(ctinfo, ctrewind, 0, REWIND_TIMEO, 0, 1);
		if(err && !error) error = err;
	}

errorclose:
	/* turn off the front panel LED; enable tape removal for those
	 * with software controllable eject; may fail if still doing
	 * immediate rewind, but that is OK, already done in that
	 * case; don't do it if we have already done it, as some tape
	 * drives will get busy status on this command while doing an
	 * immediate rewind. */
	if(!didprev && (ctinfo->ct_typ.tp_capablity&MTCAN_PREV))
	    (void)ctcmd(ctinfo, ctprevmedremov, CT_ALLOW_REMOV, SHORT_TIMEO, 0, 1);

	if(ostate&CT_MOTION) {
		if(ctinfo->ct_recov_err) {
			/* only report if user moved tape or we loaded it for
			 * them, to prevent multiple recovered error messages
			 * if there is an error followed by multiple commands
			 * like 'mt status'.  Doesn't do anything for errors
			 * persisting across a reboot though...  Don't report
			 * if only a few errors, or only a few reads/writes, as
			 * the stats aren't really meaningful. */
			/* use scsi2 logsense here eventually if scsiv >= 2 */
			if((ctinfo->ct_readcnt+ctinfo->ct_writecnt) > 100)
			ctreport(ctinfo, "had %d successfully retried commands (%d%% of r/w)\n",
				ctinfo->ct_recov_err,
				(100*ctinfo->ct_recov_err)/(ctinfo->ct_readcnt+ctinfo->ct_writecnt));
			else
			ctreport(ctinfo, "had %d successfully retried commands\n",
				ctinfo->ct_recov_err);
			ctinfo->ct_recov_err = 0; /*	clear after reporting */
		}
		ctinfo->ct_readcnt = ctinfo->ct_writecnt = 0;
	}


free_done:

	if(!ctinfo->ct_regopens && !ctinfo->ct_statopens) {
		ctinfo->ct_state &= CTKEEP; /* clear per-open states */
		ct_xdbg(4,(C, "no open either type, clear nonKEEP. "));
	}
	else {
		ct_cxdbg(ctinfo->ct_regopens, 4,(C, "close done, rego=%d ",ctinfo->ct_regopens));
		ct_cxdbg(ctinfo->ct_statopens, 4,(C, "close done, stato=%d ",ctinfo->ct_statopens));
	}

	TLI_FREE(dev)(TLI_LUN_VHDL(dev),NULL);
notopen:
	ct_dbg((C, "\n"));
	mutex_unlock(opensema);
	return error;
}

/*
 * Read from the device
 * have to do semaphoring here, because ctstrategy can
 * be called from ctgetblklen.
*/
/* ARGSUSED */
int
tpscread(dev_t dev, uio_t *uiop, cred_t *crp)
{
	sema_t *ctsem;
	int error;

	if(ctisstatdev(dev))
		return EINVAL;

	ctsem = &TLI_CTINFO_DEV(dev)->ct_bp->b_lock;

	/* done because of processes that fork, and programs
	 * that use sproc; need to ensure only one of them can
	 * be in scsi.c at a time. */
	psema(ctsem, PRIBIO);
	error = uiophysio((physiofunc)ctstrategy, NULL, dev, B_READ, uiop);
	vsema(ctsem);
	return error;
}

/*
 * Write to the device
 * have to do semaphoring here, because ctstrategy can
 * be called from ctgetblklen.
*/
/* ARGSUSED */
int
tpscwrite(dev_t dev, uio_t *uiop, cred_t *crp)
{
	sema_t *ctsem;
	int error;

	if(ctisstatdev(dev))
		return EINVAL;

	ctsem = &TLI_CTINFO_DEV(dev)->ct_bp->b_lock;

	/* done because of processes that fork, and programs
	 * that use sproc; need to ensure only one of them can
	 * be in scsi.c at a t time. */
	psema(ctsem, PRIBIO);
	error = uiophysio((physiofunc)ctstrategy, NULL, dev, B_WRITE, uiop);
	vsema(ctsem);
	return error;
}

/*
 *   perform the various ioctls that can be issued.
 *
 *   NOTE: if you add a new ioctl that either (potentially) moves the
 *   tape, or counts on the tape already having been loaded for it's
 *   operation, you must call ctloaded() first.  Olson, 1/89
*/
/*ARGSUSED3*/
int
tpscioctl(dev_t dev, uint cmd, caddr_t arg, int flags, cred_t *crp, 
	int *rvalp)
{
	ctinfo_t *ctinfo;
	ctsvinfo_t svinfo;
	buf_t *bp;
	union {
		struct mtop _mtop;
		struct mtget _mtget;
		struct mtgetext _mtgetext;
		struct mtaudio _mtaudio;
	} cpout;
#define mtop cpout._mtop
#define mtget cpout._mtget
#define mtgetext cpout._mtgetext
#define mtaudio cpout._mtaudio

	int cnt, blocksize, error = 0, isstatdev;

	isstatdev = ctisstatdev(dev);
	if( cmd != MTIOCGET && cmd != MTIOCGETEXT && cmd != MTIOCGETEXTL &&
						     isstatdev ) {
		ct_dbg((C, "ioctl %x not GET on statdev. ", cmd));
		return EINVAL;
	}

	ctinfo = TLI_CTINFO_DEV(dev);

	/* used for passing a few things, and resid counts, and
	 * for semaphoring */
	bp = ctinfo->ct_bp;

	ct_dbg((C, "ioctl %x. ", cmd));	/* before the psema */

	/* done because of processes that fork, and programs
	 * that use sproc; need to ensure only one of them can
	 * be in scsi.c at a time. */
	if ( cmd != MTIOCGETEXTL )
		psema(&bp->b_lock, PRIBIO);

	switch(cmd) {
	case ABI_MTIOCTOP:
		if(gencopy(arg, (caddr_t)&mtop, sizeof(mtop), 0)) {
		/*
		 * XXX This has to be "converted"
		 */
			error = EFAULT;
			break;
		}
		switch(mtop.mt_op) {	/* by design, all numbers and the
			structure are the same as the normal ones,
			so no conversion to do; just verify it is
			one of the abi ops */
		case ABI_MTWEOF:
		case ABI_MTFSF:
		case ABI_MTFSR:
		case ABI_MTBSF:
		case ABI_MTBSR:
		case ABI_MTREW:
		case ABI_MTOFFL:
			goto abi_to_normal;
		default:
			error = EINVAL;
			break;
		}
		break;
	case MTIOCTOP:
		if(gencopy(arg, (caddr_t)&mtop, sizeof(mtop), 0)) {
			error = EFAULT;
			break;
		}
abi_to_normal:
		/* if op 'requires' tape already positioned for i/o, check
		 * call ctloaded to make sure it is.  No-ops and commands
		 * that move to a known position are OK without it. */
		switch(mtop.mt_op) {
		case MTNOP:
		case MTAUD:
			break;
		case MTOFFL:
		case MTUNLOAD:
			/* cipher is REALLY bad; you have to be loaded
			 * before you can do an unload! */
			if(!IS_CIPHER(ctinfo))
				break;

		/* Tape needs to be loaded for ret,eom, afile,rew,erase,etc. The
		* load itself is only really needed for drives like Exabyte
		* and Kennedy that really distinguish between offline and
		* online; QIC tapes don't really need it for ret, eom,
		* etc. (except that cipher actually requires a load
		* before these if an unload has been issued...).  We
		* don't check the CT_ONL bit because it is possible that
		* the drive is online after a powerup (tape in drive,
		* etc.), but that we haven't done the modeselect.  If we
		* don't do it this way, then we might load the tape in
		* the strat routine, even though they have rewound or
		* spaced the tape.  We can't just set the CT_MOTION bit
		* in those routines, because then the modeselect done in
		* ctloaded might not happen. This caused problems in 3.2
		* and 3.3 when trying to do 'mt rew; mt feom; tar'
		* immediately after a powerup. The tape got loaded in
		* strat, and clobbered the earlier archives. */
		default:
			if(error=ctloaded(ctinfo))
				goto done;
		}

		switch(mtop.mt_op) {
		case MTWEOF: /* Write File Mark(s) */
			ct_dbg((C, "weof %dp ", mtop.mt_count));

			/* don't allow if they were reading.  */
			if(ctinfo->ct_state & CT_READ) {
				ct_dbg((C, "but was reading.  "));
				error = EINVAL;
				break;	/* this was missing... */
			}

			/* note that write 0 FM is allowed because it flushes
			 * buffered data to tape.  On Archive, it also can
			 * set EOM, which may NOT be returned at writes that
			 * cross early warning, contrary to what manual says.
			 * We actually get a recovered error sometimes under
			 * these circumstances when doing large (1Mb) writes.
			 * Need cnt because otherwise timeout is 0 if count
			 * is 0.  Olson, 2/89 */
			cnt = (mtop.mt_count==0) ? 1 : mtop.mt_count;

			/* multiply delay by count in case very high count.
			 * In particular, Exabyte is a bit slow about
			 * writing FM's. */
			ctinfo->ct_lastreq = MTR_WFM;
			error = ctcmd(ctinfo, ctwfm, (int)mtop.mt_count,
				cnt*SHORT_TIMEO, 0, 1);
			break;
		case MTFSF: /* skip filemark forward */
			ct_dbg((C, "fsf %d, ", mtop.mt_count));
			if(!mtop.mt_count)
				break;

			/* forward skip as many files as the user tells us */
			ctinfo->ct_lastreq = MTR_SFF;
			error = ctcmd(ctinfo, ctspace, SPACE_FM_CODE,
				SPACE_TIMEO, (uint)mtop.mt_count, 1);
			break;
		case MTFSR:	/* skip block forward */
			ct_dbg((C, "fsr %d, ", mtop.mt_count));
			if(!mtop.mt_count)
				break;
			/* if in var mode, assume largest possible blocks, since
			 * we don't know! */
			if(ISVAR_DEV(bp->b_edev))
			    blocksize = ctinfo->blkinfo.maxblksz*mtop.mt_count;
			else
			    blocksize = ctinfo->ct_blksz * mtop.mt_count;
			ctinfo->ct_lastreq = MTR_SRF;
			error = ctcmd(ctinfo, ctspace, SPACE_BLKS_CODE,
				XFER_TIMEO(blocksize), (uint)mtop.mt_count, 1);

			/* not really clear what the best thing to do is here;
			 * ideally we would indicate if we had moved the tape
			 * at all; fortunately, the user program can do this
			 * with the MTIOCGET and the blocks field for newer
			 * drives... */
			if(bp->b_resid && !error)
				error = ENOSPC;
			break;
		case MTAFILE: /* append to last file.  Clears state for
			reading/writing, etc.  */
			if(!(ctinfo->ct_typ.tp_capablity & MTCAN_APPEND)) {
				error = EINVAL;
				break;
			}
			ctinfo->ct_lastreq = MTR_SEOD;
			if (ctinfo->ct_typ.tp_capablity&MTCAN_LEOD) {
			    if(ctinfo->ct_state & CT_FM) {
				/* If we are already at a FM, we had better
				 * backspace over it first, since we could
				 * be between the 2 FM's written at close.
				 * Ignore any errors from the first BSF */
				(void)ctcmd(ctinfo, ctspace, SPACE_FM_CODE,
				    SPACE_TIMEO, (uint)-1, 1);
			    }
			    if(!(error = ctcmd(ctinfo, ctspace, SPACE_SFM_CODE,
				SPACE_TIMEO, 2, 1)))
				error = ctcmd(ctinfo, ctspace, SPACE_SFM_CODE,
				    SPACE_TIMEO, (uint)-2, 1);
			}
			else if(ctinfo->ct_typ.tp_capablity&MTCAN_SPEOD) {
				if(!(error = ctcmd(ctinfo, ctspace,
				     SPACE_EOM_CODE, SPACE_TIMEO, 0, 1)))
				/* Assumes 'valid'; i.e. ends with a FM */
				error = ctcmd(ctinfo, ctspace,
				    SPACE_FM_CODE, SPACE_TIMEO, (uint)-1, 1);
			}
			else {
				error = ct_fake_eod(ctinfo);
				if (!error) error = ctcmd(ctinfo, ctspace,
				    SPACE_FM_CODE, SPACE_TIMEO, (uint)-1, 1);
			}
			break;
		case MTEOM: /* space to end of recorded data */
			ct_dbg((C, "space EOD, "));
			ctinfo->ct_lastreq = MTR_SEOM;
			cnt = ctinfo->ct_typ.tp_capablity & (MTCAN_SPEOD|MTCAN_LEOD);
			if(cnt == MTCAN_LEOD) {
			    /* EOD is a somewhat meaningless term on 9 track due
			     * to overwrite, so we follow the convention that
			     * says 2 sequential FM's are EOD (same as MTAFILE).
			     * Space to AFTER the first and before the second.
			     * If we are already at a FM, we had better BS over
			     * it first, since we could be between the 2 FM's
			     * written at close. Ignore any errors from the
			     * first BSF */
			    if(ctinfo->ct_state & CT_FM)
				(void)ctcmd(ctinfo, ctspace, SPACE_FM_CODE,
				    SPACE_TIMEO, (uint)-1, 1);
			    if(!(error = ctcmd(ctinfo, ctspace, SPACE_SFM_CODE,
				SPACE_TIMEO, 2, 1))) {
				if(!(error = ctcmd(ctinfo, ctspace,
				    SPACE_FM_CODE, SPACE_TIMEO, (uint)-1, 1)))
				    ctinfo->ct_state |= CT_EOD;
			    }
			}
			else if(cnt & MTCAN_SPEOD) { /* easy */
				error = ctcmd(ctinfo, ctspace, SPACE_EOM_CODE,
					SPACE_TIMEO, 0, 1);
				/* This will fail on an 8500 with 8200 tape */
				if(error && (ctinfo->ct_typ.tp_type ==
				    EXABYTE8500)) {
				    ct_dbg((C,"tpsc: 8500 SPEOD on 8200 tape\n"));
				    error = ct_fake_eod(ctinfo);
				}
			}
			else if(!cnt)
				error= ct_fake_eod(ctinfo);
			break;
		case MTBSF: /*	Space backwards not in QIC-104, but all but
			Cipher support it.  count is two's complement.  */
			ct_dbg((C, "bsf %d, ", mtop.mt_count));
			if(!mtop.mt_count)
				break;
			if(!(ctinfo->ct_typ.tp_capablity & MTCAN_BSF)) {
				error = EINVAL;
				break;
			}
			ctinfo->ct_lastreq = MTR_SFB;
			error = ctcmd(ctinfo, ctspace, SPACE_FM_CODE,
				SPACE_TIMEO, (uint)-mtop.mt_count, 1);
			break;

		case MTBSR:
			ct_dbg((C, "bsr %d, ", mtop.mt_count));
			if(!(ctinfo->ct_typ.tp_capablity & MTCAN_BSR)) {
				error = EINVAL;
				break;
			}
			if(!mtop.mt_count)
				break;
			if(ISVAR_DEV(bp->b_edev))
			    blocksize = ctinfo->blkinfo.maxblksz*mtop.mt_count;
			else
			    blocksize = ctinfo->ct_blksz * mtop.mt_count;
			ctinfo->ct_lastreq = MTR_SRB;
			error = ctcmd(ctinfo, ctspace, SPACE_BLKS_CODE,
			    XFER_TIMEO(blocksize), (uint)-mtop.mt_count, 1);

			/* not really clear what the best thing to do is here;
			 * ideally we would indicate if we had moved the tape
			 * at all; fortunately, the user program can do this
			 * with the MTIOCGET and the blocks field for newer
			 * drives... */
			if(bp->b_resid && !error)
				error = ENOSPC;
			break;
		case MTMKPART: /* make partition (DAT & DST) */
			ct_dbg((C, "mk part %d mbytes, ", mtop.mt_count));
			if((ctinfo->ct_state&CT_AUDIO) ||
				!(ctinfo->ct_typ.tp_capablity&MTCAN_PART)) {
				error = EINVAL;
				break;
			}

			/* if doing io with audio media, but not audio mode, clear audio
			 * media  and put drive into correct mode. */
			if(ctinfo->ct_state&CT_AUD_MED) {
				ct_xdbg(1,(C, "aud media; fix. "));
				ctinfo->ct_state &= ~CT_AUD_MED;
				(void)ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0, 1);
				ct_xdbg(1,(C, "new state=%x. ", ctinfo->ct_state));
			}

			/* First get page 0x11 data, set partition, then set */
			if (IS_DST(ctinfo)) {
				if(error = ctcmd(ctinfo, ctmodesens,
						 MSD_IND_SIZE+MS_VEND_LEN, SHORT_TIMEO, 0x11, 1))
					break;
				ASSERT(MS_VEND_LEN >= (2+ctinfo->ct_ms.msd_vend[1]));
				bcopy(ctinfo->ct_ms.msd_vend, ctinfo->ct_msl.msld_vend,
				      2+ctinfo->ct_ms.msd_vend[1]);
			} else {
				if(error = ctcmd(ctinfo, ctmodesens, 
						 MSD_IND_SIZE+0xa, SHORT_TIMEO, 0x11, 1))
					break;
				bcopy(ctinfo->ct_ms.msd_vend, ctinfo->ct_msl.msld_vend, 8);
			}

			cnt = mtop.mt_count;

			if (IS_DST(ctinfo)) {

			    if(cnt == 0 || cnt == 1) {

				/* Here the meaning of the "mt_count" element is changed.
				 * It is interpreted as the number of partitions that the
				 * tape is to have.  0 is accepted to be backward compatible.
				 * One partition is interpreted to mean FDP style
				 * partitioning, which for an Ampex DST is a non-preformatted
				 * single partition volume.
				 * 0 additional partitions == 1 partition total. */
				ctinfo->ct_msl.msld_vend[3] = 0;

				/* Indicate FDP partitioning */
				ctinfo->ct_msl.msld_vend[4] |= (1 << 7); /* FDP bit */

			    } else if (cnt > 1) {

				/* Obtain multiple partitions by using the SDP bit in the
				 * Medium Partition Page 1 and set the number of Additional
				 * Partitions Defined to be "cnt - 1". */
				unsigned add_partitions = cnt - 1;

				/* Just for safety, make sure that the requested number
				 * does not exceed the maximum allowed by the drive. */
				unsigned max_add_partitions = ctinfo->ct_msl.msld_vend[2];

				if (add_partitions > max_add_partitions) {
				    error = EINVAL;
				    break;
				}
				ctinfo->ct_msl.msld_vend[3] = add_partitions;
				ctinfo->ct_msl.msld_vend[4] |= (1 << 6); /* SDP bit */

			    } else {

				error = EINVAL;
				break;

			    }

			    /* This is the worst case for large tapes i.e. 3 hrs. */
			    cnt = DST_FMT_TIMEO;

			} else 
			{
			    if(!cnt) {
				ctinfo->ct_msl.msld_vend[1] = 6; /* no size bytes */
				ctinfo->ct_msl.msld_vend[3] = 0; /* 1 partition */
				/* redef partitions */
				ctinfo->ct_msl.msld_vend[4] = 0x20|0x10;
			    } else {
				/* redef parts, and size is in Mb's, multiple
				* partitions.  There is an off chance that this
				* might work for DATA DAT and QIC-2100C also. */
				ctinfo->ct_msl.msld_vend[1] = 8;
				ctinfo->ct_msl.msld_vend[3] = 1;
				ctinfo->ct_msl.msld_vend[4] = 0x20|0x10;
				ctinfo->ct_msl.msld_vend[8] = (u_char)(cnt >> 8);
				ctinfo->ct_msl.msld_vend[9] = (u_char)cnt;
			    }
			    /* takes time since either way it has to write the
			     * partition.  When making single part, it just rewrites
			     * the sys area.  Need a timeout similar to the i/o 
			     * timeout for the same amount of data, but the i/o
			     * timeouts are a bit long for DAT, so only use half;
			     * shouldn't even take this long, but allow for rewrites
			     * during the testing part, this gives us a safety factor
			     * of about 3. If we ever support > 8Gb DAT (physical
			     * tape capacity, not compressed), we will have to inline
			     * the macro so we can divide before we multiply...
			     * Unfortunately, while this produces reasonable timeouts
			     * for small partitions (10 Mbytes gets 20 minutes,
			     * actually takes about 6), it gives unreasonably long
			     * timeouts for things like 'mt mkpart 1000' (34 hours),
			     * so adjust it a bit.*/
			    if(cnt > 100)
				    cnt /= 5;
			    cnt = XFER_TIMEO(cnt*500000);
		        } /* End of "if (IS_DST(ctinfo)) { ... } else { ... }" */

			ctinfo->ct_state &= ~CTPOS;
			ctinfo->ct_state |= CT_MOTION;

			ctinfo->ct_lastreq = MTR_FORMAT;
			error = ctcmd(ctinfo, ctmodesel, 2+ctinfo->ct_msl.msld_vend[1],
				cnt, 0, 1);
			/* in case of errors, and out of paranoia, always do
			 * modesense to see what type of tape we now have */
			if (IS_DST(ctinfo)) {
				if(!(cnt = ctcmd(ctinfo, ctmodesens, MSD_IND_SIZE+MS_VEND_LEN,
						 SHORT_TIMEO, 0x11, 1))) {
					if(!ctinfo->ct_ms.msd_vend[3])
						ctinfo->ct_state &= ~CT_MULTPART;
					else
						ctinfo->ct_state |= CT_MULTPART;
				}
			} else 
			{
				if(!(cnt = ctcmd(ctinfo, ctmodesens, MSD_IND_SIZE+0xa,
						 SHORT_TIMEO, 0x11, 1))) {
					if(!ctinfo->ct_ms.msd_vend[3])
						ctinfo->ct_state &= ~CT_MULTPART;
					else
						ctinfo->ct_state |= CT_MULTPART;
				}
			}
			if(cnt && !error) error = cnt;
			break;
		case MTSETPART: /* change partition (DAT & DST) */
			ct_dbg((C, "setpart %d, ", mtop.mt_count));
			if(!(ctinfo->ct_typ.tp_capablity&MTCAN_PART)) {
				error = EINVAL;
				break;
			}
			ctinfo->ct_state &= ~CTPOS;
			ctinfo->ct_state |= CT_MOTION;

			bp->b_blkno = mtop.mt_count;
			ctinfo->ct_lastreq = MTR_PART;
			error = ctcmd(ctinfo, ctseekblk, 1, SPACE_TIMEO,
				MTAUDPOSN_PROG, 1);
			break;
		case MTSKSM: /* skip setmark (negative is back) (DAT) */
			ct_dbg((C, "sksmk %x, ", mtop.mt_count));
			if(!(ctinfo->ct_typ.tp_capablity&MTCAN_SETMK)) {
				error = EINVAL;
				break;
			}
			if((int)mtop.mt_count>0 &&
			    (ctinfo->ct_state&(CT_FM|CT_DIDIO|CT_READ)) ==
			    (CT_FM|CT_DIDIO|CT_READ)) {
				mtop.mt_count -= 1;
				ct_dbg((C, "hit smk on last cmd (read); decr to %d. ",
				    mtop.mt_count));
				if(!mtop.mt_count) {
					ctinfo->ct_state &= ~(CT_DIDIO|CT_READ);
					break;
				}
			}
			ctinfo->ct_lastreq = MTR_SSM;
			error = ctcmd(ctinfo, ctspace, SPACE_SETM_CODE,
				SPACE_TIMEO, mtop.mt_count, 1);
			break;
		case MTWSM: /* write mt_count setmarks (DAT) */
			cnt = mtop.mt_count;
			ct_dbg((C, "wrsmk %d, ", cnt));
			if(cnt<=0 || !(ctinfo->ct_typ.tp_capablity&MTCAN_SETMK)) {
				error = EINVAL;
				break;
			}
			if(ctinfo->ct_state & CT_READ) {
				/* don't allow if they were reading.  */
				ct_dbg((C, "but was reading.  "));
				error = EINVAL;
				break;
			}
			/* multiply delay by count in case very high count. */
			ctinfo->ct_lastreq = MTR_WSM;
			error = ctcmd(ctinfo, ctwfm, (int)cnt,
			    (cnt?cnt:1)*SHORT_TIMEO, 2, 1);
			break;

		case MTAUD: /* switch in and out of audio mode for writing; also
			* allows write after read in ctstrategy; note that they
			* can start writing at ANY point, not just EOD or FM */
			cnt = mtop.mt_count;
			ct_dbg((C, "audiomode %d, ", cnt));
			if(!(ctinfo->ct_typ.tp_capablity&MTCAN_AUDIO)) {
				error = EINVAL;
				break;
			}
			/* set CT_AUDIO to force modeselect to audio; clear if
			 * error or turning off.  If turning it on, forget all
			 * positioning info other than BOT. */
			if(cnt)
				ctinfo->ct_state |= CT_AUDIO;
			else
				ctinfo->ct_state &= ~CT_AUDIO;
			ctinfo->ct_lastreq = MTR_MODEAUD;
			if(error = ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0, 1))
			    /* probably doesn't support audio */
			    ctinfo->ct_state &= ~CT_AUDIO;
			/* rewind if tape loaded; DAT drive can get confused
			 * when switching from audio to data mode */
			if(ctinfo->ct_state & CT_LOADED)
			    /* ignore any errors from the rewind */
			    ctcmd(ctinfo, ctrewind, NULL, REWIND_TIMEO, 0, 1);
			break;
		case MTREW: /* Rewind */
			ct_dbg((C, "rewind, "));
			ctinfo->ct_lastreq = MTR_REW;
			error = ctcmd(ctinfo, ctrewind, NULL, REWIND_TIMEO, 0, 1);
			break;
		case MTERASE:	/* erase from CURRENT position to EOT */
			ct_dbg((C, "erase "));
			if(ctinfo->ct_state&CT_AUDIO) {
				error = EINVAL;	/* doesn't work "right" in audio mode,
					although f/w doesn't return an error unless the
					tape is blank... */
				break;
			}

			ctinfo->ct_lastreq = MTR_ERASE;

			/* if doing io with audio media, but not audio mode, clear audio
			 * media  and put drive into correct mode. */
			if(ctinfo->ct_state&CT_AUD_MED) {
				ct_xdbg(1,(C, "aud media; fix. "));
				ctinfo->ct_state &= ~CT_AUD_MED;
				(void)ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0, 1);
				ct_xdbg(1,(C, "new state=%x. ", ctinfo->ct_state));
			}

			/* 8mm and takes ~2 hours to erase the max length tape
			 * (same time as to write entire tape).  Note that DAT
			 * doesn't erase, it just writes an EOD marker. 
			 * DLT works the same way as 8mm but is ~2.5 hrs. */
			cnt = 4 * REWIND_TIMEO + MAXLONG_TIMEO;
			error = ctcmd(ctinfo, cterase, 1, cnt, 0, 1);
			break;

		case MTRET: /* retension */
			ct_dbg((C, "retension "));
			ctinfo->ct_lastreq = MTR_RETEN;
			if((ctinfo->ct_typ.tp_capablity&MTCANT_RET)) {
				/* no retension cmd; space to EOM then load */
				(void)ctcmd(ctinfo, ctspace, SPACE_EOM_CODE,
					SPACE_TIMEO, 0, 1);
				/* no error chk, no matter what the failure,
				try to reposition to BOT */
				cmd = L_UL_LOAD;
			}
			else
				cmd = L_UL_RETENSION | L_UL_LOAD;
			/* if not at BOT, need 3 times rewind delay, since
				may need to rewind entire tape first, else 2 */
			if(ctinfo->ct_state&CT_BOT)
			    error = ctcmd(ctinfo, ctload, cmd,
				REWIND_TIMEO*2, 0, 1);
			else
			    error = ctcmd(ctinfo, ctload, cmd,
				REWIND_TIMEO*3, 0, 1);
			break;

		case MTNOP:	/* No-Op  */
			break;
		case MTUNLOAD:
			ct_dbg((C, "unload "));
			if(ctinfo->ct_typ.tp_capablity&MTCAN_PREV)
			    (void)ctcmd(ctinfo, ctprevmedremov, CT_ALLOW_REMOV,
				SHORT_TIMEO, 0, 1);
			ctinfo->ct_lastreq = MTR_UNLOAD;
			error = ctcmd(ctinfo, ctload, 0, REWIND_TIMEO, 0, 1);
			break;
		case MTOFFL:	/* off-line: unload it */
			ct_dbg((C, "offl "));
			ctinfo->ct_lastreq = MTR_UNLOAD;
			error = ctcmd(ctinfo, ctload, 0, REWIND_TIMEO, 0, 1);
			break;
		case MTSEEK:	/* seek to given block.  On DAT in audio mode,
			* this is actually the program #.  For times see
			* MTSETAUDIO also */
			ct_dbg((C, "seekblk 0x%x, ", mtop.mt_count));
			if(!(ctinfo->ct_typ.tp_capablity&MTCAN_SEEK)) {
				error = EINVAL;
				break;
			}
			bp->b_blkno = mtop.mt_count;
			/* really don't know how long this will take, although
			 * most that implement it do so about the same speed as
			 * spacing to FM, so on the order of 30 seconds.  For
			 * audio, must be leadin,leadout, or 0-799. (we take
			 * 0 to be leadin, just because it seems so obivious) */
			if((ctinfo->ct_state&CT_AUDIO) && (uint)bp->b_blkno >
			    799 && bp->b_blkno != 0xEEE && bp->b_blkno != 0xBBB)
			    error = EINVAL;
			else {
			    ctinfo->ct_lastreq = MTR_PABS;
			    error = ctcmd(ctinfo, ctseekblk, 0, SPACE_TIMEO,
				MTAUDPOSN_PROG, 1);
			    if (!error) { 
			       ctinfo->ct_state &= ~(CT_DIDIO|CT_READ|CT_WRITE);
			       /* For the Exabyte 8500, we have to set BOT
                                  flag by hand when performing a seek to 
                                  block 0 
                                */
			       if (ctinfo->ct_typ.tp_type == EXABYTE8500 && 
				   mtop.mt_count == 0)
				   ctinfo->ct_state |= CT_BOT;
			    }

			}
			break;
		default:
			ct_dbg((C, "unsupported mt_op 0x%x.  ", mtop.mt_op));
			error = EINVAL;
			break;
		} /* end of MTIOCTOP */
		break;

	case MTIOCGET: /* Get tape status */
		/* dsreg is driver specific, and is just a copy of low word
		 * of ct_status, The bits that used to be XS* are still the
		 * same values as they used to be so they are compatible
		 * with older programs.  dposn is new and is just the
		 * position bits.  Note: if we haven't yet loaded the tape, then
		 * try to load it first, so that people doing 'mt status', etc.
		 * can more easily find out if the tape is really ready, even
		 * if no 'motion-causing' tape commands have been issued since
		 * inserting the tape.  Note that this means issuing this ioctl
		 * now causes 9 track, DAT, and 8mm tapes to load the tape, even
		 * if you didn't really want that...  Enough customers have
		 * complained about this, that the occasional person who doesn't
		 * like it will just have to live with it.  Hmm; sometimes the
		 * DAT drive doesn't even have ONL set when it gets here, if the
		 * tape was never accessed before.  So just call ctloaded; it
		 * will return early if tape already loaded.  Also check state,
		 * in case program has device open, and media is changed
		 * (write protect, data/audio, etc.).  If we get EBUSY from
		 * the testready, then OR in CT_SEEKING, because we must
		 * be doing a seek or rewind with immediate mode set.  This
		 * currently happens only in audio mode.
		 * NOTE: *all* errors from any called routines are ignored,
		 * as the ioctl itself should always succeed if we get to this
		 * point.  Failures result only in not having info filled in
		 *
		 * If the STAT device is being used, don't do anything that would
		 * cause tape movement or change tape params.  For some tape drives
		 * (like 9 track), this may end up reporting no media present;
		 * tough.
		 */
		ct_cxdbg(isstatdev, 1, (C, "GET on statdev, no load or getblkno. "));
		ctsave(ctinfo,&svinfo);
		if(!isstatdev)
			(void)ctloaded(ctinfo);
		(void)ctchkstate(ctinfo);
		/* do *not* want to block here if drive is busy, since this is
		 * how we poll for completion when immediate mode cmds are
		 * running. */
		if(cnt = ctcmd(ctinfo, cttstrdy, NULL, -SHORT_TIMEO, 0, 1))
		    cnt = (cnt == EBUSY) ? CT_SEEKING : 0;
		if(isstatdev)
			mtget.mt_blkno = 0;	/* unknown */
		else 
			mtget.mt_blkno = ctgetblkno(ctinfo);

		ctrestore(ctinfo,&svinfo);
		mtget.mt_dsreg = (ushort)ctinfo->ct_state;
		/* 
		 * For correct handing of writing past LEOT,
		 *   MT_EW set  --> LEOT encountered on writes
		 *   MT_EOT set --> if MTANSI disabled, LEOT encountered on writes,
		 *                  if MTANSI enabled, PEOT encountered on writes,
		 *                  PEOT encountered on reads.
		 * Since CT_EOT is set when PEOT is encountered
		 * regardless of whether MTANSI is enabled or not, the
		 * following fudge was to be made to pass the
		 * "correct" value of MT_EOT to the app.  */
		{
		  uint_t ctstate = ctinfo->ct_state;
		  
		  if ((ctstate&CT_EOM) || ((ctstate&CT_EW) && !(ctstate&CT_ANSI)))
		    ctstate |= MT_EOT;
		  mtget.mt_dposn = ctstate & MT_POSMSK;
		}
		/* erreg has upper half of ct_state, for use by mt stat, etc */
		mtget.mt_erreg = (ushort)((ctinfo->ct_state|cnt)>>16);
		mtget.mt_fileno = 0;	/* no meaning in this driver */
		mtget.mt_type = MT_ISSCSI;
		/* DAT; partnum set in getblkno() */
		mtget.mt_resid = ctinfo->ct_partnum;
		/*
		 * The foll sys_cred check is needed so that callers of
		 * tpscstat() can get back the information in kernel buffers.
		 */
		if(gencopy((caddr_t)&mtget, arg, sizeof(mtget), (crp == sys_cred)? 1 : 0))
			error = EFAULT;
		break;

	case MTIOCGETEXT:			/* Get extended tape status */
	case MTIOCGETEXTL:		   /* Get last extended tape status */
		{
		uint_t ctstate = ctinfo->ct_state;

		ct_dbg((C, "getext"));

		if ( cmd == MTIOCGETEXT ) {
			if ( !isstatdev && !(ctinfo->ct_state & CT_LOADED) )
				ctinfo->ct_state2 = 0;
			else
				ctinfo->ct_state2 &= ~CT_NOT_READY;

			ctsave(ctinfo,&svinfo);
			if ( !isstatdev )
				(void)ctloaded(ctinfo);
			(void)ctchkstate(ctinfo);
			/* do *not* want to block here if drive is busy, since
			 * this is how we poll for completion when immediate 
			 * mode cmds are  running. */
			if (cnt = ctcmd(ctinfo, cttstrdy, NULL, -SHORT_TIMEO,
								 0, 1))
				cnt = (cnt == EBUSY) ? CT_SEEKING : 0;

			if ( isstatdev )
				mtgetext.mt_blkno = 0;		/* unknown */
			else 
				mtgetext.mt_blkno = ctgetblkno(ctinfo);

			ctrestore(ctinfo,&svinfo);
		}

		mtgetext.mt_dsreg = (ushort)ctinfo->ct_state;
		/* 
		 * For correct handing of writing past LEOT,
		 *   MT_EW set  --> LEOT encountered on writes
		 *   MT_EOT set --> if MTANSI disabled, LEOT encountered on
		 *		    writes, if MTANSI enabled, PEOT encountered
		 *		    on writes, PEOT encountered on reads.
		 * Since CT_EOT is set when PEOT is encountered
		 * regardless of whether MTANSI is enabled or not, the
		 * following fudge was to be made to pass the
		 * "correct" value of MT_EOT to the app.
		 */  
		if ( (ctstate & CT_EOM) || ( (ctstate & CT_EW) && 
					    !(ctstate & CT_ANSI) ) )
			ctstate |= MT_EOT;
		mtgetext.mt_type     = MT_ISSCSI;
		mtgetext.mt_dposn    = ctstate & MT_POSMSK;
		mtgetext.mt_erreg[0] = (ushort)((ctinfo->ct_state|cnt)>>16);
		mtgetext.mt_erreg[1] = ctinfo->ct_state2;
		mtgetext.mt_fileno   = 0;
		mtgetext.mt_partno   = ctinfo->ct_partnum;
		mtgetext.mt_cblkno   = ctinfo->ct_cblknum;
		mtgetext.mt_lastreq  = ctinfo->ct_lastreq;
		mtgetext.mt_resid    = ctinfo->ct_reqcnt - ctinfo->ct_complete;
		if ( mtgetext.mt_resid < 0 )
			mtgetext.mt_resid = -mtgetext.mt_resid;
		mtgetext.mt_ilimode  = ctinfo->ct_ilimode;
		mtgetext.mt_buffmmode = ctinfo->ct_buffmmode;
		mtgetext.mt_subtype    = ctinfo->ct_typ.tp_hinv;
		mtgetext.mt_capability = ctinfo->ct_typ.tp_capablity;
		if ( gencopy( (caddr_t)&mtgetext, arg, sizeof(mtgetext), 0) < 0)
			error = EFAULT;
		break;
		}

	case MTIOCKGET: {
		struct kmtget	kmtget;

		if (crp != sys_cred) {		/* !user invoked */
			error = EPERM;
			break;
		}
		kmtget.mt_state = ctinfo->ct_state;
		kmtget.mt_dens = -1;
		if(ctinfo->ct_typ.tp_capablity & MTCAN_SETDEN)
			kmtget.mt_dens = ctinfo->ct_density;
		kmtget.mt_block = -1;
		if ((!ISVAR_DEV(bp->b_edev)))
			kmtget.mt_block = ctinfo->ct_blksz;
		bcopy((caddr_t)&kmtget, arg, sizeof(kmtget));
		break;
	     }

	case MTCAPABILITY:
		{ struct mt_capablity caps;
		bzero(&caps, sizeof(caps));	/* zero expansion fields for
			sanity */
		caps.mtsubtype = ctinfo->ct_typ.tp_hinv;
		caps.mtcapablity = ctinfo->ct_typ.tp_capablity;
		if(gencopy((caddr_t)&caps, arg, sizeof(caps), 0))
			error = EFAULT;
		break;
		}
	case MTIOCGETBLKINFO:
		if(!ctinfo->blkinfo.curblksz)  {
			ct_dbg((C, "blkinfo; need to get curblksz "));
			(void)ctloaded(ctinfo);
			(void)ctgetblklen(ctinfo);
		}
		if(gencopy((caddr_t)&ctinfo->blkinfo, arg,
			sizeof(ctinfo->blkinfo), 0))
			error = EFAULT;
		break;
	case MTIOCGETBLKSIZE:
		/* this gets rounded UP into multiples of 512 bytes; NOT the
		 * actual block size in use, since archive programs use it to
		 * decide how many bytes of buffer space to allocate, assuming
		 * a 512 byte block size.
		 * This should become obsolete with BLKINFO, but is
		 * maintained for use of existing programs.
		 * If we can't get the blksize (writing, not at BOT or FM,
		 * etc.), just report the recommended value.  Doesn't
		 * work if in audio mode. */
		if(ctinfo->ct_state&CT_AUDIO) {
			error = EINVAL;
			break;
		}
		blocksize = ctgetblklen(ctinfo);
		if(blocksize <= 0)
			blocksize = ctinfo->blkinfo.recblksz;
		else if(!ISVAR_DEV(bp->b_edev))
		    /* fixed blocks; adjust to match 'old' meaning */
		    blocksize = (ctinfo->blkinfo.recblksz/blocksize)*blocksize;
		/* else use what we got */
		blocksize = (blocksize + 511) / 512;
		ct_xdbg(2,(C, "iocgetblksize ret %d ", blocksize));
		if(gencopy((caddr_t)&blocksize, arg, sizeof(blocksize),0)) {
			error = EFAULT;
			break;
		}
		break;
	case MTSCSIINQ:
		{
		struct scsi_target_info	*info;
		info = (TLI_INQ(dev)(TLI_LUN_VHDL(dev)));
		if(info == NULL) {
			error = ENODEV;	/* shouldn't happen! */
			break;
		}
		/* Redo the inquiry every time.  There are reasons for this.
		 * Don't optimize it by stashing the data and returning the
		 * same data every time.
		 */
		if (gencopy(info->si_inq, arg, sizeof(ct_g0inq_data_t), 0))
			error = EFAULT;
		break;
		}

	case MTSCSI_SENSE:
		/* note MAX_SENSE_DATA, not MTSCSI_SENSE_LEN */
		if (gencopy(ctinfo->ct_es_data, arg, MAX_SENSE_DATA, 0))
			error = EFAULT;
		break;

	case MTSCSI_RDLOG:

#if _MIPS_SIM == _ABI64
		if ( !ABI_IS_IRIX5_64(get_current_abi()) ) {
			irix5_mtscsi_rdlog_t i5_rdlog;

			error = gencopy( arg, &i5_rdlog, 
					 sizeof(irix5_mtscsi_rdlog_t), 0 );
			if ( !error )
				ctconvabi( &i5_rdlog, &ctinfo->ct_rdlog );
		} else
#endif /* _ABI64 */
			error = gencopy( arg, (caddr_t)&ctinfo->ct_rdlog,
					 sizeof(mtscsi_rdlog_t), 0 );

		if ( error < 0 ) {
			error = EFAULT;
			break;
		}
		if ( ctinfo->ct_rdlog.mtlen > MTSCSI_LOGLEN ) {
			error = EINVAL;
			break;
		}
		ctinfo->ct_logdata = kmem_alloc( ctinfo->ct_rdlog.mtlen,
						 KM_CACHEALIGN );
		ctinfo->ct_lastreq = MTR_RDLOG;
		if ( !(error = ctcmd( ctinfo, ctrdlog, 0, SHORT_TIMEO, 0,
								       1 )) ) {
			if ( gencopy( ctinfo->ct_logdata,
				      ctinfo->ct_rdlog.mtarg,
				      ctinfo->ct_rdlog.mtlen, 0 ) < 0 ) {
				error = EFAULT;
			}
		}
		kern_free(ctinfo->ct_logdata);
		ctinfo->ct_logdata = NULL;
		break;
	case MTSETAUDIO:
		if(!(ctinfo->ct_typ.tp_capablity&MTCAN_AUDIO)) {
			error = EINVAL;
			break;
		}
		if(gencopy(arg, (caddr_t)&mtaudio, sizeof(mtaudio), 0)) {
			error = EFAULT;
			break;
		}
		switch(mtaudio.seektype) {
		case MTAUDPOSN_PROG:	/* 3 pnum bytes, plus index */
			bcopy(&mtaudio, ctinfo->ct_posninfo, 4);
			break;
		case MTAUDPOSN_PTIME:
			bcopy(&mtaudio.ptime, ctinfo->ct_posninfo,
				sizeof(mtaudio.ptime));
			break;
		case MTAUDPOSN_ABS:
			bcopy(&mtaudio.atime, ctinfo->ct_posninfo,
				sizeof(mtaudio.atime));
			break;
		case MTAUDPOSN_RUN:
			bcopy(&mtaudio.rtime, ctinfo->ct_posninfo,
				sizeof(mtaudio.rtime));
			break;
		default:
			error = EINVAL;
			goto done;
		}
		ctinfo->ct_lastreq = MTR_PAUDIO;
		ct_dbg((C, "setaudpos %d: %x,%x,%x,%x.  ", mtaudio.seektype,
			ctinfo->ct_posninfo[0], ctinfo->ct_posninfo[1],
			ctinfo->ct_posninfo[2], ctinfo->ct_posninfo[3]));
		error = ctcmd(ctinfo, ctseekblk, 0, SPACE_TIMEO, mtaudio.seektype, 1);
		break;
	case MTGETAUDIO:
		if(!(ctinfo->ct_typ.tp_capablity&MTCAN_AUDIO)) {
			error = EINVAL;
			break;
		}
		ctinfo->ct_lastreq = MTR_GAUDIO;

		/* ensure future expansion fields are 0 for lazy users */
		bzero(&mtaudio, sizeof(mtaudio));
		if((error=ctgetblkno(ctinfo)) >= 0) {
			bcopy(&ctinfo->ct_posninfo[4], &mtaudio, 16);
			if(gencopy((caddr_t)&mtaudio, arg, sizeof(mtaudio),0))
				error = EFAULT;
			ct_xdbg(4,(C, "getaud data:  %x,%x,%x,%x  %x,%x,%x,%x  %x,%x,%x,%x  %x,%x,%x,%x\n",
			ctinfo->ct_posninfo[4], ctinfo->ct_posninfo[5], ctinfo->ct_posninfo[6], ctinfo->ct_posninfo[7],
			ctinfo->ct_posninfo[8], ctinfo->ct_posninfo[9], ctinfo->ct_posninfo[10], ctinfo->ct_posninfo[11],
			ctinfo->ct_posninfo[12], ctinfo->ct_posninfo[13], ctinfo->ct_posninfo[14], ctinfo->ct_posninfo[15],
			ctinfo->ct_posninfo[16], ctinfo->ct_posninfo[17], ctinfo->ct_posninfo[18], ctinfo->ct_posninfo[19]));
		}
		else error = -error;
		break;

	case MTABORT:	/* send abort message, then do a ctchkerr.
		Used for aborting immediate mode rewinds and seeks. */
		ct_dbg((C, "Abort "));
		if (TLI_ABORT(dev)(ctinfo->ct_req) == 0)
			ctreport(ctinfo, "abort msg failed\n");
		/* clear position info; we have no idea where we are! */
		ctinfo->ct_state &= ~CTPOS;
		ctinfo->ct_tent_state = 0;	/* no pending state bits */
		break;

	case MTSPECOP: /* special driver operations */
		/* WARNING:  different drive types use the same bits to mean
		 * different things, so the check for drive type must remain.
		 * This is somewhat unfortunate, and should be cleaned up in
		 * the future... */
		ct_dbg((C, "SPECOP: "));
		if(gencopy(arg, (caddr_t)&mtop, sizeof(mtop), 0)) {
			error = EFAULT;
			break;
		}
		switch(mtop.mt_op) {
		case MTSCSI_CIPHER_SEC_ON:
			if(IS_CIPHER(ctinfo))
				ctinfo->ct_cipherrec = 0;
			else
				error = EINVAL;
			break;
		case MTSCSI_CIPHER_SEC_OFF:
			if(IS_CIPHER(ctinfo))
				ctinfo->ct_cipherrec = 1;
			else
				error = EINVAL;
			break;
		case MTSCSI_SILI: /* suppress ILI on reads */
			ct_dbg((C, "SILI "));
			if(!(ctinfo->ct_typ.tp_capablity&MTCAN_SILI)) {
				error = EINVAL;
				break;
			}
			ctinfo->ct_sili = 1;
			break;
		case MTSCSI_EILI: /* enable ILI on reads */
			ct_dbg((C, "EILI "));
			if(!(ctinfo->ct_typ.tp_capablity&MTCAN_SILI)) {
				error = EINVAL;
				break;
			}
			ctinfo->ct_sili = 0;
			break;
		case MTSCSI_SPEED:	/* set hi/lo speed on Kennedy. Persists
			till reset or reboot */
			ct_dbg((C, "SETSPEED "));
			if(!(ctinfo->ct_typ.tp_capablity&MTCAN_SETSP) ||
			    (uint)mtop.mt_count>1) {
			    error = EINVAL;
			    break;
			}
			if(mtop.mt_count)
				ctinfo->ct_speed = 1;	/* high */
			else
				ctinfo->ct_speed = 0;	/* low */
			ctinfo->ct_lastreq = MTR_ATTR;
			error = ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0, 1);
			break;
		case MTSCSI_SETFIXED:
			ct_dbg((C, "SETFIXED=%d ", mtop.mt_count));
			/* Set the block size for fixed mode.  Must be the
			 * fixed block device, and must be at a FM, BOT,
			 * or no tape motion yet.  Only device that support
			 * variable block sizes work; this is caught by the
			 * min and maxblksz check below. */
			if(ISVAR_DEV(bp->b_edev) ||
				mtop.mt_count < ctinfo->blkinfo.minblksz ||
				mtop.mt_count > ctinfo->blkinfo.maxblksz) {
				error = EINVAL;
				break;
			}
			/* ctloaded is called to handle case of rewind-close-open-ioctl */
			/* cttstrdy is called to block if no close-open */
			(void)ctloaded(ctinfo);	/* in case imm rewind in progress */
			(void) ctcmd(ctinfo, cttstrdy, NULL, SHORT_TIMEO, 0, 1);
			ctinfo->blkinfo.curblksz = ctinfo->ct_blksz =
				ctinfo->ct_fixblksz = mtop.mt_count;
			/* tell the drive to use the new block size */
			ctinfo->ct_lastreq = MTR_ATTR;
			error = ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0, 1);
			ct_dbg((C, "to %d.  ", mtop.mt_count));
			break;
		case MTSCSI_SM:	/* How DAT setmarks are handled.  default is
			* to treat similar to FM (also if non-zero in mt_count).
			* 0 in mt_count causes setmarks to be ignored.  Lasts
			* until reboot or reset.
			* First get page 0x10 data, change RSMK bit, then set */
			ct_dbg((C, "%s setmarks, ", mtop.mt_count?"handle":
			    "ignore"));
			if(!(ctinfo->ct_typ.tp_capablity&MTCAN_SETMK)) {
				error = EINVAL;
				break;
			}
			ctinfo->ct_lastreq = MTR_ATTR;
			error = ctcmd(ctinfo, ctmodesens, MSD_IND_SIZE+0x10,
			    SHORT_TIMEO, 0x10, 1);
			if(error)
				break;
			bcopy(ctinfo->ct_ms.msd_vend, ctinfo->ct_msl.msld_vend, 0x10);
			if(mtop.mt_count)	/* handle RSMK */
				ctinfo->ct_msl.msld_vend[8] |= 0x20;
			else
				ctinfo->ct_msl.msld_vend[8] &= ~0x20;
			error = ctcmd(ctinfo, ctmodesel, 0x10, SHORT_TIMEO, 0, 1);
			break;
		case MTSCSI_LOG_OFF: /* Disable logging of sense data */
			ct_dbg((C, "Disable-Sense-Logging "));
			ctinfo->ct_logsense = 0;
			break;
		case MTSCSI_LOG_ON: /* Enable logging of sense data */
			ct_dbg((C, "Enable-Sense-Logging "));
			ctinfo->ct_logsense = 1;
			break;
		default:
			ct_dbg((C, "unsupported specop 0x%x.  ", mtop.mt_op));
			error = EINVAL;
			break;
		} /* end of MTSPECOP */
		break;
	case MTILIMODE:	/* Set overlength ILI reporting mode */
		ct_dbg((C, "MTILIMODE=%d ", arg));	
		ctinfo->ct_ilimode = arg ? 1 : 0;
		error = ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0, 1);
		break;
	case MTANSI:	/* allow ansi tape labels; that is, i/o past
		 * eot marker.  Only works correctly on 9 track tapes so far.
		 * May work on exabyte and DAT.  QIC allows you to write it, but
		 * then you don't get the indication that you passed the eot
		 * marker, so the label gets returned as regular data. */
		ct_dbg((C, "MTANSI=%d ", arg));
		if(arg)
			ctinfo->ct_state |= CT_ANSI;
		else
			ctinfo->ct_state &= ~CT_ANSI;
		break;

		default:
		ct_dbg((C, "unsupported ioctl 0x%x.  ", cmd));
		error = EINVAL;
		break;
	case MTALIAS:	
	{		
		vertex_hdl_t		tape_vhdl;
		vertex_hdl_t		lun_vhdl;
		struct tpsc_types	*ttp;
		tpsc_local_info_t	*tpsc_local_info;
		/* REFERENCED */
		graph_error_t		rv;
		extern void		ct_alias_make(vertex_hdl_t,
						      vertex_hdl_t,
						      struct tpsc_types *);

		tpsc_local_info = tpsc_local_info_get(dev);
		lun_vhdl = LUN_VHDL(tpsc_local_info);
		rv = hwgraph_traverse(lun_vhdl, EDGE_LBL_TAPE, &tape_vhdl);
		ASSERT(rv == GRAPH_SUCCESS);
		ttp = tpsc_local_info->tli_ttp;
		ASSERT(ttp);
		ct_alias_make(tape_vhdl,dev,ttp);
		hwgraph_vertex_unref(tape_vhdl);
		break;
	}

	case MTGETATTR:	/* Return device attributes */
	{
#if _MIPS_SIM == _ABI64
		int			convert_abi = !ABI_IS_IRIX5_64(get_current_abi());
#endif /* _ABI64 */
		struct mt_attr		mt_attr_data;
		char			*ma_name;
		char			*ma_value;
		int			ma_vlen;
		tpsc_local_info_t	*tli = TLI_INFO(dev);
		struct tpsc_types	*ttp = tli->tli_ttp;

#if _MIPS_SIM == _ABI64
		if (convert_abi) {
			struct irix5_mt_attr	i5_mt_attr_data;

			error = copyin(arg, &i5_mt_attr_data, sizeof(struct irix5_mt_attr));
			if (!error)
				irix5_mt_to_mt(MTGETATTR, &i5_mt_attr_data, &mt_attr_data);
		} else
#endif /* _ABI64 */
			error = copyin(arg, &mt_attr_data, sizeof(struct mt_attr));
		if (error)
			goto done;

		ma_name = kern_calloc(1, MT_ATTR_MAX_NAMELEN+1);
		ma_value = kern_calloc(1, MT_ATTR_MAX_VALLEN+1);

		error = copyin(mt_attr_data.ma_name, ma_name, MT_ATTR_MAX_NAMELEN);
		if (error)
			goto mt_attr_error;

		if (strcmp(ma_name, MT_ATTR_NAME_REWIND) == 0) {
			if (!TP_REWIND(tli->tli_prop))
				strcpy(ma_value, MT_ATTR_VALUE_TRUE);
			else
				strcpy(ma_value, MT_ATTR_VALUE_FALSE);
		}
		else
		if (strcmp(ma_name, MT_ATTR_NAME_VARIABLE) == 0) {
			if (TP_VAR(tli->tli_prop) && (ttp->tp_capablity & MTCAN_VAR))
				strcpy(ma_value, MT_ATTR_VALUE_TRUE);
			else
				strcpy(ma_value, MT_ATTR_VALUE_FALSE);
		}
		else
		if (strcmp(ma_name, MT_ATTR_NAME_SWAP) == 0) {
			if (TP_SWAP(tli->tli_prop))
				strcpy(ma_value, MT_ATTR_VALUE_TRUE);
			else
				strcpy(ma_value, MT_ATTR_VALUE_FALSE);
		}
		else
		if (strcmp(ma_name, MT_ATTR_NAME_COMPRESS_DENS) == 0) {
			ASSERT(TP_DENSITY(tli->tli_prop) < ttp->tp_dens_count);
			strcpy(ma_value, ttp->tp_hwg_dens_names[TP_DENSITY(tli->tli_prop)]);
		}
		else
		if (strcmp(ma_name, MT_ATTR_NAME_DEVSCSI) == 0) {
			vertex_hdl_t	lun_vhdl = LUN_VHDL(tli);
			vertex_hdl_t	scsi_vhdl;
			/* REFERENCED */
			graph_error_t	rv;

			rv = hwgraph_traverse(lun_vhdl, EDGE_LBL_SCSI, &scsi_vhdl);
			ASSERT(rv == GRAPH_SUCCESS);
			hwgraph_vertex_unref(scsi_vhdl);
			
			vertex_to_name(scsi_vhdl, ma_value, MT_ATTR_MAX_VALLEN);
		}
		else
			error = EINVAL;

		if (!error) {
			ma_vlen = strlen(ma_value);
			ma_vlen = (ma_vlen <= mt_attr_data.ma_vlen) ? ma_vlen : mt_attr_data.ma_vlen;
			error = copyout(ma_value, mt_attr_data.ma_value, ma_vlen);
			if (!error)
				*rvalp = ma_vlen;
		}

	mt_attr_error:
		kern_free(ma_name);
		kern_free(ma_value);
		break;
	} /* MTGETATTR */

	case MTBUFFMMODE: /* Set buffered filemark writing mode */
	{
		ct_dbg((C, "MTBUFFMODE=%d ", arg));	
		ctinfo->ct_buffmmode = arg ? 1 : 0;
	
		break;
	} /* MTBUFFMMODE */

	case MTSETPOS:		/* Position to vendor specific position (DST). */
	{

		ct_dbg((C, "\nvend. set pos., "));

		/* Only valid for DST. */
		if (!IS_DST(ctinfo)) {
			ct_dbg((C, "Not DST.\n"));
			error= EINVAL;
			break;
		}

		/* Get memory for vendor specific position data. */
		ctinfo->ct_vpos = kmem_alloc(sizeof(struct vendor_specific_pos), KM_CACHEALIGN);

		/* Get position type (ignored) and data size. */
		if (gencopy(arg, (caddr_t)ctinfo->ct_vpos, (sizeof(short)*2), 0)) {
			error = EFAULT;
			kmem_free(ctinfo->ct_vpos, sizeof(struct vendor_specific_pos));
			break;
		}

		/* Only valid data size is 14. */
		if (ctinfo->ct_vpos->size != 14) {
			ct_dbg((C, "Invalid size.\n"));
			error = EINVAL;
			kmem_free(ctinfo->ct_vpos, sizeof(struct vendor_specific_pos));
			break;
		}

		/* Get position data. */
		if (gencopy(arg, (caddr_t)ctinfo->ct_vpos, ((sizeof(short)*2)+ctinfo->ct_vpos->size), 0)) {
			error = EFAULT;
			kmem_free(ctinfo->ct_vpos, sizeof(struct vendor_specific_pos));
			break;
		}

		/* Send position data to drive. */
		ctinfo->ct_lastreq = MTR_SPOS;
		ctinfo->ct_state &= ~CTPOS;
		error = ctcmd(ctinfo, ctsetpos, ctinfo->ct_vpos->size, SPACE_TIMEO, 0, 1);
		/* Since this is a positioning operation,
		 * we should allow write after read, etc. after
		 * it */
		ctinfo->ct_state &= ~(CT_DIDIO|CT_READ|CT_WRITE);

		kmem_free(ctinfo->ct_vpos, sizeof(struct vendor_specific_pos));
		break;
	} /* End of MTSETPOS. */

	case MTGETPOS:		/* Get vendor specific position (DST). */
	{
		u_char	*ptr = (u_char *)arg;

		ct_dbg((C, "vend. get pos., "));

		/* Only valid for DST. */
		if (!IS_DST(ctinfo)) {
			ct_dbg((C, "Not DST.\n"));
			error= EINVAL;
			break;
		}

		/* Get memory for vendor specific position data. */
		ctinfo->ct_vpos = kmem_alloc(sizeof(struct vendor_specific_pos), KM_CACHEALIGN);

		/* Get position type. */
		if (gencopy(arg, (caddr_t)ctinfo->ct_vpos, sizeof(short), 0)) {
			error = EFAULT;
			kmem_free(ctinfo->ct_vpos, sizeof(struct vendor_specific_pos));
			break;
		}

		/* Validate position type and set up data size. */
		switch (ctinfo->ct_vpos->position_type) {
			case BLKPOSTYPE:
			case FSNPOSTYPE:
			case DISPOSTYPE:
				ctinfo->ct_vpos->size = 48;
				break;
			default:
				ct_dbg((C, "Invalid type.\n"));
				error = EINVAL;
				break;
		}

		if (error) {
			kmem_free(ctinfo->ct_vpos, sizeof(struct vendor_specific_pos));
			break;
		}

		/* Get position data. */
		ctinfo->ct_lastreq = MTR_GPOS;
		if (error = ctcmd(ctinfo, ctgetpos, ctinfo->ct_vpos->position_type, SHORT_TIMEO, 0, 1)) {
			kmem_free(ctinfo->ct_vpos, sizeof(struct vendor_specific_pos));
			break;
		}

		/* Return position data to caller now because the pointer
		 * to it will be destoyed by the next call to ctcmd(). */
		if (gencopy((caddr_t)ctinfo->ct_vpos, arg, ((sizeof(short)*2)+ctinfo->ct_vpos->size), 0)) {
			error = EFAULT;
			kmem_free(ctinfo->ct_vpos, sizeof(struct vendor_specific_pos));
			break;
		}

		/* Calculate and save caller's address of Ampex Specific
		 * Format Mode Sense Page. */
		ptr += ((sizeof(short)*2) + ctinfo->ct_vpos->size);

		/* Release memory for vendor specific position data. */
		kmem_free(ctinfo->ct_vpos, sizeof(struct vendor_specific_pos));

		/* Get Ampex Specific Format Mode Sense Page. */
		if (error = ctcmd(ctinfo, ctmodesens, MSD_IND_SIZE+MS_VEND_LEN, SHORT_TIMEO, 0x2a, 1))
			break;

		/* Return Ampex Specific Format Mode Sense Page to caller. */
		if (gencopy((caddr_t)&ctinfo->ct_ms, (caddr_t)ptr, ctinfo->ct_ms.msd_len, 0))
			error = EFAULT;

		break;
	} /* End of MTGETPOS. */

	case MTFPMESSAGE:
	{

#if _MIPS_SIM == _ABI64
		int                     convert_abi = !ABI_IS_IRIX5_64(get_current_abi());
#endif /* _ABI64 */
		struct mt_fpmsg         mt_fpmsg_data;
		char                    *mm_msg;
		tpsc_local_info_t       *tli = TLI_INFO(dev);
		struct tpsc_types       *ttp = tli->tli_ttp;

#if _MIPS_SIM == _ABI64
		if (convert_abi) {
			struct irix5_mt_fpmsg    i5_mt_fpmsg_data;
			
			error = copyin(arg, &i5_mt_fpmsg_data, sizeof(struct irix5_mt_fpmsg));
			if (!error)
				irix5_mt_to_mt(MTFPMESSAGE, &i5_mt_fpmsg_data, &mt_fpmsg_data);
		} else
#endif /* _ABI64 */
			error = copyin(arg, &mt_fpmsg_data, sizeof(struct mt_fpmsg));
		if (error)
			goto mt_fpmsg_error;

		if (ttp->tp_fpmsg_cdblen == 6 || ttp->tp_fpmsg_cdblen == 10) {
			mm_msg = kern_calloc(1, MT_FPMSG_MAX_MSGLEN);
			bcopy(mt_fpmsg_data.default_mm_msg, mm_msg, MT_FPMSG_MAX_MSGLEN);
			error = ctcmd(ctinfo, ctfpmsg, (__psunsigned_t)mm_msg, SHORT_TIMEO, mt_fpmsg_data.mm_mlen, 1);
			kern_free(mm_msg);
		}
		else
			error = EINVAL;

        mt_fpmsg_error:
		break;
	} /* MTFPMESSAGE */

	}	/* end of all ioctls switch */
done:
	if ( cmd != MTIOCGETEXTL )
		vsema(&ctinfo->ct_bp->b_lock);
	ct_cxdbg(error, 1, (C, "IOCTL ERROR 0x%x. ", error));
	return error;
#undef mtop
#undef mtget
#undef mtaudio
}


/*
 * ctstrategy
 *  Strategy routine for reads and writes. We do high level error
 *  checking here.  The buf_t * may be from physio, or ct_bp
 *	if called from within the driver
*/
static void
ctstrategy(buf_t *bp)
{
	ctinfo_t *ctinfo;
	int extraresid = 0, needswapbk = 0;
	int skippedfm = 0, error = 0;
	ulong ostate;
	unsigned bcount = bp->b_bcount;	/* fewer bp derefs, and also, 
		do not want to change b_bcount, or iounmap() from the 
		biophysio() can get messed up */

	if(bcount == 0)
		goto done; /* check here so we don't fail size chks */

	ctinfo = TLI_CTINFO_DEV(bp->b_edev);

	/* note that this will also skip over a FM when reading, if one was
	 * encountered on an earlier read, etc. */
	if((error=ctloaded(ctinfo)) || !(ctinfo->ct_state & CT_ONL)) {
		ct_dbg((C, "strat: offline (st %x).  ", ctinfo->ct_state));
		if(!error)
		    error = EIO;
		goto errdone;
	}

	/* abort if we are positioned at EOM (not EW), or (EW and writing
	 * unless ansi has been set).  On reads we allow reading until
	 * we get EOD.  This is primarily because with buffered
	 * writes, some amount of data may be written past EW, the
	 * exact amount is unknown (and unknowable at read time).
	 * Even the QIC drives allow this.  Otherwise we can return
	 * a higher write count than can be read back.  Ignore this
	 * in audio mode. */
	if(!(ctinfo->ct_state & CT_AUDIO) && ((ctinfo->ct_state & CT_EOM) ||
	    (ctinfo->ct_state & (CT_WRITE|CT_EW|CT_ANSI)) == (CT_WRITE|CT_EW))) {
		ct_dbg((C, "strat: at EOM.  "));
	    error = ENOSPC;
	    goto errdone;
	}

	/* check for ct_blksz == 0 because we have twice had bugs
	 * that could cause us to crash here.  I've fixed those,
	 * but this time I'm going to be paranoid, and leave the check */
	if(!ctinfo->ct_blksz || bcount % ctinfo->ct_blksz)
		goto badcnt; /*	must be no remainder.  */

	/* If we are doing a read, we must check that the device has
	 * never done a write. All of our tapes fail an attempt to
	 * alternate reads and writes between filemarks.  intermixed
	 * reads and writes are allowed in audio mode to allow for editting.  */
	if(bp->b_flags & B_READ) {
		if((ctinfo->ct_state & (CT_AUDIO|CT_WRITE)) == CT_WRITE) {
			ct_dbg((C, "tried to read after writing.  "));
			error = EINVAL;
			goto errdone;
		}

		/* If we are at a filemark and last read was short, but NOT 0,
		 * then set resid to count and return.  This can only happen in
		 * fixed block mode, and is necessary so that the program knows
		 * it encountered a FM.  */
		if(ctinfo->ct_state & CT_HITFMSHORT) {
			ct_dbg((C, "HITFM set on read; skip i/o. "));
			bp->b_resid = bp->b_bcount;
			/* also cleared on any tape motion via CTPOS in ctcmd */
			ctinfo->ct_state &= ~CT_HITFMSHORT;
			goto done;
		}

		ctinfo->ct_state |= CT_READ;
	}
	else {
		if((ctinfo->ct_state & (CT_AUDIO|CT_READ)) == CT_READ) {
			ct_dbg((C, "write tried after read.  "));
			error = EINVAL;
			goto errdone;
		}

		if(ctinfo->ct_state & CT_WRP) {
			ct_dbg((C, "strat: WRP.  "));
			error = EROFS;
			goto errdone;
		}

		/* VIPER 150 tape can read old 310 oersted tapes, but can't
		 * write them.  Only set if drive is Viper150. Olson, 11/88.  */
		if(ctinfo->ct_state & CT_QIC24) {
			ct_dbg((C, "strat: bad dens.  "));
			error = EINVAL;	/* can't distinguish from write
			    after read; too bad, only error that makes sense */
			goto errdone;
		}
		ctinfo->ct_state |= CT_WRITE;
	}

	/* Check AFTER we set CT_READ or CT_WRITE whether we are at
           EOD, in which case return ENOSPC. */
	if ((ctinfo->ct_state & (CT_READ|CT_EOD)) == (CT_READ|CT_EOD)) {
	  ct_dbg((C, "strat: at EOD.  "));
	  error = ENOSPC;
	  goto errdone;
	}

	if(ISVAR_DEV(bp->b_edev)) {
		/* check that request is within drive limits */
		if(bcount < ctinfo->ct_cur_minsz)
			goto badcnt;
		/* catch attempts to open the variable mode device for 
		 * drives that don't support it. Be paranoid, in case
		 * minsz not set due to an error. */
		if(ctinfo->ct_cur_minsz &&
			ctinfo->ct_cur_minsz == ctinfo->ct_cur_maxsz &&
			(bcount%ctinfo->ct_cur_minsz)) {
			ct_dbg((C, "min==max, and remainder; prob var mode not allowed.  "));
			goto badcnt;
		}
		if(bcount > ctinfo->ct_cur_maxsz) {
			if(bp->b_flags & B_READ) {	/* adjust down, this
				* allows determining the block size in the standard
				* unix method, by using a large read request. */
				extraresid =  bcount - ctinfo->ct_cur_maxsz;
				bcount = ctinfo->ct_cur_maxsz;
			}
			else
				goto badcnt;
		}
	}
	else if(bcount % ctinfo->ct_blksz) {
		ct_dbg((C, "bcnt=%x, blksz=%x, bad cnt. ",
			bcount, ctinfo->ct_blksz));
		/* if reading, and request size is multiple of actual block
		 * size on tape, allow it, even if not multiple of ct_blksz. */
		if(!(bp->b_flags & B_READ))
			goto badcnt; /*	must be no remainder.  */
		/* if at BOT, try to get actual blocksize first; unlikely
		 * to have done an ioctl at this point, particularly for
		 * standalone; if curblksz different, we must have
		 * already tryed to get/set it. */
		if((ctinfo->ct_state & CT_BOT) &&
			ctinfo->blkinfo.curblksz == ctinfo->ct_blksz) {
			ct_dbg((C, "chk actual size. "));
			(void)ctgetblklen(ctinfo);
		}
		else ct_dbg((C, "not at bot (%x), or blksz diff, no getblklen. ",
		ctinfo->ct_state));
		if(!ctinfo->blkinfo.curblksz ||
			(bcount % ctinfo->blkinfo.curblksz))
			goto badcnt; /*	must be no remainder.  */
		else ct_dbg((C, "but mult of curblk=%x and reading. ",
			ctinfo->blkinfo.curblksz));
	}

	/* If we are doing a read, we must check that the device has
	 * never done a write. All of our tapes fail an attempt to
	 * alternate reads and writes between filemarks.  intermixed
	 * reads and writes are allowed in audio mode to allow for editting.  */

	/* if determining block size, never use swap buffer.  Otherwise,
	 * if writing, byte swap data first.  Note that when writing, we
	 * must byte swap again at end, in case user is still using the
	 * data.  This is wasteful with tar, etc., but much simpler than
	 * the old way of allocating a buffer and doing the i/o in chunks.
	 * The in place swap is about as fast as the swbcopy that we used
	 * to do (slightly faster), but on writes we have to do two of them.
	 * for a 200Kb buffer, this means about an extra 80ms per write on
	 * an IP10, and slightly reduces the time when reading.  Slow devices
	 * like the QIC tapes still have no trouble streaming on writes.
	 * Also, we don't beat up the cache as much, and we use less code
	 * and time dealing with the swap buffer.  Finally, it fixes the
	 * case of programs (and standalone) where a small i/o is done
	 * first, followed by larger i/o's later; the old way was very
	 * slow under these conditions. */
	if(ISSWAP_DEV(bp->b_edev) && !(ctinfo->ct_state&CT_GETBLKLEN) &&
		!(bp->b_flags & B_READ)) {
		needswapbk = 1;
		swapbytes((u_char *)bp->b_dmaaddr, bcount);
		dki_dcache_wb(bp->b_dmaaddr, bcount);
	}

	/* if doing io with audio media, but not audio mode, clear audio
	 * media.  Conversely, if audio mode, but not audio media, 
	 * set it.  In both cases, put drive into correct mode.
	 * This way audio media bit always correctly reflects the
	 * state of the information on the tape.
	 * This has to be checked on reads as well, so we get the
	 * incompatible media message if in the 'wrong' mode.
	*/
	if((ctinfo->ct_state&(CT_AUDIO|CT_AUD_MED)) &&
	    (ctinfo->ct_state&(CT_AUDIO|CT_AUD_MED)) != (CT_AUDIO|CT_AUD_MED)) {
	    ct_xdbg(1,(C, "aud mode or media, but not both (%x); fix. ",
			ctinfo->ct_state));
		if(ctinfo->ct_state&CT_READ) {
			/* we can be sure we have already done the msense 
			 * by this point... */
			ctreport(ctinfo, "Incompatible %s when reading\n",
				(ctinfo->ct_state&CT_AUDIO) ? "tape mode" : "media");
			error = EINVAL;
			goto errdone;
		}
		if(ctinfo->ct_state & CT_AUDIO)
		    ctinfo->ct_state |= CT_AUD_MED;
		else
		    ctinfo->ct_state &= ~CT_AUD_MED;
		(void)ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0, 1);
		ct_xdbg(1,(C, "new state=%x. ", ctinfo->ct_state));
	}

reread:
	ostate = ctinfo->ct_state;	/* for skipfm check below */
	ctinfo->ct_req->sr_bp = bp;	/* for ctxfer */
	ctinfo->ct_lastreq = (bp->b_flags & B_READ) ? MTR_READ : MTR_WRITE;
	if(error = ctcmd(ctinfo, ctxfer, bcount, XFER_TIMEO(bcount), bp->b_flags&B_READ, 1))
	{
		if (IS_DST(ctinfo)) {
			/* Wrong block size? */
			ct_g0es_data_t  *esdptr = ctinfo->ct_es_data;
			if (esdptr->rsscsi2.as == 0x2a && esdptr->rsscsi2.aq == 0x01) {
				/* Yes - set correct block size and try again. */
				if (error=ctcmd(ctinfo, ctmodesens, MSD_IND_SIZE+MS_VEND_LEN, SHORT_TIMEO, 0x10,1))
					goto errdone;
				ctinfo->ct_blksz  = ctinfo->ct_ms.msd_blklen[0] << 16;
				ctinfo->ct_blksz |= ctinfo->ct_ms.msd_blklen[1] << 8;
				ctinfo->ct_blksz |= ctinfo->ct_ms.msd_blklen[2];
				ctinfo->ct_fixblksz = ctinfo->ct_blksz;
				goto reread; /* go back there */
			}

		}
		goto errdone;
	}

done:
	bp->b_resid += extraresid;	/* wasted if error, but most will
		succeed, so don't add an extra check */

	/* first read after open, and at FM means that we were at the BOT side
	 * of a FM, and the drive has skipped over it, so just retry the read.
	 * make sure that we only try to skip over one fm, in case there are
	 * sequential FM's, like on 9 track. */
	if(!skippedfm && bp->b_bcount == bp->b_resid &&
	    (ctinfo->ct_state & (CT_DIDIO | CT_READ | CT_FM | CT_BOT)) == (CT_READ | CT_FM)
		&& !(ostate & (CT_FM | CT_BOT))) {
	    ct_dbg((C, "FM, 1st read, no tape motion, so re-read (st=%x ost=%x. ",
			ctinfo->ct_state, ostate));
	    skippedfm = 1;
	    goto reread;
	}
	if(bp->b_bcount != bp->b_resid) {
	    ctinfo->ct_state |= CT_DIDIO;
    
	    if(bp->b_resid && !ISVAR_DEV(bp->b_edev) &&
		(ctinfo->ct_state & CT_FM)) {
		ct_dbg((C, "short read hit fm: sets HITFM. "));
		ctinfo->ct_state |= CT_HITFMSHORT;
	    }
	}

	if(ISSWAP_DEV(bp->b_edev) && !(ctinfo->ct_state&CT_GETBLKLEN)) {
		/* read AND write */
		unsigned tmp;
		if (bp->b_flags & B_READ) {
			if(bp->b_resid >= bcount)
				tmp = 0;
			else
				tmp = bcount - bp->b_resid;
		}
		else
			tmp = bcount;
		swapbytes((u_char *)bp->b_dmaaddr, tmp);
	}

	ct_cxdbg(bp->b_resid, 2, (C, "strat OK: resid=%x/%x xtra=%x. ", bp->b_resid, bcount, extraresid));
	iodone(bp);
	return;

badcnt:
	ct_dbg((C, "bad xfer sz %x.  ", bcount));
errdone:
	if(needswapbk)	/* if we swapped on writing, we have to swap back *
		* even on errors!  This has hosed multivolume bru... */
		swapbytes((u_char *)bp->b_dmaaddr, bcount);
	ct_dbg((C, "ERRDONE %d in ctstrat. ", error));
	bp->b_flags |= B_ERROR;
	bp->b_error = error ? error : EINVAL;
	iodone(bp);
}


/*
 * ctcmd
 *   Issue a command to a SCSI target.
 *   `func' is a pointer to a function which sets up the specific command
 *   and will parse any errors as a result. `timeo'  is the total length
 *   of time to completion, including delays due to BUSY status.
 *   This time may be lengthed by ct_extrabusy if the previous command
 *   had the immediate bit set (rewinds, unload, etc.).
 *   Needed a second arg for some functions, most don't use it.
 *   sr_bp is no longer used within this routine, so it only has to be
 *   set if the passed in func will used it.  ct_bp is used only for
 *   during resid count, if any.
 *   returns 0 on no errors, or errno code on errors.
 *   If CTCMD_ERROR is passed as call type to func, arg2 is set to the error.
 *
 *	 New as of late irix5.1.  While tpsc and other scsi driver opens on the
 *	 same target are not allowed, and multiple opens are not (in general)
 *	 allowed, we do now have a hack for mediad, where an additional major#
 *	 is allocated (created as the tps#d#stat device), that
 *	 does not do exclusive checking (or prevent others from opening).  In
 *	 order to prevent commands from stepping on each other, ctcmd is now
 *	 semaphored.  Recursive ctcmd calls must call ctcmd with needsema==0.
 *	 Fortunately there aren't many of those (one from ctcmd itself, and
 *	 a couple via ctchkerr and ctdrivesense from ctchkerr).  This also helps
 *	 defend against the case where a process opens the tape driver and
 *	 then forks, and accidentally or otherwise uses the tape fd in both
 *	 parent and child.
*/
static
ctcmd(ctinfo_t *ctinfo, void (*func)(), __psunsigned_t arg,
      int timeo, __psunsigned_t arg2, int needsema)
{
	int tries = 3;	/* up to 3 re-tries on unitatn, because
		some archive qic150 drives give us two in a row... */
	struct scsi_request *req = ctinfo->ct_req;
	ctsvinfo_t svinfo;
	ulong savstate;
	int extra, error;

	if (needsema) /* wait for any other cmds for this target to finish */
		mutex_lock(&ctinfo->ct_cmdsem, PRIBIO);

	savstate = ctinfo->ct_state;

	/* add interval for immediate mode busy.  Always allow at least
	 * 3 retries on busy; we used to only do this for specific commands.
	 * The exception is that if timeo is negative, we do not block on BUSY
	 * status.  This is currently needed only for the tstrdy from MTIOCGET;
	 * see the comments there for more info.  Always allow at least as
	 * much time for BUSY status as for the command itself to timeout,
	 * *without* reducing the timeout by the number of busy retries.
	 * I had broken this a while back... */
	if(timeo >= 0)
		extra = timeo + ctinfo->ct_extrabusy + 3*BUSY_INTERVAL;
	else {
		extra = 0;
		timeo = -timeo;
	}

	while(extra >= 0) {
		error = 0;
		/* always set to the original timeout val; inside loop
		 * in case we do a reqsense, etc. */
		req->sr_timeout = timeo;

		/* before setup routine called, so it can set direction */
		req->sr_flags = SRF_AEN_ACK | SRF_MAP;
		/* most do no data transfer, simplifies those routines */
		req->sr_buflen = 0;
		req->sr_buffer = NULL;

		/* Initialize here so we don't have to do in each function */
		bzero(&ctinfo->ct_cmd, sizeof(ctinfo->ct_cmd));
		ctinfo->ct_reqcnt = 1;				/* default */
		ctinfo->ct_complete = 0;

		req->sr_cmdlen = 0;

		(*(void (*)(ctinfo_t *, int, __psunsigned_t, __psunsigned_t))func)(ctinfo,
			    CTCMD_SETUP, arg, arg2); /* do command setup */

		if(ctinfo->ct_g0cdb.g0_opcode < ctinfo->ct_typ.tp_noissue_cmdlen &&
			ctinfo->ct_typ.tp_noissue_cmd[ctinfo->ct_g0cdb.g0_opcode]) {
			error = ENOTSUP;
			ct_dbg((C, "scsi cmd %x disabled. ", ctinfo->ct_g0cdb.g0_opcode));
			break;
		}

		/* 
		 * CDB length is set here, unless vendor unique in
		 * which case the individual routines are expected to
		 * have defined the length individually 
		 */
		switch((ctinfo->ct_lastcmd=ctinfo->ct_g0cdb.g0_opcode) & 0xE0) {
		case 0x00:		/* all group 0 command */
			req->sr_cmdlen = SC_CLASS0_SZ;
			break;
		case 0x20:		/* all group 1 and 2 commands */
		case 0x40:
			req->sr_cmdlen = SC_CLASS1_SZ;
			break;
		case 0x90:
			req->sr_cmdlen = SC_CLASS2_SZ;
		}
		ASSERT(req->sr_cmdlen == SC_CLASS0_SZ || 
		       req->sr_cmdlen == SC_CLASS1_SZ || 
		       req->sr_cmdlen == SC_CLASS2_SZ);

		ASSERT(!(req->sr_buffer == NULL && req->sr_buflen > 0));

		/* must have a valid state in ctmotion for every cmd issued. */
		ASSERT(sizeof(ctmotion) > ctinfo->ct_lastcmd);
		if(ctmotion[ctinfo->ct_lastcmd]) {
		    if(ctinfo->ct_state & CTPOS) {
			/* Can't do this only if ! MOTION, because ctloaded()
			 * sets BOT|MOTION on success, and we need to be
			 * sure we clear BOT on subsequent motion cmds */
			ctinfo->ct_state &= ~CTPOS;
			if(func != ctrewind || (ctinfo->ct_state & CT_BOT) != CT_BOT)
			    ctinfo->ct_state |= CT_MOTION; /* see bug 543118 */
		    }
		    if(ctinfo->ct_tent_state) {
			ct_xdbg(2,(C, "motion: clr tent_state (%x), st=%x. ",
				ctinfo->ct_tent_state, ctinfo->ct_state));
			/* need to clear the pending state if new command causes
			 * motion; otherwise can end up having 2 incompatible
			 * position bits set, when the last cmd was an immediate
			 * mode cmd */
			ctinfo->ct_tent_state = 0;
		    }
		}
		ctinfo->ct_state2 = 0;
		ct_xdbg(1,(C, "%s ", cmdnm(ctinfo->ct_lastcmd)));
		ct_cxdbg(func==ctspace, 2,(C, "(%s), cnt %d ",
		    ctspctyp[arg], arg2));
		ct_cxdbg(func!=ctspace, 2,(C, "%d %d ", arg, arg2));
		ct_xdbg(4,(C, "new st=%x. ", ctinfo->ct_state));

		SLI_COMMAND(scsi_lun_info_get(req->sr_lun_vhdl))(req);
		psema(&ctinfo->ct_sema, PRIBIO);

                /*
                   If we are trying to write an odd size request to
                   a wide SCSI device - the HBA will send to the
                   device one extra byte to make it even since by
                   definition it always delivers 2 bytes for each 
                   REQ/ACK to a wide device. A hope is that the device 
                   will ignore this extra byte.
                   On this end the HBA driver ends up returning
                   resid of -1. This case must be recognized and resid 
                   must be changed to 0, so the rest of the code is 
                   isoleted from this peculiar behavior.
		   If the tape is a narrow device or request size is 
                   an even number this(resid == -1) should never happen 
                   on a write.
                */

                if(
                    req->sr_resid == -1
                    &&  func == ctxfer
                    && !(arg2 & B_READ)
                  )
                {
                        /* It is an error to end up here if the device
                           is narrow or the request was of even size
                        */
                        if (!((int)arg & 0x1))
                           ctreport(ctinfo, "resid is -1 on an even size write\n");
                        else
                             if (
                                  !(((ct_g0inq_data_t*)\
                                   ((struct scsi_target_info*)\
                                   (ctinfo->ct_info))->si_inq)->id_wide16)
                                )
                                ctreport(ctinfo, "resid is -1 on a narrow device\n");
                              else
                                req->sr_resid = 0; /* make it 0! */
                }


		/* set resid for callers who care (before ctchkerr calls) */
		ctinfo->ct_bp->b_resid = req->sr_resid;
		ct_cxdbg(req->sr_resid, 2, (C, "sr_resid=%d. ", req->sr_resid));

		if(req->sr_status != SC_GOOD) {
			extern char *scsi_adaperrs_tab[];
			error = EIO;
			ct_dbg((C, "%s fails: stat=%x st=%x: ",
				cmdnm(ctinfo->ct_lastcmd),
				req->sr_scsi_status, ctinfo->ct_state));
			ct_cxdbg(req->sr_status<NUM_ADAP_ERRS,0,
				(C, "%s.  ", scsi_adaperrs_tab[req->sr_status]));
			ct_cxdbg(req->sr_status>=NUM_ADAP_ERRS,0,
				(C, "Unknown adapter err %x.  ", req->sr_status));
			(*(void (*)(ctinfo_t *, int, uint, int))func)(ctinfo,
				    CTCMD_ERROR, arg, error);
			break;
		}

		if (req->sr_scsi_status == ST_GOOD && req->sr_sensegotten == 0)
		{
			if(func == ctxfer && (savstate &
			  (CT_AUDIO|CT_AUD_MED|CT_BOT|CT_READ)) == CT_BOT) {
				/* This hack is for detecting write protected
				 * tapes (mostly qic drives) when writing with
				 * small block sizes.  Otherwise backup programs
				 * may think their first write was OK, so there
				 * must have been a media error when they get an
				 * error on their 2nd or nth write.  The wfm of
				 * 0 flushes the tape buffer to the tape, at
				 * which point we detect the qic24...
				 */
				ct_dbg((C, "flush buf on 1st write from BOT (%x sav=%x) ",
					ctinfo->ct_state, savstate));
				ctsave(ctinfo,&svinfo);
				error=ctcmd(ctinfo, ctwfm, 0, SHORT_TIMEO, 0,0);
				ctrestore(ctinfo,&svinfo);
				if ( error ) {
					(*(void (*)(ctinfo_t *, int, uint,
					 int))func)(ctinfo, CTCMD_ERROR,
						     arg, error);
					break;
				}
			}
			if(ctinfo->ct_tent_state && func != ctreadposn &&
			    func != ctmodesens) {
			    /* before NO_ERROR, since that is where tent_state
			     * is set, and allows success to clear states that
			     * might be in tent_state.  Never restore on 
			     * readposition or modesense cmds, since they works
			     * while rewind, etc are still in progress.  */
			    ctinfo->ct_state |= ctinfo->ct_tent_state;
			    ct_dbg((C, "tent=%x, new st=%x after OK. ",
				    ctinfo->ct_tent_state, ctinfo->ct_state));
			    ctinfo->ct_tent_state = 0;	/* no pending state */
			}
			ctinfo->ct_complete = ctinfo->ct_reqcnt;
			(*(void (*)(ctinfo_t *, int, uint, uint))func)(ctinfo,
						CTCMD_NO_ERROR, arg, arg2);
			break;
		}
		else
		{
			/* If device is busy, and retries are indicated, we
			 * do the retries until they expire. If a check
			 * condition occurred we call ctchkerr to get the
			 * sense info, and get the error, etc.  Clear xtrabusy
			 * here, as it is clear we are doing a new cmd now;
			 * if a second immediate mode cmd is being done, we
			 * will reset extrabusy when we go back through the
			 * loop.  This means if no other cmd gets issued and
			 * gets a busy, extrabusy remains set, but that is
			 * not really a problem, as they device shouldn't
			 * return BUSY for anything else, except maybe a BUS
			 * reset. */
			if(req->sr_scsi_status == ST_BUSY) {
busy:
				ctinfo->ct_extrabusy = 0;
				if(extra) {
				    ct_dbg((C, "BUSY:%d ",extra/HZ));
				    delay(BUSY_INTERVAL);
				    extra -= BUSY_INTERVAL;
				    continue;
				}
				ct_dbg((C, "BUSY, giving up. "));
				error = EBUSY;
				/* and fall through to CTCMD_ERROR below */
			}
			else if(req->sr_sensegotten > 0) {
				error = ctchkerr(ctinfo, req->sr_sensegotten);
				/*
				 * When the Exabyte gets a Command Aborted sense
				 * key on a Read, then the command can be retried,
				 * because the Exabyte repositions itself back to
				 * where it was at the start of the command.
				 */
				if (error == -SC_CMD_ABORT) {
					if (IS_EXABYTE(ctinfo) &&
					    func == ctxfer &&
					    arg2 != 0)
					{
						if (--tries > 0) {
							cmn_err(CE_CONT, "retrying\n");
							continue;
						}
					}
				    /* if Fujitsu returns ABORTED COMMAND
				     * Sense Key, re-try the command (see
				     * comment preceding fuj_drrp() for
				     * scenario that requires this code)
				     */
					if (IS_FUJ3480(ctinfo))
					{
						if (--tries > 0) {
							continue;
						}
					}
					cmn_err(CE_CONT, "fatal\n");
					ct_dbg((C, "Aborted command.  "));
					error = EIO;
				}
				else if(error == -SC_UNIT_ATTN ||
				    error == -SC_ERR_RECOVERED) {
				    if(--tries > 0) {
					    /* retry the cmd */
					    continue;
				    }
				    ct_dbg((C, "Too many retries.  "));
				    error = EIO;
				}
				else if (error == -SC_NOT_READY) {
					goto busy;
				}
				else if(error == 0) {
				    /* some cmds are ok on a chk! */
				    (*(void (*)(ctinfo_t *, int, uint, uint))
						func)(ctinfo, CTCMD_NO_ERROR,
						arg, arg2);
				    break;
				}
			}
			else {
				/*
				 * Statuses other than busy just fail, (and busy fails
				 * when the max time expires).
				 */
				error = EIO;
				ct_dbg((C, "%s fails, stat %x.  ",
				       cmdnm(ctinfo->ct_lastcmd),
				       req->sr_scsi_status));
			}
			(*(void (*)(ctinfo_t *, int, uint, int))func)(ctinfo,
					CTCMD_ERROR, arg, error);
			break;
		}
	}
	if (needsema) /* allow waiting cmds for this target to run */
		mutex_unlock(&ctinfo->ct_cmdsem);
	return error;
}


/* Set the mode select parameters.  The arg is the number of bytes of vendor
 * specific data we send (usually from ctinfo->ct_typ.tp_msl_pglen) */
static void
ctmodesel(ctinfo_t *ctinfo, int flag, __psunsigned_t arg)
{
	ct_g0msl_data_t	*ptr;
	__psunsigned_t oarg = arg;

	if(flag != CTCMD_SETUP)
		return;

	ptr = &ctinfo->ct_msl;
	/* by zeroing the independent portion, we set default density, etc.
	 * Bytes past arg in vendor specific area are left alone; and often are
	 * set from a preceding mode sense.  */
	bzero(ptr, MSD_IND_SIZE);

	/* set data always required, unless data being supplied by caller */
	if(!arg && ctinfo->ct_typ.tp_msl_pglen) {
		bcopy(ctinfo->ct_typ.tp_msl_data, ptr->msld_vend,
			ctinfo->ct_typ.tp_msl_pglen);
		arg = ctinfo->ct_typ.tp_msl_pglen;
	}

	/* change size, set params, etc, as required.  DAT fills in the
	 * msld_vend for some actions before calling us, but we currently
	 * don't do that for any other drives.  */
	if(IS_CIPHER(ctinfo))
		ptr->msld_vend[0] = ctinfo->ct_cipherrec;

	/* Note that if/when we support different densities (compression
	  maybe?) on DAT, that this code may have to be modified to work
	  with the audio stuff below */
	if(ctinfo->ct_typ.tp_capablity & MTCAN_SETDEN)
		ptr->msld_descode = ctinfo->ct_density;

	if(IS_DATTAPE(ctinfo)) {
		if(ctinfo->ct_state & CT_AUD_MED) {
			/* force to audio mode, only if already aud media set.;
			 * that is, modesense said so, or we are in audio mode,
			 * and have written to tape. */
			ptr->msld_descode = 0x80;
			ct_dbg((C, "set audmed on msel. "));
		}
		else if(!oarg && !(ctinfo->ct_state & CT_AUDIO) && 
			(ctinfo->ct_typ.tp_capablity & MTCAN_COMPRESS)) {
			/* yes, it really is page 0xf, and this should somehow
			 * come out of the tpsctype array, for all types */
			arg = 0x10;
			bzero(ptr->msld_vend, arg);

			ct_dbg((C, "do compression page %s. ", (ctinfo->ct_state & CT_COMPRESS) ? "on":"off"));

			ptr->msld_vend[0] = 0xf;
			ptr->msld_vend[1] = 0xe;
			/* enable compr on write? (0xc0 in manual is wrong!) */
			ptr->msld_vend[2] = 
				(ctinfo->ct_state & CT_COMPRESS) ? 0x80 : 0;
			ptr->msld_vend[3] = 0x80;	/* enable decomp on read, report
				* illegal to legal transitions and vice versa; c0
				* is what I want, but the drive won't accept it. */
			ptr->msld_vend[7] = 0x20; /* this should not be necessary, and 0
				* (use default) would be far more robust, in case algorithim
				* changes, but the Connor DDS2 drive won't do compression
				* unless this is set... */
		}

		ct_cxdbg(ptr->msld_descode==0x80, 3,(C, "force aud_med. "));
	}
	else if (IS_SONYAIT(ctinfo)) {
		if(!oarg && (ctinfo->ct_typ.tp_capablity & MTCAN_COMPRESS)) {

			arg = 0x10;
			bzero(ptr->msld_vend, arg);

			ptr->msld_descode = 0x30;	/* AIT format tape */

			ct_dbg((C, "do AIT compression page %s. ", 
				(ctinfo->ct_state & CT_COMPRESS) ? "on":"off"));

			ptr->msld_vend[0] = 0xf;	/* yes, page 0xf */
			ptr->msld_vend[1] = 0xe;	/* page len */
			ptr->msld_vend[2] = (ctinfo->ct_state & CT_COMPRESS) ? 0xc0 : 0x40;
			ptr->msld_vend[3] = 0xc0;	/* enable decomp on read, report
							 * illegal to legal transitions 
							 * and vice versa */
			ptr->msld_vend[7] = 0x03;	/* compression algorithm */
		}
	}

	if(ctinfo->ct_typ.tp_capablity & MTCAN_SETSP)
		ptr->msld_speed = ctinfo->ct_speed ? 2 : 1;

	/* DLT uses dev config page to turn on compression */
	if( IS_DLT(ctinfo) ) {
		ASSERT(!arg && !ctinfo->ct_typ.tp_msl_pglen);
		arg = 0x10;
		bzero(ptr->msld_vend, arg);
		ptr->msld_vend[0] = 0x10;	/* dev config page */
		ptr->msld_vend[1] = 0xe;	/* page size */
		ptr->msld_vend[7] = 0x32;	/* write delay time */
		ptr->msld_vend[8] = 0x40;	/* BIS */
		ptr->msld_vend[10] = 0x10;	/* EEG */
		if( ctinfo->ct_state & CT_COMPRESS )
			ptr->msld_vend[14] = 1;
	}

	/*
	 * On/off the compression bit only if this ctmodesel call is
	 * not for a specific page.
	 * NTP comes up in default compression mode - turn it on or
	 * off according to device number.
	 */
        if (IS_MGSTR(ctinfo) && (arg == 0)) {
		ASSERT(!arg && !ctinfo->ct_typ.tp_msl_pglen);
		arg = 0x10;
		bzero(ptr->msld_vend, arg);
		ptr->msld_vend[0] = 0x10;       /* dev config page */
		ptr->msld_vend[1] = 0xe;        /* page size */
		ptr->msld_vend[7] = 0x14;       /* write delay time */
		ptr->msld_vend[8] = 0xc0;       /* BIS, DBR */
		ptr->msld_vend[10] = 0x18;      /* EEG, SEW */
		if (ctinfo->ct_state & CT_COMPRESS)
			ptr->msld_vend[14] = 1;
		else
			ptr->msld_vend[14] = 0;
	}

        if ((IS_STK90(ctinfo) || IS_STK9840(ctinfo)) && (arg == 0)) {
                ASSERT(!arg && !ctinfo->ct_typ.tp_msl_pglen);
                arg = 0x10;
                bzero(ptr->msld_vend, arg);
                ptr->msld_vend[0] = 0x10;       /* dev config page */
                ptr->msld_vend[1] = 0xe;        /* page size */
                ptr->msld_vend[7] = 0x28;       /* write delay time */
                ptr->msld_vend[8] = 0xc2;       /* BIS, DBR, RBO */
                ptr->msld_vend[10] = 0x18;      /* EEG, SEW */
                if (ctinfo->ct_state & CT_COMPRESS)
                        ptr->msld_vend[14] = 1;
                else
                        ptr->msld_vend[14] = 0;
        }

        if (IS_STKSD(ctinfo) && (arg == 0)) {
                ASSERT(!arg && !ctinfo->ct_typ.tp_msl_pglen);
                arg = 0x10;
                bzero(ptr->msld_vend, arg);
                ptr->msld_vend[0] = 0x10;       /* dev config page */
                ptr->msld_vend[1] = 0xe;        /* page size */
                ptr->msld_vend[7] = 0x0;       /* write delay time */
                ptr->msld_vend[8] = 0xc2;       /* BIS, DBR, RBO */
                ptr->msld_vend[10] = 0x18;      /* EEG, SEW */
                if (ctinfo->ct_state & CT_COMPRESS)
                        ptr->msld_vend[14] = 1;
                else
                        ptr->msld_vend[14] = 0;
        }

        if (IS_EXA8900(ctinfo) && (arg == 0)) {
                ASSERT(!arg && !ctinfo->ct_typ.tp_msl_pglen);
		arg = 0x10;
                bzero(ptr->msld_vend, arg);
                ptr->msld_vend[0] = 0x0f;
               	ptr->msld_vend[1] = 0xe;
		ptr->msld_vend[2] = 
			(ctinfo->ct_state & CT_COMPRESS) ? 0x80 : 0;
                ptr->msld_vend[3] = 0x80;
                ptr->msld_vend[7] = 0x10;
                ptr->msld_vend[11] = 0x10;
        }

        if ((IS_TD3600(ctinfo) || IS_NCTP(ctinfo)) && (arg == 0)) {
                ASSERT(!arg && !ctinfo->ct_typ.tp_msl_pglen);
		arg = 0x10;
                bzero(ptr->msld_vend, arg);
                ptr->msld_vend[0] = 0x10;       /* dev config page */
                ptr->msld_vend[1] = 0xe;        /* page size */
                ptr->msld_vend[6] = 0x01;       /* write delay time MSB */
                ptr->msld_vend[7] = 0x2c;       /* write delay time LSB */
                ptr->msld_vend[8] = 0xc6;   /* BIS, DBR */
                ptr->msld_vend[10] = 0x18;      /* EEG, SEW */
                if (ctinfo->ct_state & CT_COMPRESS)
                        ptr->msld_vend[14] = 1;
                else
                        ptr->msld_vend[14] = 0;
        }

        if (IS_OVL490E(ctinfo) && (arg == 0)) {
                ASSERT(!arg && !ctinfo->ct_typ.tp_msl_pglen);
		arg = 0x10;
		bzero(ptr->msld_vend, arg);
                ptr->msld_vend[0] = 0x10;       /* dev config page */
                ptr->msld_vend[1] = 0xe;        /* page size */
                if (ctinfo->ct_state & CT_COMPRESS)
                        ptr->msld_vend[14] = 1;
                else
                        ptr->msld_vend[14] = 0;
	}

	if(IS_STK4781(ctinfo) && (arg == 0)) {
		if (ctinfo->ct_state & CT_COMPRESS)
			ptr->msld_vend[0] = 1;
		else
			ptr->msld_vend[0] = 0;
		/*
		 * Compress if ICRC supported.
		 */
		arg = ctinfo->ct_info->si_inq[36] & 2 ? 1 : 0;
	}

	if(IS_STK4791(ctinfo) && (arg == 0)) {
		if (ctinfo->ct_state & CT_COMPRESS)
			ptr->msld_vend[0] = 1;
		else
			ptr->msld_vend[0] = 2;
		arg = 1;
	}

        if (IS_GY(ctinfo) && (arg == 0) 
	    && (ctinfo->ct_typ.tp_capablity & MTCAN_COMPRESS)) {
                ASSERT(!arg && !ctinfo->ct_typ.tp_msl_pglen);
		arg = 0x10;
                bzero(ptr->msld_vend, arg);
                ptr->msld_vend[0] = 0x0f;
               	ptr->msld_vend[1] = 0xe;
		ptr->msld_vend[2] = (ctinfo->ct_state & CT_COMPRESS) ? 0x80 : 0;
                ptr->msld_vend[3] = 0x80;
                ptr->msld_vend[7] = 0x03;
                ptr->msld_vend[11] = 0x03;
        }

    /* compression is an option on the Fujitsu Diana-1, but is standard
       on the Diana-2 and -3.  If compression is not installed on a drive
       but is attempted to be enabled here, the Mode Select will fail.
       However, as ctsetup() ignores such errors, I/O operations for the
       compressed device when compression is not installed will run
       successfully (albeit without compression). */
	if (IS_FUJD1(ctinfo) && (arg == 0))
	{
		ASSERT(!arg && !ctinfo->ct_typ.tp_msl_pglen);
		arg = 0x10;
		bzero(ptr->msld_vend, arg);
		ptr->msld_vend[0] = 0x10;	/* dev config page */
		ptr->msld_vend[1] = 0xe;	/* page size */
		ptr->msld_vend[14] =
			(ctinfo->ct_state & CT_COMPRESS) ? 0x84 : 0;
	}

	if(ctinfo->ct_scsiv > 1)
		ctinfo->ct_g0cdb.g0_cmd0 = 0x10;	/* SCSI 2 PF bit */

	ctinfo->ct_g0cdb.g0_opcode = MODE_SEL_CMD;
	ctinfo->ct_req->sr_buflen = ctinfo->ct_g0cdb.g0_cmd3 =
		arg + MSD_IND_SIZE;

	ctinfo->ct_req->sr_buffer = (u_char *)ptr;

	ptr->msld_bfm = 1;		/* buffered mode */
	ptr->msld_bdlen = MSLD_BDLEN;

	/* if getting blklen, force variable mode */
	if(!(ctinfo->ct_state&CT_GETBLKLEN) &&
	   !ISVAR_DEV(ctinfo->ct_bp->b_edev)) 
	{
		if (IS_DST(ctinfo) && !ctinfo->ct_blksz)
			ctinfo->ct_blksz = ctinfo->ct_fixblksz;
		ptr->msld_blklen[2] = (u_char) ctinfo->ct_blksz;
		ptr->msld_blklen[1] = (u_char)(ctinfo->ct_blksz >> 8);
		ptr->msld_blklen[0] = (u_char)(ctinfo->ct_blksz >> 16);
	}
	/* else 0 indicates variable size blocks */


	/* 
	 * Don't supress reporting of ILI on overlength condition in
	 * variable mode by setting the block length to a "non-zero
	 * value" -- see SCSI-2 spec., section 10.2.4. Setting the
	 * block length to zero (the default) will suppress all ILI
	 * conditions when ILI is enabled.
	 */
	if (ISVAR_DEV(ctinfo->ct_bp->b_edev) && 
	    (ctinfo->ct_sili) && 
	    (ctinfo->ct_ilimode))
		ptr->msld_blklen[0] = 1;

#ifdef SCSICT_DEBUG
	if(ctdebug>3) {
		int i;
		unchar *d = (unchar *)ptr;
		printf ("msel opcode: %x, ",ctinfo->ct_g0cdb.g0_opcode);
		printf ("msel pll: %x, ",ctinfo->ct_g0cdb.g0_cmd3);
		printf("msel hdr: ");
		for(i=0; i<4;i++)
			printf("%x ", d[i]);
		printf(" blk descr: ");
		for(; i<MSD_IND_SIZE;i++)
			printf("%x ", d[i]);
		if(arg) {
			printf(" page data: ");
			for(i=0; i<arg;i++)
				printf("%x ", ptr->msld_vend[i]);
			printf("\n");
		}
	}
#endif /* SCSICT_DEBUG */

	dki_dcache_wb(ctinfo->ct_req->sr_buffer, ctinfo->ct_req->sr_buflen);
}

/* Test to see if the device is ready to accept media access commands.  */
/*ARGSUSED2*/
static void
cttstrdy(ctinfo_t *ctinfo, int flag, __psunsigned_t arg, __psunsigned_t arg2)
{
	if(flag == CTCMD_SETUP)
		ctinfo->ct_g0cdb.g0_opcode = TST_UNIT_RDY_CMD;
	else if(flag == CTCMD_NO_ERROR) {
		if(ctinfo->ct_typ.tp_capablity&MTCAN_CHKRDY)
			ctinfo->ct_state |= CT_ONL; /* ready for other cmds */
	}
	else if(arg2 != EBUSY)
		/* in case the error wasn't one which clears ONL; EBUSY
		 * means we must be in the middle of a seek or rewind
		 * with immediate bit set, so don't clear ONL */
		ctinfo->ct_state &= ~CT_ONL;
}


/*
 *   Get the mode sense info from the device.
 *   Set/clear state bits as appropriate, including WRP, QIC24, and QIC120.
*/
static void
ctmodesens(ctinfo_t *ctinfo, int flag, __psunsigned_t arg, __psunsigned_t arg2)
{
	ct_g0ms_data_t	*ptr;

	ptr = &ctinfo->ct_ms;

	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = MODE_SENS_CMD;
		ctinfo->ct_g0cdb.g0_cmd3 = arg;
		ctinfo->ct_g0cdb.g0_cmd1 = arg2; /* which page */

		ctinfo->ct_req->sr_buffer = (u_char *)ptr;
		ctinfo->ct_req->sr_flags |= SRF_DIR_IN;
		ctinfo->ct_req->sr_buflen = arg;

		dki_dcache_inval(ptr, arg);
		return;
	}

	if(flag == CTCMD_ERROR)
		return;

	if(ptr->msd_wrp) {
		ct_dbg((C, "modesens: wrp set.  "));
		ctinfo->ct_state |= CT_WRP;
	}
	else
		ctinfo->ct_state &= ~CT_WRP;
	if(IS_VIPER150(ctinfo) || IS_TANDBERG(ctinfo)) {
		/* note we check density only for the viper150; QIC24 'always'
		* the case for older drives, since we don't support QIC11, and
		* only the 150 cares.
		* Density has following meaning on VIPER 150:
		*	0x10 - QIC150(18 tracks); 0xf - QIC120 (15 tracks);
		* 	5 - QIC24 (9 tracks); a QIC24 tape is readonly
		* 	0 - if set idrive should adapt to type of tape present;
		* 	otherwise attempt indicated format only.
		*
		* Some other drives:
		* 	Cipher 540s: 5 - QIC24 9trk; 4 - QIC11 4trk;
		*	0x84 - QIC11 9track.
		* 	Cipher 150S: 0,5 QIC24 9 track; 0xf - QIC120 15 track;
		* 		0x10 - QIC150 18 track (not qualified yet).
		*
		* Unfortunately, the density code is set ONLY after a successful
		* read or space cmd.  A firmware change has been requested,
		* but we still have to live with the older drives...
		*
		* Worse yet, the density code refers to the TAPE format,
		* NOT the cartridge format.  This means that if someone
		* reads a high density tape that was written in QIC24
		* format, we refuse to let them write it unless they
		* first remove and then re-insert the tape. BAD!  So we
		* have to clear the QIC24 bits in open after this sense
		* is done, if open for writing...  */
		if(ptr->msd_descode) {
			/* if not default, turn off all density info first, in
			 * case opposite set, or now QIC150 and either set. */
			ctinfo->ct_state &= ~(CT_QIC120|CT_QIC24);
			if(ptr->msd_descode == 5)
				ctinfo->ct_state |= CT_QIC24;
			else if(ptr->msd_descode == 0xf)
				ctinfo->ct_state |= CT_QIC120;
		}
	}
	if(IS_DATTAPE(ctinfo) && ptr->msd_descode == 0x80) {
			ctinfo->ct_state |= CT_AUD_MED;
			ct_cxdbg(!(ctinfo->ct_state&CT_AUD_MED), 1,
				(C, "set aud_med on sense. "));
	}
	else
		ctinfo->ct_state &= ~CT_AUD_MED;

#ifdef SCSICT_DEBUG
	if(ctdebug>3) {
		int i; unchar *d = (unchar *)ptr;
		i = (arg < MSD_IND_SIZE) ? arg : MSD_IND_SIZE;
		printf("msen hdr: ");
		while(i-->0)
			printf("%x ", *d++);
		if(arg>MSD_IND_SIZE && (ptr->msd_len+1)>MSD_IND_SIZE) {
			printf("; data: ");
			for(i=0; i< (ptr->msd_len+1-MSD_IND_SIZE);i++)
				printf("%x ", ptr->msd_vend[i]);
		}
		printf("\n");
	}
#endif /* SCSICT_DEBUG */
}


/* 
 * Rewind the tape. If arg is 0, it indicates an immediate mode
 * rewind; else it indicates a non immediate rewind.
 */
static void
ctrewind(ctinfo_t *ctinfo, int flag, __psunsigned_t arg)
{
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = REWIND_CMD;
		if((ctinfo->ct_state & CT_AUDIO) || 
			(tpsc_immediate_rewind && (arg == 0) &&
			!(ctinfo->ct_typ.tp_capablity&MTCANT_IMM))) {
			/* return successful status immediately, so readposn,
			 * etc can be done to monitor position during the
			 * operation.  */
			ctinfo->ct_g0cdb.g0_cmd0 = 1;
			ctinfo->ct_extrabusy = ctinfo->ct_req->sr_timeout;
		}
	}
	else if(flag == CTCMD_NO_ERROR) {
		/* if tape is rewound OK, forget about reads and
		 * writes; this also prevents us from writing a FM
		 * at BOT if programs rewinds prior to closing;
		 * unfortunately, in this case the tape won't have
		 * a FM at the end of the data just written, it's
		 * assumed the program knows what it's doing...
		 * Use ct_tent_state whether immediate rewind or not,
		 * so after it completes, the next command then sets BOT, etc.
		 * can't call ctloaded from this routine, because it leads to
		 * recursive ctcmd() calls.
		*/
		ctinfo->ct_state &= ~(BACKSTATES|CT_READ|CT_WRITE);
		ctinfo->ct_tent_state = CT_BOT|CT_ONL;
		ctinfo->ct_cblknum = 0;
	}
}

/*
 *  Position the tape using the space command.  Note that not all
 *	drives support all ops; they should probably be checked before
 *  this routine is called.
 *  If successful, and not a space blk cmd, forget reads or writes.
*/
static void
ctspace(ctinfo_t *ctinfo, int flag, __psunsigned_t arg, __psunsigned_t cnt)
{
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = SPACE_CMD;
		ctinfo->ct_g0cdb.g0_cmd0 = arg;	/* sub-function */
		ctinfo->ct_g0cdb.g0_cmd1 = HI8(cnt);
		ctinfo->ct_g0cdb.g0_cmd2 = MD8(cnt);
		ctinfo->ct_g0cdb.g0_cmd3 = LO8(cnt);
		/* clear these when we start, since we don't know
		 * where we will be on an error, unless perhaps
		 * the request sense tells us.  Even on failure,
		 * we should allow reads if we were writing, and
		 * vice-versa (subject to drive constraints later)
		 * since they have attempted the required action. */
		ctinfo->ct_state &= ~(CT_DIDIO|CT_READ|CT_WRITE);
		ctinfo->ct_reqcnt = (int)cnt;
	}
	else if(flag == CTCMD_ERROR) {
		ct_dbg((C, "space fails.  "));
		ctinfo->ct_cblknum += ctinfo->ct_complete;
	}
	else {
		if(arg == SPACE_FM_CODE) {
			if((int)cnt < 0) /* so that a read after an
				* mt bsf will skip to the EOT side of a FM in
				* ctloaded(), or MTFSF */
				ctinfo->ct_state |= CT_DIDIO;
			ctinfo->ct_state |= CT_FM;
			ctinfo->ct_cblknum += ctinfo->ct_complete;
		}
		else if(arg == SPACE_EOM_CODE)
			ctinfo->ct_state |= CT_EOD;
		else if(arg == SPACE_SETM_CODE)
			ctinfo->ct_state |= CT_SMK;
		else if ( arg == SPACE_BLKS_CODE ) {
			ctinfo->ct_state   |= CT_DIDIO;
			ctinfo->ct_cblknum += ctinfo->ct_complete;
			if ( ctinfo->ct_state & CT_FM ) {
				if ( (int)cnt < 0 )
					ctinfo->ct_cblknum--;
				else
					ctinfo->ct_cblknum++;
			}
		} else
			ctinfo->ct_state |= CT_DIDIO;	/* explict tape motion OK */
	}
}

/*
 *   Performs the cdb setup and low level error checking for reads and
 *   writes.
*/
static void
ctxfer(ctinfo_t *ctinfo, int flag, __psunsigned_t arg, __psunsigned_t dir)
{
	buf_t *bp = (buf_t *)ctinfo->ct_req->sr_bp;

	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = dir ? READ_CMD : WRITE_CMD;
		if(dir)
			ctinfo->ct_req->sr_flags |= SRF_DIR_IN;
		ctinfo->ct_req->sr_buffer = (u_char *)bp->b_dmaaddr;
		ctinfo->ct_req->sr_buflen = arg;

		if(!(ctinfo->ct_state & CT_GETBLKLEN)) {
		    if(ctinfo->ct_sili && dir) /* suppress ILI on read */
			    ctinfo->ct_g0cdb.g0_cmd0 |= 2;
		    if(!ISVAR_DEV(bp->b_edev)) {
			/* for resid cnt chks; curblksz set back to 0 on errs */
			if(!dir)
				ctinfo->blkinfo.curblksz = ctinfo->ct_blksz;
			arg /= ctinfo->ct_blksz;
			ctinfo->ct_g0cdb.g0_cmd0 = 1;
			ct_xdbg(2,(C, "fixed blksz=%d/%d blks. ",
				ctinfo->ct_blksz, arg));
		    }
		}
		/* else determining blk len, using variable mode, and
		 * want ILI's */

		ctinfo->ct_g0cdb.g0_cmd1 = HI8(arg);
		ctinfo->ct_g0cdb.g0_cmd2 = MD8(arg);
		ctinfo->ct_g0cdb.g0_cmd3 = LO8(arg);
		ctinfo->ct_reqcnt = (int)arg;
	}
	else if(flag == CTCMD_NO_ERROR) {
		/* ACTUAL bytes done, not requested size! */
		bp->b_resid = ctinfo->ct_bp->b_resid;
		if(ISVAR_DEV(bp->b_edev))
			ctinfo->blkinfo.curblksz = arg - bp->b_resid;
		else	/* necessary to do it here to get it right */
			ctinfo->blkinfo.curblksz = ctinfo->ct_blksz;
		if(dir)
			ctinfo->ct_readcnt++;
		else
			ctinfo->ct_writecnt++;
	}
	else	/* error; need to figure blksz on on next ioctl request */
		ctinfo->blkinfo.curblksz = 0;

	/*
	 *   Update the block number.
	 */
	if ( flag != CTCMD_SETUP ) {
		if ( ISVAR_DEV(bp->b_edev) ) {
			if ( ctinfo->ct_complete )
				ctinfo->ct_cblknum++;
		} else {
			ctinfo->ct_cblknum += ctinfo->ct_complete; 
		}
		if ( dir && ((ctinfo->ct_state & CT_FM) ||
			     (ctinfo->ct_state2 & CT_BLKLEN)) ) {
			ctinfo->ct_cblknum++;
		}
	}
}


/* erase tape; note that some drives (like Exabyte)
	take a LONG time to erase
*/
static void
cterase(ctinfo_t *ctinfo, int flag, uint arg)
{
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = ERASE_CMD;
		ctinfo->ct_g0cdb.g0_cmd0 = arg & 1;
		if(tpsc_immediate_rewind && ctinfo->ct_scsiv >= 2
			&& !(ctinfo->ct_typ.tp_capablity&MTCANT_IMM)) {
		    /* SCSI 2 specs immediate mode erases also */
		    ctinfo->ct_g0cdb.g0_cmd0 |= 2;
		    ctinfo->ct_extrabusy = ctinfo->ct_req->sr_timeout;
		}
	}
}


/*	Seek to a particular block.  Supported by many but not all drives.
	On DAT tape in audio mode, the 'block' is actually the program #.
	When arg is set, we are changing partition for DAT or DST, so force 
	locate even if someone somehow set the drive to be in SCSI 1 mode.
*/
static void
ctseekblk(ctinfo_t *ctinfo, int flag, __psunsigned_t chgpart, 
	__psunsigned_t seektyp)
{
	unsigned blkno;

	if ( flag == CTCMD_NO_ERROR ) {
		if ( chgpart )
			ctinfo->ct_cblknum = 0;
		return;
	} else if ( flag != CTCMD_SETUP )
		return;

	blkno = (uint)ctinfo->ct_bp->b_blkno;

	if (ctinfo->ct_state&CT_AUDIO) {
	     /* if audio, return successful status immediately, so
	     * readposn, etc can be done to monitor position during
	     * the operation; also if immediate set for non-audio.
	     * we don't know just where we will wind up so no tent_state */
	    ctinfo->ct_g0cdb.g0_cmd0 = 1;
	    ctinfo->ct_extrabusy = ctinfo->ct_req->sr_timeout;
	}

	/* if doing partitions or is scsi 2, use locate, it is a standard. */
	/* some drives can fastseek even though scsi1, come through here. */
	if(chgpart || ctinfo->ct_scsiv >= 2 || (ctinfo->ct_typ.tp_capablity&MTCAN_FASTSEEK)) {
	    ctinfo->ct_g2cdb.g2_opcode = LOCATEBLK_CMD;
	    if(chgpart) {
		    /* partition change; block always 0; pnum in blkno */
		    ctinfo->ct_g2cdb.g2_cmd0 |= 2;	/* CP bit */
		    /* DST needs to locate to block 1 (not 0) for BOP */
		    if (IS_DST(ctinfo))
			    ctinfo->ct_g2cdb.g2_cmd5 = 1;
		    ctinfo->ct_g2cdb.g2_cmd7 = blkno;
	    }
	    else {
		if(ctinfo->ct_state&CT_AUDIO) {
		    /* seek to program number, or time.
		     * set BT to indicate vendor specific block format */
		    ctinfo->ct_g2cdb.g2_cmd0 |= 4;
		    ctinfo->ct_g2cdb.g2_cmd2 = seektyp << 5;
		    if(seektyp == MTAUDPOSN_PROG)
			bcopy(ctinfo->ct_posninfo,
			    &ctinfo->ct_g2cdb.g2_cmd3, 3);
		    else /* time based */
			bcopy(ctinfo->ct_posninfo,
			    &ctinfo->ct_g2cdb.g2_cmd3, 4);
		}
		else {
		    if (ctinfo->ct_typ.tp_capablity&MTCAN_FASTSEEK) 
			ctinfo->ct_g2cdb.g2_cmd0 |= 4;  /* set BT bit */
		    ctinfo->ct_g2cdb.g2_cmd2 = (unchar)(blkno>>24);
		    ctinfo->ct_g2cdb.g2_cmd3 = (unchar)(blkno>>16);
		    ctinfo->ct_g2cdb.g2_cmd4 = (unchar)(blkno>>8);
		    ctinfo->ct_g2cdb.g2_cmd5 = (unchar)blkno;
		}
	    }
	}
	else {
		ctinfo->ct_g0cdb.g0_opcode = SEEKBLK_CMD;
		ctinfo->ct_g0cdb.g0_cmd1 = (unchar)(blkno>>16);
		ctinfo->ct_g0cdb.g0_cmd2 = (unchar)(blkno>>8);
		ctinfo->ct_g0cdb.g0_cmd3 = (unchar)blkno;
	}
}



/* Write File Marks */
static void
ctwfm(ctinfo_t *ctinfo, int flag, __psunsigned_t arg, __psunsigned_t arg2)
{
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = WFM_CMD;
		/* if arg2 is 2, then set marks, not file marks */
		ctinfo->ct_g0cdb.g0_cmd0 = arg2;
		if (ctinfo->ct_buffmmode && (ctinfo->ct_typ.tp_capablity & MTCAN_BUFFM) && arg)
			ctinfo->ct_g0cdb.g0_cmd0 |= 1;
		ctinfo->ct_g0cdb.g0_cmd1 = HI8(arg);
		ctinfo->ct_g0cdb.g0_cmd2 = MD8(arg);
		ctinfo->ct_g0cdb.g0_cmd3 = LO8(arg);
		ctinfo->ct_reqcnt = (int)arg;
	}
	else if(flag == CTCMD_NO_ERROR && arg) {
		/* do not change any state bits if it's just a flush! */
		ctinfo->ct_state &= ~(CT_DIDIO|CT_READ|CT_WRITE);
		ctinfo->ct_state |= arg2==0?CT_FM:CT_SMK;
		ctinfo->ct_cblknum += ctinfo->ct_complete;

	} else if ( flag == CTCMD_ERROR && arg )
		ctinfo->ct_cblknum += ctinfo->ct_complete;
}


/*
 *   Load, unload or retension.  The `arg' is the specific action to take.
 *   Note that the CT_MOTION bit is never set here, because then we might
 *   not do a needed modeselect.
*/
static void
ctload(ctinfo_t *ctinfo, int flag, __psunsigned_t arg)
{
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = L_UL_CMD;
		ctinfo->ct_g0cdb.g0_cmd3 = arg & 3;
		/* If DST Unload, set Vendor Specific Locate bit
		 * to unload at nearest System Zone. */
		if (IS_DST(ctinfo) && (!(arg & L_UL_LOAD)))
			ctinfo->ct_g0cdb.g0_vu67 = 1;
	}
	else if(flag == CTCMD_NO_ERROR) {
		/* if we completed successfully, we are either at
		 * PBOT, or LBOT, depending on the cmd.  If at
		 * PBOT, we are effectively offline, and need a
		 * load cmd before next tape motion (and some other
		 * cmds, depending on drive type).  This is almost
		 * the same as changing a tape, except we can
		 * remember the QIC* bits.  If tape is
		 * loaded/unloaded OK, forget about reads and
		 * writes; this also prevents us from writing a FM
		 * at BOT if programs rewinds/offlines prior to
		 * closing; unfortunately, in this case the tape
		 * won't have a FM at the end of the data just
		 * written, it's assumed the program knows what
		 * it's doing... ; note that all the state info
		 * gets cleared right away even on immediate_rew.
		 * clear CT_MOTION since this causes close to return
		 * EIO after an MTUNLOAD */
		ctinfo->ct_state &= ~(BACKSTATES|CT_ONL|CT_READ|CT_WRITE|
					CT_MOTION);
		if(arg & L_UL_LOAD) {
			ctinfo->ct_state |= CT_ONL|CT_LOADED|CT_BOT;
		}
	}
}


/*	Retrieve statistical information from the device.	*/
/*ARGSUSED2*/
static void
ctrdlog( ctinfo_t *ctinfo, int flag, __psunsigned_t arg, __psunsigned_t arg2 )
{
	mtscsi_rdlog_t	*mt = &ctinfo->ct_rdlog;

	if ( flag == CTCMD_SETUP ) {
		ctinfo->ct_g2cdb.g2_opcode = LOG_SENSE_CMD;
		ctinfo->ct_g2cdb.g2_cmd0   = (mt->mtppc << 1) | mt->mtsp;
		ctinfo->ct_g2cdb.g2_cmd1   = (mt->mtpc << 6) | mt->mtpage;
		ctinfo->ct_g2cdb.g2_cmd4   = (unchar)(mt->mtparam >> 8);
		ctinfo->ct_g2cdb.g2_cmd5   = (unchar)(mt->mtparam & 0xff);
		ctinfo->ct_g2cdb.g2_cmd6   = (unchar)(mt->mtlen >> 8);
		ctinfo->ct_g2cdb.g2_cmd7   = (unchar)(mt->mtlen & 0xff);

		ctinfo->ct_req->sr_flags  |= SRF_DIR_IN;
		ctinfo->ct_req->sr_buffer  = (u_char *)ctinfo->ct_logdata;
		ctinfo->ct_req->sr_buflen  = mt->mtlen;
		dki_dcache_inval( ctinfo->ct_logdata, mt->mtlen );
	}
}


/* Prevent/Allow  Media removal command */
static void
ctprevmedremov(ctinfo_t *ctinfo, int flag, __psunsigned_t arg)
{
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = PREV_MED_REMOV_CMD;
		ctinfo->ct_g0cdb.g0_cmd3 = arg;
	}
}

/*	get the block limits, for checking in ctstrategy, and for
 *	the MTIOCGETBLKINFO ioctl.
*/
/*ARGSUSED2*/
static void
ctblklimit(ctinfo_t *ctinfo, int flag, __psunsigned_t arg)
{
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = REQ_BLKLIM_CMD;

		ctinfo->ct_req->sr_buffer = ctinfo->ct_reqblkinfo;
		ctinfo->ct_req->sr_buflen = sizeof(ctinfo->ct_reqblkinfo);
		ctinfo->ct_req->sr_flags |= SRF_DIR_IN;

		dki_dcache_inval(ctinfo->ct_req->sr_buffer,
			ctinfo->ct_req->sr_buflen);
	}
	else if(flag == CTCMD_NO_ERROR) {
		u_char *info = ctinfo->ct_reqblkinfo;

		ctinfo->ct_cur_minsz = ((ulong)(info[4]) << 8) | info[5];
		ctinfo->ct_cur_maxsz = ((ulong)(info[1]) << 16) |
			((ulong)(info[2]) << 8) | info[3];
		if(ctinfo->ct_cur_minsz == 0)
		/* on Kennedy in fixed mode, if blksz is 0x10000, then
		 * min is 0, since it only has 2 bytes.  This means the
		 * Kennedy can't read tapes w blksizes > 64k in fixed
		 * mode.  It can't in variable mode either, but
		 * theoretically it could.  minsz==maxsz used to
		 * catch invalid i/o sizes in strat.  This would be
		 * true of any device with 64k or larger blocks when
		 * in fixed mode, so just do it. */
			ctinfo->ct_cur_minsz = ctinfo->ct_cur_maxsz;

		/* save the absolute max and min for the BLKINFO ioctl.
		 * Kennedy only one affected by this for now.  It
		 * retuns the min and max based on the values last set,
		 * if not at BOT */
		if(ctinfo->blkinfo.minblksz > ctinfo->ct_cur_minsz)
			ctinfo->blkinfo.minblksz = ctinfo->ct_cur_minsz;
		if(ctinfo->blkinfo.maxblksz < ctinfo->ct_cur_maxsz)
			ctinfo->blkinfo.maxblksz = ctinfo->ct_cur_maxsz;
	}
	else ct_dbg((C, "getblklimits fails.  "));
}


/*	get all the drive specific info after a request sense. Don't
 *	bother doing stuff that will be done anyway because of the
 *	primary sense key.  Only called from ctchkerr().
*/
static void
ctdrivesense(ctinfo_t *ctinfo)
{
	ct_g0es_data_t	*esdptr = ctinfo->ct_es_data;
	u_char addsense0 = 0, addsense1 = 0;
	uint recovers = 0;

	switch(ctinfo->ct_typ.tp_hinv) {
	case TPDLT:
	case TPDLTSTACKER:
		addsense1 = esdptr->rsscsi2.aq;
		/* these are only the 'interesting' info states, in that
		 * we either report them, or set a state flag, or set
		 * to report some different kind of error.  */
		switch(addsense0 = esdptr->rsscsi2.as) {
		case 0: switch(addsense1) {
		    case 1: ctinfo->ct_state |= CT_FM; break;
		    case 3: ctinfo->ct_state |= CT_SMK; break;
		    case 4: ctinfo->ct_state |= CT_BOT; break;
		    case 5: ctinfo->ct_state |= CT_EOD; break;
		    }
		    break;
		case 3:
		    if(addsense1 == 2)
			ctalert(ctinfo, 
				"Excessive write errors");
		    break;
		case 0x1a:
		    ct_dbg((C, "invalid length for param list. "));
		    break;
		case 0x20:
		    ct_dbg((C, "illegal opcode. "));
		    break;
		case 0x24:
		    ct_dbg((C, "invalid CDB. "));
		    break;
		case 0x25:
		    ct_dbg((C, "illegal LUN. "));
		    break;
		case 0x26:
		    ct_dbg((C, "invalid param list. "));
		    break;
		case 0x27:
		    ctinfo->ct_state |= CT_WRP;
		    break;
		case 0x30:
		    ctreport(ctinfo, "Incompatible media in drive, may be wrong tape type\n");
		    break;
		case 0x3a:
		    ctinfo->ct_state &= ~CT_ONL;
		    break;
		case 0x40:
		    ctreport(ctinfo,
			"Drive diagnostic failure: code 0x%x\n",
			addsense1);
		    break;
		case 0x51:
		    ctreport(ctinfo, "Erase failure. ");
		    break;
		case 0x53: switch(addsense1) {
		    case 0:
			ctreport(ctinfo,"media load/eject failure. ");
			ctinfo->ct_state2 |= CT_LOAD_ERR;
			break;
		    case 1:
			ctreport(ctinfo,"unload tape failure. ");
			break;
		    case 2:
			ctreport(ctinfo,"media removal prevented. ");
			break;
		    }
		    break;
		case 0x80:
		    if( addsense1 == 1 )
			ctinfo->ct_state |= CT_CLEANHEADS;
		    break;
		}
		break;
	case TP8MM_8200:
	case TP8MM_8500:
	case TP8MM_8900:
		recovers = (esdptr->exab.errs[0] << 16) |
			(esdptr->exab.errs[1] << 8) | esdptr->exab.errs[2];
		addsense0 = esdptr->exab.as;
		addsense1 = esdptr->exab.aq;
		if(esdptr->exab.bot) {
			ctinfo->ct_state &= ~BACKSTATES;
			ctinfo->ct_state |= CT_BOT;
		}
		if(esdptr->exab.wp)
			ctinfo->ct_state |= CT_WRP;
		if(esdptr->exab.tnp)
			ctinfo->ct_state &= ~CT_ONL;
		if(esdptr->exab.pf)
			ct_dbg((C, "powerfail/reset.  "));
		if(esdptr->exab.bpe)
			ct_dbg((C, "scsi bus parity error.  "));
		if(esdptr->exab.fpe)
			ct_dbg((C, "buffer parity error.  "));
		if(esdptr->exab.me)
			/* tell the user; it's better than std error msg */
			ctalert(ctinfo, "Uncorrectable media error");
		if(esdptr->exab.eco)
			ct_dbg((C, "retry ctr overflow.  "));
		if(esdptr->exab.tme)
			ct_dbg((C, "tape motion error.  "));
		if(esdptr->exab.fmke)
			ct_dbg((C, "fmk write error.  "));
		if(esdptr->exab.ure)
			ct_dbg((C, "underrun error.  "));
		if(esdptr->exab.we1)
			ct_dbg((C, "max write retries error.  "));
		if(esdptr->exab.sse)
			/* this is difficult for user to diagnose, and we
			 * have seen a fair number of them, so
			 * actually tell the user about this */
			ctreport(ctinfo, "Exabyte servo failure\n");
		if(esdptr->exab.fe)
			ctreport(ctinfo, "Exabyte data formatter failure\n");
		if(esdptr->exab.tmd)
			ct_dbg((C, "TMD set.  "));
		if(esdptr->exab.xfr)
			ct_dbg((C, "XFR set.  "));
		if(esdptr->exab.wseb || esdptr->exab.wse0)
			ct_dbg((C, "write splice error.  "));
		if(esdptr->exab.cln && ((ctinfo->ct_typ.tp_type == EXABYTE8500)
			|| (ctinfo->ct_typ.tp_type == EXABYTE8900)))
			ctinfo->ct_state |= CT_CLEANHEADS;
		else
			ctinfo->ct_state &= ~CT_CLEANHEADS;
		ct_xdbg(2, (C, "%dK left.  ",
			(((ulong)esdptr->exab.left[0]) << 16) |
			(((ulong)esdptr->exab.left[1]) << 8) |
			(ulong)esdptr->exab.left[2]));
		if(ctinfo->ct_typ.tp_type == EXABYTE8900) {
			if (esdptr->exab.rrr)
				ct_dbg((C, "tape forced to invoke retries. "));
			if (esdptr->exab.clnd)
				ct_dbg((C, "tape has been cleaned. "));
			if (esdptr->exab.ucln)
				ctreport(ctinfo, "Cleaning tape was used up\n");
		}
			
		break;
	case TP9TRACK:
		if(esdptr->esd_defer)
			ct_dbg((C, "DEFERed error: "));
		/* Kennedy is more like the disk drives; it provides
		 * additional sense bytes, but no recovered error cnt. */
		addsense0 = esdptr->kenn.as;
		addsense1 = esdptr->kenn.aq;
		if(addsense0 == 0) {
			switch(addsense1) {
			case 1:
				ctinfo->ct_state |= CT_FM;
				break;
			case 2:
				ctinfo->ct_state &= ~CTPOS;
				ctinfo->ct_state |= CT_EW;
				break;
			case 4:
				ctinfo->ct_state &= ~BACKSTATES;
				ctinfo->ct_state |= CT_BOT;
				break;
			case 5:
				ctinfo->ct_state &= ~CTPOS;
				ctinfo->ct_state |= CT_EOD;
				break;
			}
		}
		else if(addsense0 == 0x11 && addsense1 == 1 && 
			(ctinfo->ct_state & CT_EW))
			/* this happens when data was written past the
			eot marker almost to the very end of the tape */
			ctinfo->ct_state |= CT_EOM;
		else if(addsense0 == 0x27)
			ctinfo->ct_state |= CT_WRP;
		else if(addsense0 == 0x2E)
			ctinfo->ct_state |= CT_EOD;
		break;
	case TPQIC24:
		switch(ctinfo->ct_typ.tp_type) {
		case CIPHER540:
		    if(esdptr->esd_aslen) {	/* older firmware may have 0! */
			addsense0 = esdptr->ciph.as;
			if(addsense0 == 0x17)
			    ctinfo->ct_state |= CT_WRP;
			if(addsense0 == 0x34)
			    ctinfo->ct_state |= CT_EOD;
			ct_dbg((C, "Cipher, aslen %d, recerr %x,%x.  ",
			    esdptr->esd_aslen, esdptr->ciph.errs[0],
			    esdptr->ciph.errs[1]));
			recovers = (esdptr->ciph.errs[0] << 8) |
			    esdptr->ciph.errs[1];
		    }
		    break;
		case VIPER60:
		    recovers =
			(esdptr->viper.errs[0] << 8) | esdptr->viper.errs[1];
		    /* no additional sense. */
		    break;
		}
		break;
	case TPQIC150:
		switch(ctinfo->ct_typ.tp_type) {
		case TANDBERG3660:
		    recovers =
			    (esdptr->tand.errs[0] << 8) | esdptr->tand.errs[1];
		    addsense0 = esdptr->tand.ercl;
		    addsense1 = esdptr->tand.ercd;
		    switch(addsense0) {
		    case 0xc: case 0x11: case 0x31: case 0x34:
		    case 0x35: case 0x38: case 0x39: case 0x3A:
		    case 0x3B: case 0x3C: case 0x3D: case 0x3e: case 0x3F:
		    /* we report the ones that the manual claims aren't cleared
		     * by a reset or power cycle, to make life easier for
		     * support people */
			ctreport(ctinfo, "Unrecoverable drive error cl=%x,"
			    "cd=%x, exercd %x\n", addsense0, addsense1,
			    esdptr->tand.exercd);
			break;
		    case 0x1d:	/* writing and early warning */
			ctinfo->ct_state |= CT_EW;
			break;
		    case 0x17:	/* read LEOT (you get this on end of data;
			i.e. at a blank chk; not at EW, like most other mfg.) */
			ctinfo->ct_state |= CT_EOD;
			break;
		    case 0x18:	/* read PEOT */
		    case 0x1e:	/* writing */
			ctinfo->ct_state &= CT_BOT;	/* paranoia */
			ctinfo->ct_state |= CT_EW;
			break;
		    case 0x1f:
			ctinfo->ct_state |= CT_WRP;
			break;
		    }
		    break;
		case VIPER150:
		    recovers =
			(esdptr->viper.errs[0] << 8) | esdptr->viper.errs[1];
		    /* no additional sense. */
		    break;
		}
		break;
	case TPUNKNOWN:
		if(ctinfo->ct_scsiv > 1)
			goto scsi2as;	/* unknown, but scsi 2 */
		break;
	case TPDAT:
		if(ctinfo->ct_scsiv < 2)
		    /* scsi1 mode (we don't ship this, but customers...) */
		    recovers =
			(esdptr->viper.errs[0] << 8) | esdptr->viper.errs[1];
		else {
scsi2as:
			addsense1 = esdptr->rsscsi2.aq;
			/* these are only the 'interesting' info states, in that
			 * we either report them, or set a state flag, or set
			 * to report some different kind of error.  */
			switch(addsense0 = esdptr->rsscsi2.as) {
			case 0: switch(addsense1) {
			    case 1: ctinfo->ct_state |= CT_FM; break;
			    case 2: ctinfo->ct_state |= CT_EOM; break;
			    case 3: ctinfo->ct_state |= CT_SMK; break;
			    case 4: ctinfo->ct_state |= CT_BOT; break;
			    case 5: ctinfo->ct_state |= CT_EOD; break;
			    }
			    break;
			case 3:
			    if(addsense1 == 2)
				ctalert(ctinfo, "Excessive write errors");
			    break;
			case 0x1a:
			    ct_dbg((C, "invalid length for param list. "));
			    break;
			case 0x20:
			case 0x24:
			    ct_dbg((C, "invalid CDB. "));
			    break;
			case 0x26:
			    ct_dbg((C, "invalid param list. "));
			    break;
			case 0x27:
			    ctinfo->ct_state |= CT_WRP;
			    break;
			case 0x30:
			    if(addsense1 == 3)
				ctreport(ctinfo,
				    "Cleaning cartridge in drive\n");
			    else if(addsense1 > 0xc2 && addsense1 < 0xc6) {
				ctreport(ctinfo,
				    "Compressed data error\n");
			    }
			    else {
		    /* since some firmware revs are not great about getting
		     * the tape type correct, we try to correct that
		     * when loading the tape.  Even the revs that
		     * mostly get it correct don't correct their state
		     * after this error occurs...  The only downside to
		     * this is that we may mess up if a compressed tape
		     * gets reported as incompatible media...  So far,
		     * it seems to get reported as 0x31, corrupted media.
		     * Preserve and restore the sense data and state,
		     * so ctmodesel side effects/errors get ignored. This
		     * only happens after "good" modesel's are done, so we
		     * shouldn't cause any problems by doing this, we hope.
		     * We can still get the incompat media validly if we
		     * recorded audio on previously data tape, and the
		     * data extended past where we stopped recording.
		     * so if we have been reading successfully in audio mode,
		     * and get this, then don't report it; 10 is a reasonable
		     * heuristic.  We don't clear aud_media, since we are still
		     * at the audio media.  The incompat media bit gets cleared
		     * on any repositioning command.
		    */
				if(ctinfo->ct_readcnt > 10 &&
				    (ctinfo->ct_state &
				    (CT_AUDIO|CT_AUD_MED|CT_READ)) ==
				    (CT_AUD_MED|CT_AUDIO|CT_READ))
				    ctinfo->ct_state |= CT_INCOMPAT_MEDIA;
				else
				    ctreport(ctinfo,
				       "Incompatible media in drive, may be blank tape or wrong tape type\n");

			    if(ctsuppress && IS_DATTAPE(ctinfo)) {

				ct_g0es_data_t	esd;
				ulong state;
				state = ctinfo->ct_state ^= CT_AUD_MED;
				bcopy(ctinfo->ct_es_data, &esd,
				    MAX_SENSE_DATA);
				ct_dbg((C, "new aud state %x. ",state));
				ctcmd(ctinfo, ctmodesel, 0,
				    SHORT_TIMEO, 0, 0);
				ctinfo->ct_state = state;
				bcopy(&esd, ctinfo->ct_es_data,
				    MAX_SENSE_DATA);
			    }
			}
			    break;
			case 0x31:
			    ctalert(ctinfo, "Media format is corrupted");
			    break;
			case 0x3a:
			    ctinfo->ct_state &= ~CT_ONL;
			    break;
			case 0x40:
			    ctreport(ctinfo,
				"Drive diagnostic failure: code 0x%x\n",
				addsense1);
			    break;
			case 0x70: /* returned by Python f/w >= 2.68 */
			    ctreport(ctinfo,
				"Decompression error: 0x%x\n",
				addsense1);
			    break;
			}

			switch(esdptr->esd_sensekey) {
			case SC_ERR_RECOVERED:
			    /* may want to jerk this when we go to logsense */
			    recovers +=
				(esdptr->rsscsi2.sensespecific[0] << 8)
				| esdptr->rsscsi2.sensespecific[1];
			    break;
			case SC_ILLEGALREQ:
			    if(esdptr->rsscsi2.sksv)
				ct_dbg((C, "illegal byte 0x%x in %s, bit %d\n",
				    (esdptr->rsscsi2.sensespecific[0] << 8)
				    | esdptr->rsscsi2.sensespecific[1],
				    esdptr->rsscsi2.cd?"cmd":"data",
				    esdptr->rsscsi2.bitp));
			    break;
			}
		}
		break;
	case TPFUJDIANA1:
	case TPFUJDIANA2:
	case TPFUJDIANA3:
			addsense1 = esdptr->rsscsi2.aq;
			/* these are only the 'interesting' info states, in that
			 * we either report them, or set a state flag, or set
			 * to report some different kind of error.  */
			switch(addsense0 = esdptr->rsscsi2.as) {
			case 0: switch(addsense1) {
			    case 1:
				ctinfo->ct_state |= CT_FM;
				break;
			    case 2:	/* detect EW for proper EOT handling */
				ctinfo->ct_state &= ~CTPOS;
				ctinfo->ct_state |= CT_EW;
				break;
			    case 4:
				ctinfo->ct_state &= ~BACKSTATES;
				ctinfo->ct_state |= CT_BOT;
				break;
			    case 5:
				ctinfo->ct_state &= ~CTPOS;
				ctinfo->ct_state |= CT_EOD;
				break;
			    }
			    break;
			case 3:
			    if(addsense1 == 2)
				ctalert(ctinfo, "Excessive write errors");
			    break;
			case 0x1a:
			    ct_dbg((C, "invalid length for param list. "));
			    break;
			case 0x20:
			case 0x24:
			    ct_dbg((C, "invalid CDB. "));
			    break;
			case 0x26:
			    ct_dbg((C, "invalid param list. "));
			    break;
			case 0x27:
			    ctinfo->ct_state |= CT_WRP;
			    break;
			case 0x30:
			    if(addsense1 == 3)
				ctreport(ctinfo,
				    "Cleaning cartridge in drive\n");
			    break;
			case 0x31:
			    ctalert(ctinfo, "Media format is corrupted");
			    break;
			case 0x3a:
			    ctinfo->ct_state &= ~CT_ONL;
			    break;
			case 0x40:
			    ctreport(ctinfo,
				"Drive diagnostic failure: code 0x%x\n",
				addsense1);
			    break;
                        case 0x53:
                            if ( addsense1 == 0 )
                                ctinfo->ct_state2 |= CT_LOAD_ERR;
                            break;
			}

			switch(esdptr->esd_sensekey) {
			case SC_ERR_RECOVERED:
			    /* may want to jerk this when we go to logsense */
			    recovers +=
				(esdptr->rsscsi2.sensespecific[0] << 8)
				| esdptr->rsscsi2.sensespecific[1];
			    break;
			case SC_ILLEGALREQ:
			    if(esdptr->rsscsi2.sksv)
				ct_dbg((C, "illegal byte 0x%x in %s, bit %d\n",
				    (esdptr->rsscsi2.sensespecific[0] << 8)
				    | esdptr->rsscsi2.sensespecific[1],
				    esdptr->rsscsi2.cd?"cmd":"data",
				    esdptr->rsscsi2.bitp));
			    break;
			}
		break;
	case TPNTP:
	case TPNTPSTACKER:
        case TPMGSTRMP:
        case TPMGSTRMPSTCKR:
	case TPSTK9490:
	case TPSTKSD3:
	case TPSTK9840:
        case TPNCTP:
        case TPTD3600:
        case TPOVL490E:
		addsense1 = esdptr->rsscsi2.aq;
		/* these are only the 'interesting' info states, in that
		 * we either report them, or set a state flag, or set
		 * to report some different kind of error.  */
		switch(addsense0 = esdptr->rsscsi2.as) {
		case 0: switch(addsense1) {
		    case 1: ctinfo->ct_state |= CT_FM; break;
		    case 3: ctinfo->ct_state |= CT_SMK; break;
		    case 4: ctinfo->ct_state |= CT_BOT; break;
		    case 5: ctinfo->ct_state |= CT_EOD; break;
		    }
		    break;
		case 3:
		    if(addsense1 == 2)
			ctalert(ctinfo, 
				"Excessive write errors");
		    break;
		case 0x1a:
		    ct_dbg((C, "invalid length for param list. "));
		    break;
		case 0x20:
		    ct_dbg((C, "illegal opcode. "));
		    break;
		case 0x24:
		    ct_dbg((C, "invalid CDB. "));
		    break;
		case 0x25:
		    ct_dbg((C, "illegal LUN. "));
		    break;
		case 0x26:
		    ct_dbg((C, "invalid param list. "));
		    break;
		case 0x27:
		    ctinfo->ct_state |= CT_WRP;
		    break;
		case 0x30:
		    ctreport(ctinfo, "Incompatible media in drive, may be wrong tape type\n");
		    break;
	        case 0x31:
                    ctalert(ctinfo, "Media format is corrupted");
                    break;
		case 0x3a:
		    ctinfo->ct_state &= ~CT_ONL;
		    break;
		case 0x40:
		    ctreport(ctinfo,
			"Drive diagnostic failure: code 0x%x\n",
			addsense1);
		    break;
		case 0x51:
		    ctreport(ctinfo, "Erase failure. ");
		    break;
		case 0x53: switch(addsense1) {
		    case 0:
			ctreport(ctinfo,"media load/eject failure. ");
			ctinfo->ct_state2 |= CT_LOAD_ERR;
			break;
		    case 1:
			ctreport(ctinfo,"unload tape failure. ");
			break;
		    case 2:
			ctreport(ctinfo,"media removal prevented. ");
			break;
		    }
		    break;
		}

                switch(esdptr->esd_sensekey) {
                case SC_ERR_RECOVERED:
                    /* may want to jerk this when we go to logsense */
                    recovers +=
                        (esdptr->rsscsi2.sensespecific[0] << 8)
                        | esdptr->rsscsi2.sensespecific[1];
                    break;
                case SC_ILLEGALREQ:
                    if(esdptr->rsscsi2.sksv)
                        ct_dbg((C, "illegal byte 0x%x in %s, bit %d\n",
                            (esdptr->rsscsi2.sensespecific[0] << 8)
                            | esdptr->rsscsi2.sensespecific[1],
                            esdptr->rsscsi2.cd?"cmd":"data",
                            esdptr->rsscsi2.bitp));
                    break;
                }

		break;

	case TP8MM_AIT:
		addsense1 = esdptr->rsscsi2.aq;
		addsense0 = esdptr->rsscsi2.as;
		switch(esdptr->esd_sensekey) {
		case SC_NOSENSE:
			switch(addsense0) {
			case 0: 
				switch(addsense1) {
				case 1: ctinfo->ct_state |= CT_FM; break;
				case 2: ctinfo->ct_state |= CT_EOM; break;
				case 3: ctinfo->ct_state |= CT_SMK; break;
				case 4: ctinfo->ct_state |= CT_BOT; break;
				case 5: ctinfo->ct_state |= CT_EOD; break;
				}
				break;
			case 0xa:
				ct_dbg((C, "error log overflow/error rate warning "));
				ctalert (ctinfo, "Excessive i/o errors.");
				break;
			case 0x80:
				ctinfo->ct_state |= CT_CLEANHEADS;
				break;
			}
			break;
		case SC_NOT_READY:
			switch (addsense0) {
			case 0x3a:
				ct_dbg ((C, "no media. "));
				ctinfo->ct_state &= ~CT_ONL;
				break;
			case 0x4:
				switch (addsense1) {
				case 0: ct_dbg ((C, "not ready. ")); break;
				case 1: ct_dbg ((C, "becoming ready. ")); break;
				}
			}
			break;
		case SC_MEDIA_ERR:
			switch (addsense0) {
			case 0:
				if (addsense1 == 1) ctinfo->ct_state |= CT_EOM;
				break;
			case 0xc:
				ct_dbg ((C, "raw retry limit exceeded. "));
				break;
			case 0x11:
				if (addsense1 == 0) ct_dbg ((C, "unrecovered read. "));
				else if (addsense1 == 8) {
					ct_dbg ((C, "incomplete read. "));
					ctinfo->ct_state |= CT_CLEANHEADS;
				}
				break;
			case 0x14:
				switch (addsense1) {
				case 0x3:
					ct_dbg ((C, "EOD not found. "));
					ctinfo->ct_state |= CT_CLEANHEADS;
					break;
				case 0x4:
					ct_dbg ((C, "block seq err. "));
					break;
				}
			case 0x15:
				if (addsense1 ==2) ct_dbg ((C, "pos err. "));
				break;
			case 0x30:
			case 0x31:
				ct_dbg ((C, "incompat media %x %x ",addsense0,addsense1));
				ctreport (ctinfo, "Incompatible media.");
				ctreport (ctinfo, "Incompatible media in drive, may be wrong tape type.");
				ctinfo->ct_state |= CT_INCOMPAT_MEDIA;
				break;
			case 0x33:
				ct_dbg ((C, "part size err. "));
				break;
			case 0x3b:
				switch (addsense1) {
				case 0x0:
					ctinfo->ct_state |= CT_NEEDBOT;
					ct_dbg ((C, "need bot. "));
					break;
				case 0x1:
					ctalert (ctinfo, "Broken tape.");
					break;
				case 0x8:
					ct_dbg ((C, "pos lost. "));
					break;
				}
				break;
			case 0x50:
				ct_dbg ((C, "write append err. "));
				break;
			case 0x70:
			case 0x71:
				ct_dbg ((C, "decomp exception: %x %x ",addsense0, addsense1));
				ctreport (ctinfo, "data decompression exception.");
				break;
			}
			break;
		case SC_HARDW_ERR:
			ctalert (ctinfo, "hardware error, code 0x%x.",esdptr->asd._rsscsi2.sensespecific[0]);
			switch (addsense0) {
			case 0x3:
				ctreport (ctinfo, "write fault");
				break;
			case 0x9:
				break;
			case 0x15:
				break;
			case 0x44:
				ctalert (ctinfo, "internal target failure, drive fru code 0x%x.",esdptr->asd._rsscsi2.fru);
				break;
			case 0x52:
				ctalert (ctinfo, "cartridge fault");
				break;
			case 0x53:
				ctalert (ctinfo, "tape load/eject failed");
				break;
			}
			break;
		case SC_ILLEGALREQ:
			if(esdptr->rsscsi2.sksv)
				ct_dbg((C, "illegal byte 0x%x in %s, bit %d\n",
					(esdptr->rsscsi2.sensespecific[0] << 8)
					| esdptr->rsscsi2.sensespecific[1],
					esdptr->rsscsi2.cd?"cmd":"data",
					esdptr->rsscsi2.bitp));
			switch (addsense0) {
			case 0x1a:
				ct_dbg((C, "invalid length for param list. "));
				break;
			case 0x20:
				ct_dbg((C, "illegal opcode. "));
				break;
			case 0x24:
				ct_dbg((C, "invalid CDB. "));
				break;
			case 0x25:
				ct_dbg((C, "illegal LUN. "));
				break;
			case 0x26:
				if (addsense1 == 0)
					ct_dbg((C, "invalid param list. "));
				else if (addsense1 == 1)
					ct_dbg((C, "parameter not supported. "));
				break;
			case 0x2c:
				ct_dbg ((C, "cmd seq err. "));
				break;
			case 0x3d:
				ct_dbg ((C, "invalid identify msg. "));
				break;
			}
			break;
		case SC_UNIT_ATTN:
			switch (addsense0) {
			case 0x28:
				ct_dbg ((C, "not ready to ready. "));
				break;
			case 0x29:
				if (addsense1 == 0)
					ct_dbg ((C, "pwr on or reset. "));
				else if (addsense1 == 0x80) {
					ct_dbg ((C, "diag failure. "));
					ctalert (ctinfo, "diagnostic failure.");
				}
				break;
			case 0x2a:
				ct_dbg ((C, "params changed 0x%x . ",addsense1));
				break;
			}
			break;
		case SC_DATA_PROT:
			if (addsense0 == 0x27) ctinfo->ct_state |= CT_WRP;
			break;
		case SC_BLANKCHK:
			ct_dbg ((C, "blank chk 0x%x. ",addsense1));
			if (addsense1 == 5) {
				ct_dbg ((C, "eod. "));
				ctinfo->ct_state |= CT_EOD;
			}
			break;
		case SC_CMD_ABORT:
			ct_dbg ((C, "cmd abort 0x%x. ",addsense0));
			break;
		case SC_VOL_OVERFL:
			if (addsense0 == 0 && addsense1 == 2) ctinfo->ct_state |= CT_EOM;
			ct_dbg ((C, "vol ovrflw. "));
			break;
		case SC_VENDUNIQ:
		case SC_COPY_ABORT:
		case SC_EQUAL:
		case SC_ERR_RECOVERED:
		case SC_MISCMP:
		default:
			ct_dbg ((C, "unexpected sense key 0x%x. ",esdptr->esd_sensekey));
			break;
                }

		break;

	case TPGY10:
	case TPGY2120:
		addsense1 = esdptr->rsscsi2.aq;
		/* these are only the 'interesting' info states, in that
		 * we either report them, or set a state flag, or set
		 * to report some different kind of error.  */
		switch(addsense0 = esdptr->rsscsi2.as) {
		case 0: switch(addsense1) {
		    case 1: ctinfo->ct_state |= CT_FM; break;
		    case 3: ctinfo->ct_state |= CT_SMK; break;
		    case 4: ctinfo->ct_state |= CT_BOT; break;
		    case 5: ctinfo->ct_state |= CT_EOD; break;
		    }
		    break;
		case 3:
		    if(addsense1 == 2)
			ctalert(ctinfo, 
				"Excessive write errors");
		    break;
		case 0x1a:
		    ct_dbg((C, "invalid length for param list. "));
		    break;
		case 0x20:
		    ct_dbg((C, "illegal opcode. "));
		    break;
		case 0x24:
		    ct_dbg((C, "invalid CDB. "));
		    break;
		case 0x25:
		    ct_dbg((C, "illegal LUN. "));
		    break;
		case 0x26:
		    ct_dbg((C, "invalid param list. "));
		    break;
		case 0x27:
		    ctinfo->ct_state |= CT_WRP;
		    break;
		case 0x30:
		    ctreport(ctinfo, "Incompatible media in drive, may be wrong tape type\n");
		    break;
	        case 0x31:
                    ctalert(ctinfo, "Media format is corrupted");
                    break;
		case 0x3a:
		    ctinfo->ct_state &= ~CT_ONL;
		    break;
	        case 0x4c:
                    ctalert(ctinfo, "Tape has not been loaded");
                    break;
		case 0x50:
		    ctreport(ctinfo,
			"Error occurred in WRITE operation");
		    break;
		case 0x51:
		    ctreport(ctinfo, "Erase failure. ");
		    break;
		case 0x90: switch(addsense1) {
		    case 2:
			ctreport(ctinfo,"Tape has not been formatted");
			break;
		    }
		    break;
		}

                switch(esdptr->esd_sensekey) {
                case SC_ERR_RECOVERED:
                    /* may want to jerk this when we go to logsense */
                    recovers +=
                        (esdptr->rsscsi2.sensespecific[0] << 8)
                        | esdptr->rsscsi2.sensespecific[1];
                    break;
                case SC_ILLEGALREQ:
                    if(esdptr->rsscsi2.sksv)
                        ct_dbg((C, "illegal byte 0x%x in %s, bit %d\n",
                            (esdptr->rsscsi2.sensespecific[0] << 8)
                            | esdptr->rsscsi2.sensespecific[1],
                            esdptr->rsscsi2.cd?"cmd":"data",
                            esdptr->rsscsi2.bitp));
                    break;
                }

		break;

	case TPD2:
		addsense1 = esdptr->rsscsi2.aq;
		/* these are only the 'interesting' info states, in that
		 * we either report them, or set a state flag, or set
		 * to report some different kind of error.  */
		switch(addsense0 = esdptr->rsscsi2.as) {
		case 0x00: switch(addsense1) {
			case 0x01: ctinfo->ct_state |= CT_FM;  break;
			case 0x04: ctinfo->ct_state |= CT_BOT; break;
			case 0x05: ctinfo->ct_state |= CT_EOD; break;
		    }
		    break;
		case 0x04: switch(addsense1) {
			case 0x01:
			    ct_dbg((C, "In process of becoming ready (vol search). "));
			    break;

			case 0x02:
			    /* DST tape only.
			     */
			    if (IS_DST(ctinfo)) {
			    	ct_dbg((C, "Implicit tape position "
					"not established. "));
				ctinfo->ct_state2 |= CT_INV_POS;
			    }
			    break;

			case 0x03:
			    ct_dbg((C, "Unit not ready - manual intervention required. "));
			    break;
		    }
		    break;
		case 0x1a:
		    ct_dbg((C, "invalid length for param list. "));
		    break;
		case 0x20:
		case 0x24:
		    ct_dbg((C, "invalid CDB. "));
		    break;
		case 0x26:
		    ct_dbg((C, "invalid param list. "));
		    break;
		case 0x27:
		    ctinfo->ct_state |= CT_WRP;
		    break;
		case 0x28: switch(addsense1) {
			case 0x00:
			    ct_dbg((C, "Unit became ready. "));
			    break;
			case 0x80:
			    ct_dbg((C, "Unit became ready (same tape). "));
			    break;
		    }
		    break;
		case 0x29:
		    ct_dbg((C, "Power on (or reset). "));
		    break;
		case 0x2a:
		    if (addsense1 == 0x01)
		    	ct_dbg((C, "Mode parameters changed. "));
		    break;
		case 0x30: switch(addsense1) {
			case 0x00:
			    ctreport(ctinfo, "Incompatible media in drive\n");
			    break;
			case 0x01:
			    ctreport(ctinfo, "Media has unknown format - blank tape?\n");
			    break;
			case 0x02:
			    ctreport(ctinfo, "Media has incompatible format\n");
			    break;
			case 0x03:
			    ctreport(ctinfo, "Cleaning cartridge in drive\n");
			    break;
		    }
		    break;
		case 0x31:
		    ctalert(ctinfo, "Media format is corrupted");
		    break;
		case 0x3a:
		    ct_dbg((C, "Media not present. "));
		    ctinfo->ct_state &= ~CT_ONL;
		    break;
		case 0x3e:
		    ct_dbg((C, "Lun not self-configured yet. "));
		    break;
		case 0x3f:
		    if (addsense1 == 0x03)
			ct_dbg((C, "Inquiry data changed. "));
		    break;
		case 0x44:
		    if (addsense1 == 0x80)
			ct_dbg((C, "Environmental fault. "));
		    break;
		}

		switch(esdptr->esd_sensekey) {
		    case SC_ERR_RECOVERED:
			/* may want to jerk this when we go to logsense */
			recovers += (esdptr->rsscsi2.sensespecific[0] << 8) | 
						esdptr->rsscsi2.sensespecific[1];
			break;
		    case SC_ILLEGALREQ:
			if(esdptr->rsscsi2.sksv)
			    ct_dbg((C, "illegal byte 0x%x in %s, bit %d\n",
					(esdptr->rsscsi2.sensespecific[0] << 8) |
						esdptr->rsscsi2.sensespecific[1],
					esdptr->rsscsi2.cd?"cmd":"data",
					esdptr->rsscsi2.bitp));
			break;
		}
		break;

	}

	if(addsense0 || addsense1)
		ct_dbg((C, "as0=%x as1=%x, ", addsense0, addsense1));
	ct_cxdbg(ctinfo->ct_scsiv >= 2 && esdptr->rsscsi2.sksv, 0,
		(C, "c/d=%x bpv=%x bit=%x byte=%x. ", esdptr->rsscsi2.cd,
		esdptr->rsscsi2.bpv, esdptr->rsscsi2.bitp,
		((uint)esdptr->rsscsi2.sensespecific[0]<<8) +
			esdptr->rsscsi2.sensespecific[1]));

	/* get bogus counts for 9 track, and others sometimes
	 * while we are opening, so just check here, rather
	 * than adding check at each drive type. */
	ctinfo->ct_recov_err += recovers;
}


/* Called when we get a short count at EOT.  In an ideal
 * world, this would not be an error if some data was read/written,
 * because then there is no way to know that some of the i/o was done. 
 * However, due to the braindead we implemented multivolume tapes, we
 * need to treat a short i/o as an error so multi-vol tapes can be
 * exchanged with older OS releases.  Only QIC and exabyte have this
 * problem; all newer drives do the 'right thing' (except that 9 track is
 * a bit difficult to deal with, because the drive doesn't enforce EOT;
 * tar, bru, cpio, etc. have been fixed to treat ENOSPC, or i/o return 0,
 * and at EW/EOT as multi-vol for IRIX 4.0.
 * Deliberately code this way so all new drives added will
 * work 'right', without code changes.
 * Also used at close to decide whether to write a filemark if we saw EW.
*/
static
ctresidOK(ctinfo_t *ctinfo)
{
	switch(ctinfo->ct_typ.tp_hinv) {
		case TPQIC24:
		case TPQIC150:
		case TP8MM_8200:
			ct_xdbg(1,(C, "ret ERR on resid. "));
			return 0;
		default:
			ct_xdbg(1,(C, "ret OK on resid. "));
			return 1;
	}
}


/*
 *  This common routine parses the sense key and other status bits
 *  and returns a PASS/FAIL indication.  As a side effect,
 *  it will correctly indicate (and possibly report) the error
 * (if any).  It is called only from ctcmd, and on status == ST_CHECK
 *  Any ctcmd calls from this code, or code it calls, must use a 
 *  last argument of 0 (no cmdsema usage in ctcmd).
*/
static
ctchkerr(ctinfo_t *ctinfo, short sensevalid)
{
	u_char	sensekey;
	buf_t	*bp;
	u_char opcode = ctinfo->ct_lastcmd;
	ct_g0es_data_t	*esdptr;
	u_char cmd0 = ctinfo->ct_g0cdb.g0_cmd0;	/* for SPACE_EOM_CODE */
	int residbytes, resid, reqcnt, error = 0;

	esdptr = ctinfo->ct_es_data;

	bp = ctinfo->ct_bp;

	ct_dbg((C,"chk t=%d,%d ", TPSCTLRID_DEV(bp->b_edev), SCSIID_DEV(bp->b_edev)));
	/* be paranoid; zero rest of data; most checks are for non-zero
	 * values */
	if(sensevalid < sizeof(*esdptr))
		bzero((caddr_t)esdptr + sensevalid, sizeof(*esdptr)-sensevalid);
#ifdef SCSICT_DEBUG
	/* not if ili because we get ili on reads in variable mode frequently */
	if(ctdebug>3 && (!esdptr->esd_ili || ctdebug>5)) {
		unsigned char *p = (unsigned char *)esdptr;
		unsigned char *lp = &p[sensevalid];
		printf("sensedata: ");
		while(p < lp)
			printf("%x ", *p++);
		printf("\n");
	}
#endif

	/* for determining if fw/bk tape motion; also for ILI error reports.
	 * Only needed for read, write, space cmds; (want sign extension
	 * on high byte, but compiler won't do it for us...) */
	sensekey = (ctinfo->ct_g0cdb.g0_cmd1 & 0x80) ? 0xff : 0;
	reqcnt = (sensekey<<24) | (((int)ctinfo->ct_g0cdb.g0_cmd1) << 16) |
		(((ulong)ctinfo->ct_g0cdb.g0_cmd2) << 8) |
		(ulong)ctinfo->ct_g0cdb.g0_cmd3;

	if (esdptr->esd_errclass != 7) {
		ct_dbg((CE_WARN, "bad class (%d) on reqsense len=0x%x, key=0x%x. ", esdptr->esd_errclass, sensevalid, sensekey));
		goto anerror;
	}

	if (esdptr->esd_valid)	{		/* get resid count if valid */
		resid = MKINT(esdptr->esd_resid);
		if ( ctinfo->ct_reqcnt < 0 )
			ctinfo->ct_complete = ctinfo->ct_reqcnt + ABS(resid);
		else
			ctinfo->ct_complete = ctinfo->ct_reqcnt - resid;
	} else
		resid = 0;

	if(resid) {
		/* note that if comparing against sr_buflen in this routine,
		 * cast it to int... */
		if(ISVAR_DEV(bp->b_edev))
			residbytes = resid;
		else	/* curblksz could be 0 if very first read is
			a short read */
			residbytes = resid * (ctinfo->blkinfo.curblksz ?
				ctinfo->blkinfo.curblksz :ctinfo->ct_blksz);
		ct_dbg((C, "resid %d/%d (%d), ", resid, reqcnt, residbytes));
	}
	else
		residbytes = 0;

	/* get drive specific stuff before chking for FM, etc., since
	 * they make get set there. */
	ctdrivesense(ctinfo);

	sensekey = esdptr->esd_sensekey;

	ct_dbg((C, "%s [%s]: ",
		sensekey?scsi_key_msgtab[sensekey]:"no sense", cmdnm(opcode)));

	if(esdptr->esd_eom||(IS_CIPHER(ctinfo) && sensekey == SC_VOL_OVERFL)) {
		/* may be converted from EOM to BOT in some special cases */
		ctinfo->ct_state |= CT_EW;
		ct_dbg((C, "at EW/BOT (esd_eom), "));
	}
	if(esdptr->esd_fm) {
		/* for general use; also needed by several checks below */
		ctinfo->ct_state |= CT_FM;
		ct_dbg((C, "at FM (esd_fm), "));
	}

	/* handle Fujitsu Diana-3 quirk: residue count builds up followng EW */
	if((opcode == READ_CMD || opcode == WRITE_CMD) &&
	    IS_FUJD3(ctinfo) && (ctinfo->ct_state & CT_EW) && residbytes)
		residbytes = 0;
	else
	/* If the residual is larger than the request, we can't recover data
	 * at this stage of the driver's life.  Punt.  */
	if((opcode == READ_CMD || opcode == WRITE_CMD) &&
		residbytes > (int)ctinfo->ct_req->sr_buflen)
		goto anerror;
	
	/* Now handle the sense key based on the command issued.
	 * The QIC-104 spec gives a priority to each sense key, in case
	 * 2 or more occur at the same time, only the highest priority
	 * gets indicated.  */
	switch(sensekey) {
	/* Illegal Request:
	 *   means we screwed up some values in the command descriptor
	 *   block or associated parameter list.  This is a driver error,
	 *	or some new type of drive.
	 *   Happens with a modesel on a Kennedy drive that
	 *	isn't at BOT, and when trying to set variable mode
	 *	on drives that don't support it.  */	
	case SC_ILLEGALREQ:
		if(IS_TANDBERG(ctinfo) && opcode == WRITE_CMD &&
			esdptr->tand.ercl == 0x10) {
			/* we get this on Tandberg 3660 when we try to write on
				QIC24 tapes */
			ctinfo->ct_state |= CT_QIC24;
			error = EINVAL;
		} else {
			ctinfo->ct_state2 |= CT_BAD_REQT;
		}

		goto anerror;

	/* Hardware Error:
	 *   Non-recoverable hardware error (parity, etc...). We just
	 *   punt!!!! */
	case SC_HARDW_ERR:
		ctalert(ctinfo, "Hardware error, Non-recoverable");
		if ( !(ctinfo->ct_state2 & CT_LOAD_ERR) )
			ctinfo->ct_state2 |= CT_HWERR;
		goto anerror;

	/* Aborted Command:
	 *   Drive aborted the command, the initiator could try
	 *   again, but we don't! */
	case SC_CMD_ABORT:
		ctreport(ctinfo, "Command aborted by target -- ");
		error = -SC_CMD_ABORT;
		ctinfo->ct_state2 |= CT_HWERR;
		goto anerror;

	/* Media Error:
	 *   Associated with the READ, WRITE, WFM and SPACE commands.
	 *   Cannot read, write or space over data (bad data), or
 	 *   command hit EOM.  Some devices, notable DLT, NTP, STK, "may" 
 	 *   return MEDIUM ERROR when EOT is encountered. Not really a MEDIUM
 	 *   error -- should be returned VOLUME OVERFLOW according to SCSI2
 	 *   spec. */
	case SC_MEDIA_ERR:
		ctinfo->ct_state |= CT_MEDIA_ERR;
  		if(esdptr->esd_eom && esdptr->rsscsi2.as==0 && esdptr->rsscsi2.aq==2) {
  		  switch(ctinfo->ct_typ.tp_hinv) {
  		  case TPDLT:
  		  case TPDLTSTACKER:
  		  case TPNTP:
  		  case TPNTPSTACKER:
                  case TPMGSTRMP:
                  case TPMGSTRMPSTCKR:
  		  case TPSTK9490:
  		  case TPSTKSD3:
		  case TPSTK9840:
        	  case TPNCTP:
                  case TPTD3600:
                  case TPOVL490E:
  		    ctinfo->ct_state |= CT_EOM;
		    ctinfo->ct_state &= ~CT_MEDIA_ERR;
  		    if (residbytes) {
  		      /* note that b_resid could be > b_count,
  		       * due to buffering */
  		      if(((opcode == WRITE_CMD && residbytes < (int)ctinfo->ct_req->sr_buflen) ||
  			  (opcode == READ_CMD && residbytes <= (int)ctinfo->ct_req->sr_buflen)) &&
  			 ctresidOK(ctinfo))
  			bp->b_resid = residbytes;
  		      else {
  			error = ENOSPC;
  			goto anerror;
  		      }
  		    }
  		    break;
  		  default:
  		    ctalert(ctinfo, "Unrecoverable media error");
  		    goto anerror;
  		  }
  		}
  		else {
  		  ctalert(ctinfo, "Unrecoverable media error");
  		  goto anerror;
  		}
  		break;

	/* some vendors report by default (modesel can change); some vendors
	 * report on specific kinds of errors regardless. */
	case SC_ERR_RECOVERED:
		/* count recoverable errors (may be overridden by code above
		 * on later call to this routine) */
		ctinfo->ct_recov_err++;
		/* Kennedy gives this on some occasions, even when switches
		 * set correctly (to not report recovered errors).  If
		 * deferred is set, command hasn't yet been executed,
		 * so simply re-issue it from ctcmd. */
		if(esdptr->esd_defer) {
			ct_dbg((C, "re-issue cmd; defer set. "));
			return -(int)sensekey;
		}
		break;

	/* Data Protect:
	 *   A command was issued, usually write, and the tape was
	 *   write protected.  This is a driver error, because during
	 *   the open (for writing) we confirm that write protect is off.
	 *   For now, on viper 2150 this happens when we try to write a
	 *   low density (310 oersted) QIC24 tape;  set bit indicating
	 *   it for the GET ioctl; See comments at end of ctmodesense()
	 *   To ensure the writeprotect didn't get missed we do
	 *   another modesense here to be sure.  Sort of kludgy, but it
	 *   happens rarely, and only on fatalerrors, so not too bad...  */
	case SC_DATA_PROT:
		if(IS_VIPER60(ctinfo) || IS_VIPER150(ctinfo) ||
		    IS_TANDBERG(ctinfo)) {
			if(!(ctinfo->ct_state & (CT_QIC24|CT_WRP))) {
			    buf_t sbp;
			    /* we don't know which; go find out, have to
			     * preserve bp contents first */
			    bcopy(bp, &sbp, sizeof(sbp));
			    if(!ctcmd(ctinfo, ctmodesens, MAX_MODE_SEL_DATA,
				    SHORT_TIMEO, 0, 0) &&
				    !(ctinfo->ct_state & CT_WRP)) {
				    ctinfo->ct_state |= CT_QIC24;
			    }
			    bcopy(&sbp, bp, sizeof(sbp));
			}
			if(!(ctinfo->ct_state & CT_WRP))
			    ctinfo->ct_state |= CT_QIC24;
			error = ctinfo->ct_state&CT_QIC24 ? EINVAL :
			    EROFS;
		}
		else  {
			ct_dbg((C, "WRP, but not WP in modesense.  "));
			error = EROFS;
		}
		break;

	/* Indicates that the tape cannot be accessed, usually
	 *  offline or no tape in drive.  ctsetup() treats this
	 *  error specially on a tstunitready.  audio mode is
	 *  NOT cleared, audio media is.
	 *  Exception: Sony AIT and DDS3 DAT can return an indication
	 *  that they are coming ready (after rewind), and this should
	 *  be treated as being busy.  Perhaps this shouldn't be
	 *  handled device specifically?
	 */
	case SC_NOT_READY:
		if ((IS_SONYAIT(ctinfo) || IS_DATTAPE(ctinfo))
		 && esdptr->asd._rsscsi2.as==4 && esdptr->asd._rsscsi2.aq==1) {	/* coming ready */
			return -SC_NOT_READY;
		}
		ctinfo->ct_state &= ~CHGSTATES;
		ctinfo->ct_state2 |= CT_NOT_READY;
		ctinfo->ct_tent_state = 0;	/* clear any pending state */
		error = EAGAIN;
		break;

	/* Unit Attention:
	 *   Indicates that the cartridge has been changed or the
	 *   drive reset. ctcmd will normally retry the cmd.
	 *	Always indicate a media change, regardless of cmd;
	 *	if there has been a bus reset, many drives will forget
	 *	their parameters, require re-load, etc.
	 *  audio mode is NOT cleared, audio media is. */
	case SC_UNIT_ATTN:
		ctinfo->ct_state &= ~CHGSTATES;
		ctinfo->ct_tent_state = 0;	/* clear any pending state */
		ctinfo->ct_state |= CT_CHG;
		if(opcode == SPACE_CMD || opcode == WRITE_CMD ||
			opcode == READ_CMD || opcode == WFM_CMD) {
			/* do NOT retry these; since we don't know
			 * for sure if we are on same tape or at same
			 * place */
			error = EIO;
			goto anerror;
		}
		return -SC_UNIT_ATTN;

	/* Blank Check:
	 *   Associated with READ and SPACE commands.
	 *   could be either No data or
	 *   Logical Endof Media (or BOT if going backwards).
	 *   Also get this on seek block if block would be past EOD. */
	case SC_BLANKCHK:
		if(!(ctinfo->ct_state & CT_FM)) {	/* not at a FM */
			ct_dbg((C, reqcnt<0 ? "BOT.  ":"EODATA\n"));
			if(reqcnt<0) {	/* space back, at BOT */
				ctinfo->ct_state &= ~BACKSTATES;
				ctinfo->ct_state |= CT_BOT;
			}
			else /* spaced forwards */
				ctinfo->ct_state |= CT_EOD;
		}
		if(opcode == SPACE_CMD && cmd0 == SPACE_EOM_CODE &&
			(ctinfo->ct_state & CT_EOD))
			break;	/* being at EOD on a space to eom is OK! */
		else if(residbytes) {
			if(opcode == READ_CMD && residbytes < 
				(int)ctinfo->ct_req->sr_buflen && ctresidOK(ctinfo))
				/* note that b_resid could be > b_count, due
				 * to buffering */
				bp->b_resid = residbytes;
			else {
				error = ENOSPC;
				goto anerror;
			}
		}
		/* else not an error if no resid, error will occur on next op */
		break;
	/* Volume Overflow:
	 *   Associated with WRITE and possibly? WFM commands */
	case SC_VOL_OVERFL:
		if(esdptr->esd_eom || IS_CIPHER(ctinfo)) { /* check for EOM */
			/* EOM: physical or logical EOM or BOT if backwards.
			 * Since we don't support read/write reverse, we don't
			 * need to worry about the BOT case here, but code it
			 * right just in case.  */
			ct_dbg((C, reqcnt<0 ? "BOT.  ":"EW\n"));
			if(reqcnt<0) {	/* back motion, at BOT */
				ctinfo->ct_state &= ~BACKSTATES;
				ctinfo->ct_state |= CT_BOT;
			}
			else switch(ctinfo->ct_typ.tp_hinv) {
			    case TPD2:
			    case TPQIC24:
			    case TPQIC150:
			    case TP8MM_8200:
			    case TP8MM_8500:
			    case TP8MM_8900:
				/* really does mean phys EOM for these.  For
					* 9track and DAT it means logical eot.
					* CT_EW was set above. */
				ctinfo->ct_state |= CT_EOM;
			}
			if(residbytes) {
				/* note that b_resid could be > b_count,
				 * due to buffering */
				if(opcode == WRITE_CMD &&
					residbytes <= (int)ctinfo->ct_req->sr_buflen
					&& ctresidOK(ctinfo))
					bp->b_resid = residbytes;
				else {
					error = ENOSPC;
					goto anerror;
				}
			}
			else if(esdptr->esd_defer) {
				/* this happens on deferred error reporting on
				 * writes, when the data was all written, but
				 * EOM was encountered.  So far only seen on
				 * Kennedy */
				error = ENOSPC;
				goto anerror;
			}
			/* else resid==0 means i/o ok, get error on next i/o */
		}
		else
			ct_dbg((C, " EOM not set!.  "));
		break;

	/* No Sense:
	 *   Associated with READ, WRITE, WFM and SPACE commands.
	 *   Check condition occured because of the state of
	 *   the FM or EOM or ILI bits or no sense is available.
	 *   Also because of deferred error reporting on Kennedy.
	 *   if because eom, it's logical eom (CT_EW) */
	case SC_NOSENSE:
		if(esdptr->esd_eom) {
			ct_dbg((C, reqcnt<0 ? "BOT.  ":"EOM\n"));
			/* EOM means logical EOM; or BOT if back.  */
			if(reqcnt<0) {	/* back motion, at BOT */
				ctinfo->ct_state &= ~BACKSTATES;
				ctinfo->ct_state |= CT_BOT;
			}
			else if(opcode == SPACE_CMD && cmd0 == SPACE_EOM_CODE &&
				(ctinfo->ct_state & CT_EOD)) {
				break;	/* at EOD on a space to eom is OK! */
			}
			if(residbytes) {
				/* not an error! otherwise program has no
				 * way to know that some of the i/o was done.
				 * NOTE: resid returned may be greater than
				 * most recent request when buffered mode
				 * is enabled!  For compat with 3.1 releases
				 * this is treated as an error for all except
				 * Kennedy (for multi-vol)
				 * note that b_resid could be > b_count,
				 * due to buffering; if *writing*, and resid==cnt
				 * make it ENOSPC, or some programs get confused;
				 * still need old behavior of read ret 0 though!
				 * theoretically, in the NOSENSE case, we hit EOM,
				 * and all of the drives that support buffering are
				 * supposed to guarantee that the previous write did
				 * in fact make it; if it didn't we lose, and aren't
				 * sure what happened, but so far we don't really
				 * see this... */
				if(((opcode == WRITE_CMD && residbytes <
					(int)ctinfo->ct_req->sr_buflen) ||
				    (opcode == READ_CMD &&
					residbytes <=
					(int)ctinfo->ct_req->sr_buflen)) &&
				    ctresidOK(ctinfo))
					bp->b_resid = residbytes;
				else {
					error = ENOSPC;
					goto anerror;
				}
			}
			else if(esdptr->esd_defer) {
 				/* this happens on deferred error reporting
  				 * on writes, when the data for the
  				 * PREVIOUS write was all written, but EOM
  				 * was encountered.  So far only seen on
  				 * Kennedy.  The current command wasn't
  				 * even done in this case.  Also seen on
  				 * STK, when hitting EW. Since no sense
  				 * key, no error in that case since it's
  				 * only telling us that the previous
  				 * command actually saw the EW and block was
  				 * in fact successfully written to tape.*/
  			        if (IS_KENNEDY(ctinfo)) {
  				  error = ENOSPC;
  				  goto anerror;
  				}
			}
			/* else resid==0 means i/o ok, get error on next i/o */
		}
		/* If FM is set, we just mark that we hit a FM and update the
		 * residual count (if any) and higher level stuff handle it.  */
		else if(esdptr->esd_fm)
			bp->b_resid = residbytes;
		else if(esdptr->esd_ili) {
			ct_dbg((C, "ILI.  "));
			if(esdptr->esd_valid) {
				/* resid < 0 indicates read request less than
				 * size of block on tape.  The next read starts
				 * at the beginning of the next block, so data
				 * will be skipped over.  We return a unique
				 * error so programs that care can tell:
				 * "Not enough space" is the perror() msg.
				 *
				 * resid > 0 will return a short count, which is
				 * how programs like tar traditionally get the
				 * block size of a tape.  set b_private to remainder
				 * even on error, if doing getblklen code */
				if(ctinfo->ct_state & CT_GETBLKLEN) {
					bp->b_private = (void *) (__psint_t)residbytes;
					ct_dbg((C, "set remain = %d. ", (__psint_t)bp->b_private));
				}
				if(residbytes < 0) {
					ctinfo->ct_state2 |= CT_LARGE_BLK;
					error = ENOMEM;
					ct_shortili(ctinfo, reqcnt, residbytes);
					goto anerror;
				}
				else
					bp->b_resid = residbytes;

				if ( !ISVAR_DEV(bp->b_edev) )
					ctinfo->ct_state2 |= CT_BLKLEN;
			}
			else
				ct_badili(ctinfo, reqcnt);
		}
		else {
			/* If the FM or EOM or ILI bits are not set, we got a
			 * NO SENSE code and we don't know why; ignore it */
			ctreport(ctinfo,
			    "\"Check Condition\" with unknown cause\n");
		}
		break;

	case SC_VENDUNIQ:
		/*for Exabyte, this can happen under some circumstances
		 * during a space; if xfr bit is set it's not recoverable,
		 * if tmd is set we could recover;  we don't bother since
		 * this is so unusual, it would be hard to test the
		 * recover code. */
		ct_dbg((C, "Vendor unique ERROR on %s.  ", cmdnm(opcode)));
		goto anerror;
		
	default:
		ctreport(ctinfo, "Unexpected sense key %s (%x)\n",
			scsi_key_msgtab[sensekey], sensekey);
		goto anerror;
	}	/* end switch on sense key	*/

	ct_cxdbg(error, 1, (C, "ctchkerr: ERR=%x ", error));
	return error ? error : 0;
anerror:
	if(!error)
		error = EIO;
	ct_cxdbg(error, 1, (C, "anerror: ERR=%d ", error));
	return error;
}


/* Try to read 4 bytes in variable mode.  When done, we
 * subtract the residual which is saved as a special case
 * in ctchkerr(). from 4 to get the actual blocksize.
 * Shouldn't be called if we have already started writing.
 * Return -errno on any errors, and 0 if we can't/shouldn't try
 * to do anything.  Saves and restores all ct_state
 * in case they are going to be writing, etc.  Only exception is
 * if we don't think we got back to where we started.
*/
static
ctgetblklen(ctinfo_t *ctinfo)
{
	buf_t	*bp;
	ulong save_blksz, save_state;
	ushort fm_or_bot;
	int blocksize, isvar, error = 0;
	dev_t save_dev;
	ulong save_min;
	/* REFERENCED */
	graph_error_t		rv;

	/* Fail if we've written, or if at EOM, or not opened for reading,
	 * or if variable mode not supported */
	if((ctinfo->ct_state & (CT_WRITE|CT_EOM|CT_EW|CT_EOD)) ||
		!(ctinfo->ct_openflags & FREAD) ||
		!(ctinfo->ct_typ.tp_capablity&MTCAN_VAR))
		return 0;

	if((error=ctloaded(ctinfo)) || !(ctinfo->ct_state & CT_ONL))
		return error ? -error : -EAGAIN; /* load failure, or offline */

	bp = ctinfo->ct_bp;
	isvar = ISVAR_DEV(bp->b_edev);

	/* Some drives, like Kennedy, can only switch to var mode
	 * at BOT */
	if(isvar || (ctinfo->ct_typ.tp_capablity&MTCAN_CHTYPEANY))
		fm_or_bot = CT_BOT|CT_FM;
	else
		fm_or_bot = CT_BOT;

	/* check to be sure loaded first, then must be at FM or BOT */
	if(!(ctinfo->ct_state & fm_or_bot)) {
		ct_dbg((C, "no len because not at BOT/FM.  "));
		return -1;
	}

	save_blksz = ctinfo->ct_blksz;

	/* everything but position related info */
	save_state = ctinfo->ct_state & ~CTPOS;
	fm_or_bot = ctinfo->ct_state & (CT_BOT|CT_FM);

	ctinfo->ct_state |= CT_GETBLKLEN;
	ctinfo->ct_blksz = 1;

	/* be sure in variable mode, and sili bits */
	if(error = ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0, 1)) {
		ctinfo->ct_state &= ~CT_GETBLKLEN;
		goto restore;
	}

	save_dev = bp->b_edev;
	save_min = ctinfo->ct_cur_minsz;
	bp->b_dmaaddr = (caddr_t)ctinfo->ct_blklenbuf;
	/* same basic setup that *physio() do */
	bp->b_flags = B_READ|B_BUSY;	/* !done for assert in fs_bio.c */
	ctinfo->ct_cur_minsz = bp->b_bcount = sizeof(ctinfo->ct_blklenbuf);
	bp->b_resid = 0;
	/* If the current device is not in variable-block
	 * mode try to move to th state machine vertex which
	 * corresponds to the variable-block device mode
	 */
	if (!isvar) {
		rv = hwgraph_traverse(hwgraph_connectpt_get(save_dev),
				      CT_SM_VAR,
				      &(bp->b_edev));
		ASSERT(rv == GRAPH_SUCCESS); rv = rv;
		hwgraph_vertex_unref(bp->b_edev);
	}
	ctstrategy(bp);
	error = bp->b_error;

	bp->b_edev = save_dev;
	ctinfo->ct_cur_minsz = save_min;
	ctinfo->ct_state &= ~CT_GETBLKLEN;

	if(error && (!ctinfo->ct_es_data->esd_ili ||
		ctinfo->ct_es_data->esd_sensekey != SC_NOSENSE)) {
		ct_dbg((C, "strat ERROR, other than ili/no sense "
			"(sensekey %d)  ", ctinfo->ct_es_data->esd_sensekey)); 
		blocksize = -1;
		goto getblklen_err;
	}
	else error = 0;	/* the error we expected */

	/* we use b_private here, because ctxfer sets b_resid to sr_resid */
	blocksize = sizeof(ctinfo->ct_blklenbuf) - (__psint_t)bp->b_private;
	ct_xdbg(1, (C, "block size is %d (%x).  ", blocksize, blocksize));

	if(blocksize <= 0)	/* can happen if we hit fm with resid==cnt */
		blocksize = -1;	/* so leave it undefined */
	else {
		if(!isvar) {
			/* if in fixed mode, set the blksize for further i/o. 
			 * Note that this may be reset if writing */
			ct_dbg((C, "fixed blksiz set to %d.  ", blocksize));
			save_blksz = blocksize;
			/* reset recommended size to ensure it's a multiple */
			ctinfo->blkinfo.recblksz /= blocksize;
			ctinfo->blkinfo.recblksz *= blocksize;
		}
		else
		        ctinfo->blkinfo.recblksz = ctinfo->ct_typ.tp_recblksz;
		ctinfo->blkinfo.curblksz = blocksize;
	}

getblklen_err:	/* try to reposition to same place */
	if (IS_DST(ctinfo) && (ctinfo->ct_state2 & CT_INV_POS)) {
		/* No reposition is possible if DST tape and position 
		 * not established.
		 */
		;
	} else if(fm_or_bot & CT_BOT)
		error = ctcmd(ctinfo, ctrewind, 0, REWIND_TIMEO, 0, 1);
	else {
		/* We space back to prev fm, then
		 * forward over it.  This leaves FM correctly set, 
		 * DIDIO cleared, and us on the 'correct' side of the FM.
		 * If we just spaced back, and then cleared DIDIO, the code
		 * in MTFSF wouldn't work correctly, if someone happened
		 * to do an MTFSF after the getblklen.
		 * BSR would be better, but I don't want to test to be
		 * sure all the drives do it right and set the correct
		 * status flags in the sense routines.  If we weren't at
		 * a FM before, and we are now, then we must have hit it
		 * while trying the read, so we want to remain on the BOT side!
		 */
		error = ctcmd(ctinfo, ctspace, SPACE_FM_CODE,
			SPACE_TIMEO, (uint)-1, 1);
		if((fm_or_bot & CT_FM) || !(ctinfo->ct_state & CT_FM)) {
		    int err = error;
		    error = ctcmd(ctinfo, ctspace, SPACE_FM_CODE,
			SPACE_TIMEO, 1, 1);
		    if(err && !error) error = err;
		}
		/* else we were on the BOT side of a FM before, so we want
		 * to stay there */
	}

restore:	/* restore changed fields */
	ctinfo->ct_blksz = save_blksz;

	/* be sure in correct mode and sili bits */
	isvar = error;
	error = ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0, 1);
	if(!error && isvar) error = isvar;
	ctinfo->ct_state &= ~CT_READ;	/* forget read, but keep
		position/offline, etc */
	ctinfo->ct_state |= save_state;
	return error ? -error : blocksize;
}

/* fake a space to EOM.  Some drives, like the Exabyte, do not
 * support a direct space to EOM.  For Exabyte, this doesn't
 * work correctly if already at the FM at EOD as would be
 * used by MTAFILE, instead, you get TMD errors.
*/
static
ct_fake_eod(ctinfo_t *ctinfo)
{
	int error;

	do {
		/* Try to space block.  If blank check, we are
		 * at end of data.  If not, go to next filemark.
		 * and check for blank check there.  */
		error = ctcmd(ctinfo, ctspace, SPACE_BLKS_CODE,
			XFER_TIMEO(ctinfo->ct_blksz), 1, 1);
		if((ctinfo->ct_state & (CT_EOM|CT_EW|CT_EOD|CT_FM)) == CT_FM)
			continue;	/* while we are at a FM, skip over it
				* with space block so we get blank check,
				* not an error */
		if(!error && !(ctinfo->ct_state & (CT_EOM|CT_EW|CT_EOD)))
		    error = ctcmd(ctinfo, ctspace, SPACE_FM_CODE,
			SPACE_TIMEO, 1, 1);
	} while(!error && !(ctinfo->ct_state & (CT_EOM|CT_EW|CT_EOD)));

	if(ctinfo->ct_state & (CT_EOM|CT_EW|CT_EOD))
		return 0;	/* whether error set or not */
	ct_dbg((C, "fake space EOD fails; state %x, err %d.  ",
		ctinfo->ct_state, error));
	return error;
}


/* return current block number.  Many drives don't support this, so
 * ignore errors, and return 0 if cmd fails
 * Tandberg and Archive (qic and DAT) are the only ones that support
 * this at the moment, and both return 3 bytes of blkno.
 * Note that in scsi2 the equivalent is read position, code 0x34,
 * and that the return values are quite different.

 * Can almost fake it with Exabyte by using the exab.left field,
 * but apparently the drive ucode only updates it every so often;
 * or more likely it doesn't account for the data in it's buffer.
*/
static
ctgetblkno(ctinfo_t *ctinfo)
{
	uint blkno = 0;
	int err = 0;

	if(ctinfo->ct_scsiv >= 2 || (IS_STK47XX(ctinfo))) {
	    if(!(err=ctcmd(ctinfo, ctreadposn, sizeof(ctinfo->ct_posninfo),
		SHORT_TIMEO, 0, 1))) {
#ifdef SCSICT_DEBUG
 	      if(ctdebug>3) {
 		unsigned char *p = (unsigned char *)ctinfo->ct_posninfo;
 		int i;
 		printf("readposndata: ");
 		for (i = 0; i < 8; ++i)
 		  printf("%x ", *p++);
 		printf("\n");
 	      }
#endif
		if(!(ctinfo->ct_state&CT_AUDIO)) {
		    if(!(ctinfo->ct_posninfo[0]&4)) {	/* posn known */
			blkno = (ulong)ctinfo->ct_posninfo[4]<<24;
			blkno |= (ulong)ctinfo->ct_posninfo[5]<<16;
			blkno |= (ulong)ctinfo->ct_posninfo[6]<<8;
			blkno |= (ulong)ctinfo->ct_posninfo[7];
		    }
		    /* we set bot/eot if at bop/eop for any partition */
		    if(ctinfo->ct_posninfo[0]&0x80)
			ctinfo->ct_state |= CT_BOT;
		    if(ctinfo->ct_posninfo[0]&0x40)
			ctinfo->ct_state |= CT_EW;
		    if(!(IS_STK47XX(ctinfo)) &&
		       ctinfo->ct_typ.tp_capablity&MTCAN_PART)
			ctinfo->ct_partnum = ctinfo->ct_posninfo[1];
		}
		/* else: aud mode returns BCD vals, program decodes,
		 * included BB for BOT, AA for invalid, and EE for EOD */
	    }
	}
	else {
	    if(!(err=ctcmd(ctinfo, ctreqaddr, 3, SHORT_TIMEO, 0, 1))) {
		blkno = (ulong)ctinfo->ct_blklenbuf[0]<<16;
		blkno |= (ulong)ctinfo->ct_blklenbuf[1]<<8;
		blkno |= (ulong)ctinfo->ct_blklenbuf[2];
	    }
	}

        if (err) {

                /* Somehow AUDIO tape wants the value of the err
                   so we pass it back. The rest of the code
                   does not care.
                */

                if (ctinfo->ct_typ.tp_capablity&MTCAN_AUDIO)
                        return -err;
                else
                        return 0;
        }

	return blkno;
}


/*
 *	Issue the request block address request.  Note that many tape
 *	drives don't support it.  Used by cgetblkno above
*/
static void
ctreqaddr(ctinfo_t *ctinfo, int flag, __psunsigned_t arg)
{
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g0cdb.g0_opcode = REQ_BLKADDR;
		ctinfo->ct_g0cdb.g0_cmd3 = arg;
		ctinfo->ct_req->sr_buflen = arg;
		ctinfo->ct_req->sr_flags |= SRF_DIR_IN;
		ctinfo->ct_req->sr_buffer = ctinfo->ct_blklenbuf;
		bzero(ctinfo->ct_req->sr_buffer, arg); /* so 0's on errs */
		dki_dcache_wbinval(ctinfo->ct_req->sr_buffer, arg);
	}
}


/*
 *	Issue the request position command.
 *	DAT, AIT, and other drives support this command.
*/
static void
ctreadposn(ctinfo_t *ctinfo, int flag, __psunsigned_t arg)
{
	if(flag == CTCMD_SETUP) {
		ctinfo->ct_g2cdb.g2_opcode = READ_POS_CMD;

		/*
		** Set BT if in audio mode or device uses it to
		** implement fast seeks (or other TBD reasons).
		*/

		if (ctinfo->ct_typ.tp_capablity&MTCAN_FASTSEEK
		 || ctinfo->ct_state&CT_AUDIO) {
			ctinfo->ct_g2cdb.g2_cmd0 = 1;	/* set BT bit */
		}

		ctinfo->ct_req->sr_buflen = arg;
	    	ctinfo->ct_req->sr_buffer = ctinfo->ct_posninfo;
		ctinfo->ct_req->sr_flags |= SRF_DIR_IN;
		bzero(ctinfo->ct_req->sr_buffer, arg); /* so 0's on errs */
		dki_dcache_wbinval(ctinfo->ct_req->sr_buffer, arg);
	}
}


/*
 *  Fujitsu Diana-1 and -2 routine to disable the drive from performing
 *   retries of data transfer operations by way of disconnect/reconnect
 *   and the Restore Pointers message.  This is because Restore Pointers
 *   messages are not correctly handled in the low-level WD95 SCSI code.
 *   The QLogic code was not checked re: the Restore Pointers message
 *   since this routine removes the need for it to work, and the Fujitsu
 *   drives actually require the handling of this message a very small
 *   percentage of the time when the Restore Pointers message is enabled.
 *  The reason this message would ever even be needed is that, during a
 *   write operation, the drive is doing compression on the fly (unless
 *   it's a Diana-1 without compression).  If the drive runs into a data
 *   stream *   that causes the compression method to have to back-up and
 *   try another algorithm, the drive may have to have the host re-transmit
 *   some of the data.  This is most efficiently effected by telling the
 *   host to Restore its Data Pointers, i.e., back up to the last point at
 *   which it received Save Data Pointers from the drive.  However, this
 *   method will not work if this message is not properly implemented by
 *   the host, as is the case with at least the WD95 code.
 *  The alternative to this method is for the drive to conclude the command
 *   with an ABORTED COMMAND Sense Key, and have the host re-issue the
 *   command.  The Fujitsu drive will then expect the data stream
 *   accompanying the next WRITE command to require use of the alternate
 *   compression method.
 *  The handling of this ABORTED COMMAND Sense Key for the Fujitsu drive is
 *   in ctcmd() following the string "Fujitsu returns ABORTED COMMAND".
 */
static int
fuj_drrp(ctinfo_t *ctinfo)
{
#define FUJ_VUNIQ_PNO	0	/* Fujitsu Vendor Unique Params Mode Page no. */
#define DRRP_OFF	3	/* Dis/Recon,RestorePtrs Bit Mode Page Offset */
#define DRRP_BIT	3	/* discon/reconnect and restore pointers bits */

	int err;
    /* Vendor Unique page lengths for Diana-1 and Diana-2 */
	int vuplen = IS_FUJD1(ctinfo) ? 10 : 12;

    /* get Fujitsu Vendor Unique Parameters Mode Page with Mode Sense */
	if (err = ctcmd(ctinfo, ctmodesens, MSD_IND_SIZE+vuplen,
	    SHORT_TIMEO, FUJ_VUNIQ_PNO, 1))
	{
	    return err;
	}

    /* clear Dis/Recon and RestorePtrs bits in Mode Page and do Mode Select */
	bcopy(ctinfo->ct_ms.msd_vend, ctinfo->ct_msl.msld_vend, vuplen);
	ctinfo->ct_msl.msld_vend[0] &= 0x7F;	/* clear PS bit */
	ctinfo->ct_msl.msld_vend[DRRP_OFF] &= 0xFC; /* clear D/R and RP bits */
	if (err = ctcmd(ctinfo, ctmodesel, vuplen, SHORT_TIMEO, 0, 1))
	{
	    return EINVAL;
	}

    /* verify new Dis/Recon and Restore Pointers bits with Mode Sense */
	if (err = ctcmd(ctinfo, ctmodesens, MSD_IND_SIZE+vuplen,
	    SHORT_TIMEO, FUJ_VUNIQ_PNO, 1))
	{
	    return err;
	}
	if (ctinfo->ct_ms.msd_vend[DRRP_OFF] & DRRP_BIT)
	{
	    return EIO;
	}
	return 0;
}


/* check to see if we the tape is loaded and online.
 * If not, load it now.  We do this in ctstrategy, and in ctioctl
 * for cmds that(might) move the tape, or 'assume' the
 * tape is already loaded.  This used to be done in
 * open, but drives like the Exabyte and the Kennedy
 * take a long time to load and unload, so it is done
 * only when first needed.
 * May be marked offline before this, and online after.
 * Return 0 on success, error number on errors
*/
static
ctloaded(ctinfo_t *ctinfo)
{
	int error = 0;
	if((ctinfo->ct_state & (CT_MOTION|CT_ONL)) != (CT_MOTION|CT_ONL)) {
		ct_dbg((C, "loading tape st%x.  ", ctinfo->ct_state));
		if (ctinfo->ct_typ.tp_capablity & MTCANT_LOAD) {
		    if (ctinfo->ct_typ.tp_capablity & MTCAN_LDREW) {
			if(error=ctcmd(ctinfo, ctrewind, 1, REWIND_TIMEO, 0, 1)) {
				ct_dbg((C, "tape load (rewind) failed.  "));
					return error;
			}
			ctinfo->ct_state |= CT_ONL|CT_LOADED|CT_BOT;
			ctinfo->ct_state &= ~(CT_MOTION);
		    }
		}
		else 
		{
		    if(error=ctcmd(ctinfo, ctload, L_UL_LOAD, REWIND_TIMEO, 0, 1)) {
			ct_dbg((C, "tape load failed.  "));
			return error;
		    }
		}
		ct_dbg((C, "tape load OK.  "));

		/* Do this after the tape is loaded for two reasons; it may be
		 * a nop for some drives if no media was present on open, and
		 * for apps like datman that hold the drive open for a long time,
		 * and MTUNLOAD tapes, need to do every time a new tape is inserted.
		 */
		if(ctinfo->ct_typ.tp_capablity&MTCAN_PREV)
		    /* turn on the front panel LED; disable tape removal for
		     * those with software controllable eject */
		    ctcmd(ctinfo, ctprevmedremov, CT_PREV_REMOV,
			SHORT_TIMEO, 0, 1);

		/* do modesel after load to set dens, etc.  If tape
		 * wasn't loaded, the modesel in open fails for some
		 * drives, such as Kennedy and Cipher 540.  Always do a
		 * sense first, so things like audio vs non-audio don't
		 * get reset by a blind modeselect! Do just before audio junk
		 * below so sense data in union still valid for either modesel
		 * if we support partitioning, do the modesense to check for
		 * that (still gets the header stuff, density, etc.), else
		 * do the simple modesense.
		 */
		if(ctinfo->ct_typ.tp_capablity&MTCAN_PART) {
		    /* check to see if it is partitioned whenever we load it. */
		    error = ctcmd(ctinfo, ctmodesens, MSD_IND_SIZE+0xa,
			SHORT_TIMEO, 0x11, 1);
		    ctinfo->ct_state &= ~CT_MULTPART; /* in case of failures */
		    if(!error && ctinfo->ct_ms.msd_vend[3])
			    ctinfo->ct_state |= CT_MULTPART;
		}
		else
		    error = ctcmd(ctinfo, ctmodesens, MAX_MODE_SEL_DATA,
			SHORT_TIMEO, 0, 1);
		if(error)
			return error;	/* shouldn't happen... */

		if(IS_DATTAPE(ctinfo)) {
		    /* this is to try and find out whether we are dealing with
		     * audio or data mode tapes, because some firmware revs
		     * (even 2.67 sometimes) get the mode wrong, and nothing
		     * seems to correct it, but by doing this, if we have it
		     * wrong, we will correct it with the code in
		     * ctdrivesense().  Clear audio media state first, so drive
		     * doesn't just give us an illegal request without trying
		     * the command.  Ugh.   Note that this doesn't *completely*
		     * fix the problem, as we get the media type right, but
		     * every now and then, the drive refuses to play the tape
		     * in audio mode.  At least it is detectable though, because
		     * both aud_med and EOD are set, so datman, etc. can pop up
		     * a notifier (ejecting and reloading the tape usually seems
		     * to fix it)  */
		    ctinfo->ct_state &= ~CT_AUD_MED;
		    (void)ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0, 1);
		    ctsuppress = 1;
		    (void)ctcmd(ctinfo, ctspace, SPACE_BLKS_CODE,
			XFER_TIMEO(1), 1, 1);
		    ctsuppress = 0;
		    /* 2nd modesel was done in ctdrivesense for DAT if needed */
		    error = ctcmd(ctinfo, ctload, L_UL_LOAD, REWIND_TIMEO, 0, 1);
		    if(error) {
			ct_dbg((C, "2nd tape load failed!  "));
			return error;	
		    }
		}
		else if(error=ctcmd(ctinfo, ctmodesel, 0, SHORT_TIMEO, 0, 1))
		    return error;

		/* online, loaded, AND modesel done.  set CT_ONL and BOT
		 * here because with kennedy, we sometimes get a unit atn
		 * on the modeselect, if the drive was offline, since
		 * the load changed it from offline to online... */
		ctinfo->ct_state |= CT_MOTION|CT_ONL|CT_BOT;
		ct_dbg((C, "load setup OK.  "));
		if(!ctinfo->ct_blksz) {
			/* remain paranoid to avoid divide by zero errors; shouldn't 
			 * be a problem any more, but... */
			if(ISVAR_DEV(ctinfo->ct_bp->b_edev))
				ctinfo->ct_blksz = 1;
			else
				ctinfo->ct_blksz = ctinfo->ct_fixblksz;
			if(!ctinfo->ct_blksz)
				ctinfo->ct_blksz = 512;
			ct_dbg((C, "ct_blksz was 0, set to %d.  ",ctinfo->ct_blksz ));
		}
		if (IS_DST(ctinfo) && !ctinfo->blkinfo.curblksz)
			ctinfo->blkinfo.curblksz = ctinfo->ct_blksz;
	}
	return error;
}

/*
 * Issue a command to write to the device specific diagnosic console.
 * arg = buffer pointer, arg2 = buffer length
 */
static void
ctfpmsg(ctinfo_t *ctinfo, int flag, __psunsigned_t arg, __psunsigned_t arg2)
{
	struct tpsc_types *ttp = &ctinfo->ct_typ;

	if (flag == CTCMD_SETUP) {
		if (ttp->tp_fpmsg_cdblen == 6 || ttp->tp_fpmsg_cdblen == 10)
		{
			ctinfo->ct_req->sr_cmdlen = ttp->tp_fpmsg_cdblen;
			bcopy(ttp->tp_fpmsg_cdb, (u_char *) &ctinfo->ct_cmd, ttp->tp_fpmsg_cdblen);
			ctinfo->ct_req->sr_buffer = (u_char *)arg;
			ctinfo->ct_req->sr_buflen = arg2;
			dki_dcache_wbinval(ctinfo->ct_req->sr_buffer, arg);
			
			/* 
			 * If tp_fpmsg_datalen is 1 or 2, use the user
			 * specified data length. Otherwise use the
			 * hardcoded value in the CDB and ignore the
			 * user specified value.
			 */
			if (ttp->tp_fpmsg_datalen == 1)
				*(((u_char *)&ctinfo->ct_cmd) + ttp->tp_fpmsg_data_offset) = arg2 & 0xFF;
			else
			if (ttp->tp_fpmsg_datalen == 2)
			{
				*(((u_char *)&ctinfo->ct_cmd) + ttp->tp_fpmsg_data_offset) = 
					(arg2 & 0xFF00) >> 8;
				*(((u_char *)&ctinfo->ct_cmd) + ttp->tp_fpmsg_data_offset + 1) =
					arg2 & 0x00FF;
			}
		}
	}
}

/* We got an ILI on a request sense, but the valid bit
 * wasn't set.  This shouldn't happen!  It does happen
 * with tape block sizes > 160K on the Exabyte,
 * (1/89), and it might happen with others also.
 * Don't report if we were trying to determine
 * the block length for an ioctl or it wasn't a read.
 * Separate routine because indent level was ridculous.
*/
static void
ct_badili(ctinfo_t *ctinfo, int reqcnt)
{
	ct_dbg((C, "ILI, but resid not valid!.  "));
	if(ctinfo->ct_lastcmd!=READ_CMD ||
		(ctinfo->ct_state & CT_GETBLKLEN))
		return;
	ctreport(ctinfo, "blocksize on tape larger than request of %d bytes\n\
But actual blocksize couldn't be determined\n",
		reqcnt *= ctinfo->ct_blksz);
}


/* A read was done with a block size smaller than the current
 * block size on the tape.
 * Don't report if we were trying to determine
 * the block length for an ioctl or it wasn't a read.
 * Separate routine because indent level was ridculous.
 * There is some argument about whether the user should be told.
*/
static void
ct_shortili(ctinfo_t *ctinfo, int reqcnt,  int resid)
{
	if (!tpsc_print_ili)
		return;
	ct_dbg((C, "req blk size too small.  "));
	if(ctinfo->ct_lastcmd!=READ_CMD ||
		(ctinfo->ct_state & CT_GETBLKLEN))
		return;
	ctreport(ctinfo, "blocksize mismatch; blocksize on tape is %d bytes\n",
		(reqcnt - resid) * ctinfo->ct_blksz);
}



/* Here so all errors reported in a standard way. Can't use CE_NOTE because
 * you get extra newlines that way. Can't use protoype because of varargs,
 * and we don't have a vprintf in the kernel.
*/
/*VARARGS2*/
static void
ctreport(ctinfo_t *ctinfo, char *fmt, ...)
{
	vertex_hdl_t	tape_vhdl = SUI_UNIT_VHDL(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(ctinfo->ct_bp->b_edev))));
	inventory_t    *pinv;
	va_list		ap;

	if (ctsuppress)
		return;  

	if ((hwgraph_info_get_LBL(tape_vhdl, INFO_LBL_INVENT, (arbitrary_info_t *)&pinv) == GRAPH_SUCCESS) &&
	    (pinv->inv_controller == (major_t)-1)) 
	{
		cmn_err(CE_CONT, "%v: ", tape_vhdl);
	}
	else
	{ 
		cmn_err(CE_CONT, "tps%dd%d ",
			pinv->inv_controller,
			pinv->inv_unit);
	}
	va_start(ap, fmt);
	icmn_err(CE_CONT, fmt, ap);
	va_end(ap);
}

/* same as ctreport, but for messages "useful/interesting" for users
 * via system monitor stuff; keep fmt short!!!!  */
/*VARARGS2*/
static void
ctalert(ctinfo_t *ctinfo, char *fmt, ...)
{
	vertex_hdl_t	tape_vhdl = SUI_UNIT_VHDL(((scsi_unit_info_t *)TLI_UNIT_INFO(TLI_INFO(ctinfo->ct_bp->b_edev))));
	inventory_t    *pinv;
	va_list		ap;
	char		buf[128];

	void vsprintf(char *buf, char *fmt, va_list ap);

	if (ctsuppress)
		return;  

	if ((hwgraph_info_get_LBL(tape_vhdl, INFO_LBL_INVENT, (arbitrary_info_t *)&pinv) == GRAPH_SUCCESS) &&
	    (pinv->inv_controller == (major_t)-1)) 
	{
		/* 
		 * If controller number is unknown (this should only
		 * happen during boot time) use CE_CONT instead of
		 * CE_ALERT. Otherwise we'd have to allocate an
		 * temporary buffer for the full HWG name to pass to
		 * icmn_err(CE_ALERT) 
		 */
		cmn_err(CE_CONT, "%v: ", tape_vhdl);
		va_start(ap,fmt);
		icmn_err(CE_CONT, fmt, ap);
		va_end(ap);
	}
	else
	{ 
		sprintf(buf, "tps%dd%d ",
			pinv->inv_controller,
			pinv->inv_unit);
		va_start(ap, fmt);
		vsprintf(buf+strlen(buf), fmt, ap);
		va_end(ap);
		cmn_err(CE_ALERT, buf);
	}
}


extern int fastick, fastclock;


/* swap bytes in place.
 * Unroll a bit since we are typically doing many blocks.
 * if count is odd, last byte is left untouched, unlike
 * dd conv=swab.
 * We check for preemption aprox each fastick usecs if fastclock
 * is on, otherwise each sched clock tick.  We may check first time
 * through after fewer iterations, but thats OK; better than the
 * overhead of maintaining 2 counters.
 * These are the approximate swapping rates, as of june 91:
 *	IP4     gets 1.5 Kb/ms
 *	IP6     gets 1.6 Kb/ms
 *	IP7/33  gets 2.5 Kb/ms
 *	IP12    gets 8.5 Kb/ms
 * other new machines enough faster it really doesn't matter...
 * A preempt value of 30 is right for a 33 MHz IP7.  Other MP
 * machines are newer and faster, so we check a bit too often.
 * Tough; most newer systems will use drives other than QIC, where byte
 * swapping should never happen.  Older slower machines (except 25 MHz
 * IP5 MP) were never claimed to support realtime, so it doesn't matter
 * that we check a bit less often.  30 is fine for all the Indigo's as well.
*/
static void
swapbytes(u_char *ptr, unsigned n)
{
#define SW_LOOPUNROLL 16
	unchar tmp;
	int cnt;
	
	cnt = n / SW_LOOPUNROLL;
	n -= cnt * SW_LOOPUNROLL;	/* for resid, if any */
	while(cnt) {
		tmp = ptr[0]; ptr[0] = ptr[1]; ptr[1] = tmp;
		tmp = ptr[2]; ptr[2] = ptr[3]; ptr[3] = tmp;
		tmp = ptr[4]; ptr[4] = ptr[5]; ptr[5] = tmp;
		tmp = ptr[6]; ptr[6] = ptr[7]; ptr[7] = tmp;
		tmp = ptr[8]; ptr[8] = ptr[9]; ptr[9] = tmp;
		tmp = ptr[10]; ptr[10] = ptr[11]; ptr[11] = tmp;
		tmp = ptr[12]; ptr[12] = ptr[13]; ptr[13] = tmp;
		tmp = ptr[14]; ptr[14] = ptr[15]; ptr[15] = tmp;
		ptr += SW_LOOPUNROLL;
		cnt--;
	}

	while(n>1) {	/* needed for both cases */
		unchar etmp = ptr[0]; ptr[0] = ptr[1]; ptr[1] = etmp;
		ptr += 2;
		n -= 2;
	}
}


/*	Save table information between requests.	*/
static void
ctsave( ctinfo_t *ctinfo, ctsvinfo_t *svinfo )
{
	svinfo->ct_reqcnt   = ctinfo->ct_reqcnt;
	svinfo->ct_complete = ctinfo->ct_complete;
	svinfo->ct_state2   = ctinfo->ct_state2;
}


/*	Restore table information. 	*/
static void
ctrestore( ctinfo_t *ctinfo, ctsvinfo_t *svinfo )
{
	ctinfo->ct_reqcnt   = svinfo->ct_reqcnt;
	ctinfo->ct_complete = svinfo->ct_complete;
	ctinfo->ct_state2  |= svinfo->ct_state2;
}


#if _MIPS_SIM == _ABI64
static void
ctconvabi( irix5_mtscsi_rdlog_t *i5io, mtscsi_rdlog_t *io )
{
	io->mtppc   = i5io->mtppc;
	io->mtsp    = i5io->mtsp;
	io->mtpc    = i5io->mtpc;
	io->mtpage  = i5io->mtpage;
	io->mtparam = i5io->mtparam;
	io->mtlen   = i5io->mtlen;
	io->mtarg   = (caddr_t)(__psint_t)i5io->mtarg;
}
#endif /* _ABI64 */
 
/* is this the major number for the stat only device? */
static
ctisstatdev(dev_t dev)
{
	if (!dev_is_vertex(dev))
		return 0;
	return	TLI_INFO(dev)->tli_isstat;
}

/*
 * Position the tape to the specified vendor specific location (DST only).
 * arg is size of position data.
 */
static void
ctsetpos(ctinfo_t *ctinfo, int flag, __psunsigned_t arg)
{
	ct_dbg((C, "ctsetpos() - arg = %d. ", arg));
	if (flag != CTCMD_SETUP)
		return;

	ctinfo->ct_g0cdb.g0_opcode = AMPEX_LOCATE_CMD;
	ctinfo->ct_req->sr_buffer = ctinfo->ct_vpos->pos;
	ctinfo->ct_req->sr_buflen = arg;
	dki_dcache_wb(ctinfo->ct_req->sr_buffer, arg);
}

/*
 * Get the tape position in vendor specific format (DST only).
 * arg indicates which format is required.
 */
static void
ctgetpos(ctinfo_t *ctinfo, int flag, __psunsigned_t arg)
{
	ct_dbg((C, "ctgetpos() - arg = %x. ", arg));
	if (flag != CTCMD_SETUP)
		return;

	ctinfo->ct_g2cdb.g2_opcode = READ_POS_CMD;
	switch (arg) {
		case BLKPOSTYPE:
			break;
		case FSNPOSTYPE:
			/* Set BT field. */
			ctinfo->ct_g2cdb.g2_cmd0 = 0x01;
			break;
		case DISPOSTYPE:
			/* Set TC field. */
			ctinfo->ct_g2cdb.g2_cmd0 = 0x02;
			break;
	}
	ctinfo->ct_g2cdb.g2_vu67 = 0x1;		/* Set Format field. */
	ctinfo->ct_g2cdb.g2_cmd7 = ctinfo->ct_vpos->size;
	ctinfo->ct_req->sr_flags |= SRF_DIR_IN;
	ctinfo->ct_req->sr_buffer = ctinfo->ct_vpos->pos;
	ctinfo->ct_req->sr_buflen = ctinfo->ct_vpos->size;
	bzero(ctinfo->ct_req->sr_buffer, ctinfo->ct_req->sr_buflen); /* so 0's on errs */
	dki_dcache_wbinval(ctinfo->ct_req->sr_buffer, ctinfo->ct_req->sr_buflen);
}

#ifdef BCOPYTEST
/*
 * If BCOPYTEST is defined, then a bcopytest will be run every time an
 * open is attempted on a SCSI tape device.  The drive doesn't have to
 * exist for the test to be run.  'mt -t /dev/mt/tps0d1 exist' would be
 * enough to run the test.  This is useful for determining whether
 * changes to bcopy() are beneficial.
 */
int
bcopytest(void)
{
	int	 s;
	int	 i;
	uint	 ftime, stime;
	uint	 etime;
	uint	 kbytes;
	static char *tpscb;

	if(!tpscb) {
		/* need to kvpalloc, or it won't work on loadable driver... */
		tpscb = kvpalloc((381*IO_NBPP)/NBPP, VM_DIRECT|VM_NOSLEEP, 0);
		if(!tpscb) {
			printf("couldn't kvpalloc 1.5 MB\n");
			return 0;
		}
#ifdef R10000_SPECULATION_WAR	/* start at a known place */
#if MH_R10000_SPECULATION_WAR
                if (IS_R10000())
#endif /* MH_R10000_SPECULATION_WAR */
		dki_dcache_inval(tpscb, (381*IO_NBPP));
#endif
		printf("Using %x for bcopy test base addr\n", tpscb);
	}

	stime = lbolt;
	for (i = 0; i < 300; i++)
		bzero(&tpscb[0], 1024*256);
	ftime = lbolt;
	etime = ftime - stime;
	printf("Bzero (256K) speed was %d kb/s\n", (30000 * 256) / etime);

	stime = lbolt;
	for (i = 0; i < 300; i++)
		bcopy(&tpscb[0], &tpscb[1024*1024], 1024*256);
	ftime = lbolt;
	etime = ftime - stime;
	printf("Bcopy (1MB apart) speed was %d kb/s\n", (30000 * 256) / etime);

	stime = lbolt;
	for (i = 0; i < 300; i++)
		bcopy(&tpscb[0], &tpscb[1024*262], 1024*256);
	ftime = lbolt;
	etime = ftime - stime;
	printf("Bcopy (262K apart) speed was %d kb/s\n", (30000 * 256) / etime);

	stime = lbolt;
	for (i = 0; i < 300; i++)
		bcopy(&tpscb[0], &tpscb[1024*256], 1024*256);
	ftime = lbolt;
	etime = ftime - stime;
	printf("Bcopy (256K apart) speed was %d kb/s\n", (30000 * 256) / etime);

	stime = lbolt;
	for (i = 0; i < 4000; i++)
		bcopy(&tpscb[0], &tpscb[1024*32], 1024*32);
	ftime = lbolt;
	etime = ftime - stime;
	printf("Bcopy (32KB apart) speed was %d kb/s\n", (400000 * 32) / etime);

	stime = lbolt;
	for (i = 0; i < 4000; i++)
		bcopy(&tpscb[0], &tpscb[1024*16], 1024*32);
	ftime = lbolt;
	etime = ftime - stime;
	printf("Bcopy (16KB apart) speed was %d kb/s\n", (400000 * 32) / etime);

	stime = lbolt;
	for (i = 0; i < 4000; i++)
		bcopy(&tpscb[0], &tpscb[1024*8], 1024*32);
	ftime = lbolt;
	etime = ftime - stime;
	printf("Bcopy (8KB apart) speed was %d kb/s\n", (400000 * 32) / etime);

	stime = lbolt;
	for (i = 0; i < 300; i++)
		bcopy((void *)K0_TO_K1(&tpscb[0]), &tpscb[1024*262], 1024*256);
	ftime = lbolt;
	etime = ftime - stime;
	printf("Uncache src Bcopy (262K apart) speed was %d kb/s\n",
		(30000 * 256) / etime);


	stime = lbolt;
	for (i = 0; i < 300; i++)
		bcopy((void *)K0_TO_K1(&tpscb[0]), &tpscb[1024*256], 1024*256);
	ftime = lbolt;
	etime = ftime - stime;
	printf("Uncache src Bcopy (256K apart) speed was %d kb/s\n",
		(30000 * 256) / etime);

#if !_NO_UNCACHED_MEM_WAR		/* cannot do uncached stores */
	stime = lbolt;
	for (i = 0; i < 300; i++)
		bcopy((void *)K0_TO_K1(&tpscb[0]), (void *)K0_TO_K1(&tpscb[1024*262]), 1024*256);
	ftime = lbolt;
	etime = ftime - stime;
	printf("Uncache src+dest Bcopy (262K apart) speed was %d kb/s\n", (30000 * 256) / etime);

	stime = lbolt;
	for (i = 0; i < 300; i++)
		bcopy((void *)K0_TO_K1(&tpscb[0]), (void *)K0_TO_K1(&tpscb[1024*256]), 1024*256);
	ftime = lbolt;
	etime = ftime - stime;
	printf("Uncache src+dest Bcopy (256 apart) speed was %d kb/s\n", (30000 * 256) / etime);


	stime = lbolt;
	for (i = 0; i < 300; i++)
		bcopy(&tpscb[0], (void *)K0_TO_K1(&tpscb[1024*262]), 1024*256);
	ftime = lbolt;
	etime = ftime - stime;
	printf("Uncache dest Bcopy (262K apart) speed was %d kb/s\n", (30000 * 256) / etime);

	stime = lbolt;
	for (i = 0; i < 300; i++)
		bcopy(&tpscb[0], (void *)K0_TO_K1(&tpscb[1024*256]), 1024*256);
	ftime = lbolt;
	etime = ftime - stime;
	printf("Uncache dest Bcopy (256K apart) speed was %d kb/s\n", (30000 * 256) / etime);

	stime = lbolt;
	for (i = 0; i < ((300*256)/4); i++) {
		bcopy(&tpscb[0], (void *)K0_TO_K1(&tpscb[1024*256]), 4096);
		dki_dcache_inval(&tpscb[0], 4096);
	}
	ftime = lbolt;
	etime = ftime - stime;
	printf("Uncache dest Bcopy (256 apart) 4K at a time with cache flush speed was %d kb/s\n",
		(30000 * 256) / etime);

#endif /* !_NO_UNCACHED_MEM_WAR */

	stime = lbolt;
	for (i = 0; i < ((300*256)/4); i++) {
		bcopy(&tpscb[0], &tpscb[1024*256], 4096);
		dki_dcache_inval(&tpscb[0], 4096);
		dki_dcache_inval(&tpscb[1024*256], 4096);
	}
	ftime = lbolt;
	etime = ftime - stime;
	printf("both cache Bcopy (256 apart) 4K at a time with both cache flush speed was %d kb/s\n",
		(30000 * 256) / etime);

#if !_NO_UNCACHED_MEM_WAR		/* cannot do uncached stores */
	stime = lbolt;
	for (i = 0; i < ((300*256)/4); i++) {
		bcopy(&tpscb[0], (void *)K0_TO_K1(&tpscb[1024*262]), 4096);
	}
	ftime = lbolt;
	etime = ftime - stime;
	printf("Uncache dest Bcopy (262K apart) 4K at a time speed was %d kb/s\n",
		(30000 * 256) / etime);

	stime = lbolt;
	for (i = 0; i < ((300*256)/4); i++) {
		bcopy(&tpscb[0], (void *)K0_TO_K1(&tpscb[1024*256]), 4096);
	}
	ftime = lbolt;
	etime = ftime - stime;
	printf("Uncache dest Bcopy (256 apart) 4K at a time speed was %d kb/s\n",
		(30000 * 256) / etime);
#endif /* !_NO_UNCACHED_MEM_WAR */

	return 0;
}
#endif /* BCOPYTEST */

#if _MIPS_SIM == _ABI64
static void
irix5_mt_to_mt(int	cmd,
	       void	*i5dp,
	       void 	*dp)
{
	switch (cmd)
	{
	case MTGETATTR:
	{
		struct irix5_mt_attr     *i5ma_dp = (struct irix5_mt_attr *)i5dp;
		struct mt_attr           *ma_dp = (struct mt_attr *)dp;
		
		ma_dp->ma_name = (char *)(__psint_t)i5ma_dp->ma_name;
		ma_dp->ma_value = (char *)(__psint_t)i5ma_dp->ma_value;
		ma_dp->ma_vlen = i5ma_dp->ma_vlen;

		break;
	}

	case MTFPMESSAGE:
	{
		struct irix5_mt_fpmsg    *i5ma_dp = (struct irix5_mt_fpmsg *)i5dp;
		struct mt_fpmsg          *ma_dp = (struct mt_fpmsg *)dp;

		ma_dp->mm_mlen = i5ma_dp->mm_mlen;
		bcopy(i5ma_dp->default_mm_msg, ma_dp->default_mm_msg, MT_FPMSG_MAX_MSGLEN);

		break;
	}

	}
}
#endif /* _ABI64 */
