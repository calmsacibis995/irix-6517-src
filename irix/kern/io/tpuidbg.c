/**************************************************************************
 *									  *
 * 		 Copyright (C) 1998, Silicon Graphics, Inc		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/********************************************************************************
 *										*
 * tpuidbg.c - Mesa TPU driver idbg routines.					*
 *										*
 ********************************************************************************/

#ident	"$Id: tpuidbg.c,v 1.4 1999/04/29 19:32:28 pww Exp $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/cred.h>
#include <ksys/ddmap.h>
#include <sys/invent.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/ksynch.h>
#include <sys/edt.h>
#include <sys/dmamap.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/param.h>
#include <sys/pio.h>
#include <sys/sema.h>
#include <sys/ddi.h>
#include <sys/idbgentry.h>
#include <sys/time.h>
#include <sys/timers.h>
#include <sys/tpudrv.h>
#include <stdarg.h>

/* ==============================================================================
 *		 Macros
 */

#define	NEW(ptr)	(ptr = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))
#define	VHDL_MAX	16384
#define	LBD(v,n,b)	lfs (v, 10, n, b, ' ')
#define	LZD(v,n,b)	lfs (v, 10, n, b, '0')
#define	LBX(v,b)	lfu (v, 16, (sizeof(v) * 2), b, ' ')
#define	LZX(v,b)	lfu (v, 16, (sizeof(v) * 2), b, '0')


/* ==============================================================================
 *		Externals
 */

extern const char *		tpu_version (void);
extern const char *		tpusim_version (void);

extern tpud_traceb_t *		tpu_global_trace_buffer;
extern tpud_gstats_t		tpu_gstats;
extern const tpu_service_t	tpu_map_xtalk;
extern const tpu_service_t	tpu_map_sim;
extern tpu_list_t		tpu_active;	/* list of active tpus */
extern time_t			time;		/* system seconds since 1970 */


/* ==============================================================================
 *		Globals
 */


/* ==============================================================================
 *		Function Table of Contents
 */

extern const char *	tpuidbg_version (void);

extern void		idbg_tpu_init (void);
static void		idbg_tpu_help (void);
static void		idbg_tpu_vers (void);
static void		idbg_tpu_ls (void);
static void		idbg_tpu_trace (ulong_t);
static void		idbg_tpu_treset (ulong_t);
static void		idbg_tpu_soft (ulong_t);
static void		idbg_tpu_stats (ulong_t);
static void		idbg_tpu_gstats (void);
static void		idbg_tpu_xregs (ulong_t);
static void		idbg_tpu_aregs (ulong_t);
static void		idbg_tpu_d0regs (ulong_t);
static void		idbg_tpu_d1regs (ulong_t);
static void		idbg_tpu_dregs (tpu_dma_regs_t *);
static void		idbg_tpu_lregs (ulong_t);
static void		idbg_tpu_linst (ulong_t);
static void		idbg_tpu_all (ulong_t);

static char *		statename (tpu_statecode_t);
static char *		lfs (__int64_t, int, int, char *, char);
static char *		lfu (__uint64_t, int, int, char *, char);


/* ==============================================================================
 *		Version function
 */

const char *
tpuidbg_version (void)
{
	return ("$Revision: 1.4 $ Built: " __DATE__ " " __TIME__);
}


/* ==============================================================================
 *		IDBG functions
 */

/********************************************************************************
 *										*
 *	idbg_tpu_init: Register idbg entry points.				*
 *										*
 ********************************************************************************/
void 
idbg_tpu_init (void)
{
	idbg_addfunc("tpu",        idbg_tpu_help);
	idbg_addfunc("tpu_vers",   idbg_tpu_vers);
	idbg_addfunc("tpu_ls",     idbg_tpu_ls);
	idbg_addfunc("tpu_soft",   idbg_tpu_soft);
	idbg_addfunc("tpu_stats",  idbg_tpu_stats);
	idbg_addfunc("tpu_gstats", idbg_tpu_gstats);
	idbg_addfunc("tpu_xregs",  idbg_tpu_xregs);
	idbg_addfunc("tpu_aregs",  idbg_tpu_aregs);
	idbg_addfunc("tpu_d0regs", idbg_tpu_d0regs);
	idbg_addfunc("tpu_d1regs", idbg_tpu_d1regs);
	idbg_addfunc("tpu_lregs",  idbg_tpu_lregs);
	idbg_addfunc("tpu_linst",  idbg_tpu_linst);
	idbg_addfunc("tpu_trace",  idbg_tpu_trace);
	idbg_addfunc("tpu_treset", idbg_tpu_treset);
	idbg_addfunc("tpu_all",    idbg_tpu_all);
}


/********************************************************************************
 *										*
 *	idbg_tpu_help: Display a list of tpu idbg commands.			*
 *										*
 ********************************************************************************/
static void
idbg_tpu_help (void)
{
	qprintf (
"\
TPU debugging functions:\n\
tpu             Show TPU debug routines\n\
tpu_ls          Display all the configured tpus\n\
tpu_vers        Display software version levels\n\
tpu_soft        Display a tpu_soft structure\n\
tpu_gstats      Display global statistics\n\
tpu_stats       Display TPU statistics\n\
tpu_xregs       Display TPU xtalk registers\n\
tpu_aregs       Display TPU address translation registers\n\
tpu_d0regs      Display TPU DMA0 registers\n\
tpu_d1regs      Display TPU DMA1 registers\n\
tpu_lregs       Display TPU LDI registers\n\
tpu_linst       Display TPU LDI instructions\n\
tpu_trace       Display TPU trace buffer\n\
tpu_treset      Clear TPU trace buffer\n\
tpu_all         Combined display\n\
"
	);
}


/********************************************************************************
 *										*
 *	idbg_tpu_vers: Display version information for various software		*
 *			components.						*
 *										*
 ********************************************************************************/
static void
idbg_tpu_vers (void)
{
	qprintf ("tpu:     %s\n", tpu_version ());		/* must be linked */
	qprintf ("tpuidbg: %s\n", tpuidbg_version ());		/* we must be here! */

	if (tpusim_version ())					/* may be a stub */
		qprintf ("tpusim:  %s\n", tpusim_version ());
	else
		qprintf ("tpusim:  not included in the kernel\n");
}


/********************************************************************************
 *										*
 *	idbg_tpu_list: Display a list of the tpus.				*
 *										*
 ********************************************************************************/
static void
idbg_tpu_ls (void)
{
	tpu_soft_t *		soft;
	int			i = 0;
	char			name[MAXDEVNAME];

	for (soft = tpu_active.head; i++ < tpu_active.count; soft = soft->ls_alink) {

		if (soft->ls_online)
			qprintf("%d %s ", soft->ls_vhdl, statename (soft->ls_state));
		else
			qprintf("%d OFFLINE       ", soft->ls_vhdl);

		if (hwgraph_vertex_name_get (soft->ls_vhdl, name, MAXDEVNAME) == GRAPH_SUCCESS)
			qprintf("%s\n", name);
		else
			qprintf("<unknown>\n");
	}
}


/********************************************************************************
 *										*
 *	idbg_tpu_trace: Display a trace buffer.					*
 *										*
 ********************************************************************************/
static void		
idbg_tpu_trace 
	(
	ulong_t		arg
	)
{
	tpud_traceb_t *	b;
	char		b1[128];
	char		b2[128];
	int		count;
	int		index;
	__int64_t	boot;
	__int64_t	secs;
	__int64_t	usecs;

	if ((arg == 0) || (arg == -1)) {
		b = tpu_global_trace_buffer;
	}
	else if (arg < VHDL_MAX) {
		b = (TPU_SOFT_GET ((vertex_hdl_t) arg))->ls_tbuffer;
	}
	else {
		b = (tpud_traceb_t *) arg;
	}

	if (b->magic != TPU_M_TRACE) {
		qprintf ("invalid trace buffer address\n");
		return;
	}

	if (b->count <= TPU_TRACEB_LEN) {
		index = 0;
		count = b->count;
	}
	else {
		index = b->count % TPU_TRACEB_LEN;
		count = TPU_TRACEB_LEN;
	}

	boot = b->create_sec - ((b->create_rtc * timer_unit) / 1000000000);

	for (; count > 0; count--) {

		usecs = (b->e[index].timestamp * timer_unit) / 1000;
		secs = (usecs / 1000000) + boot;
		usecs %= 1000000;

		qprintf ("%s %d.%s  ", LBD (b->e[index].cpu, 3, b1), secs, LZD (usecs, 6, b2));
		qprintf (b->e[index].fmt, 
				b->e[index].param[0], b->e[index].param[1], 
				b->e[index].param[2], b->e[index].param[3], 
				b->e[index].param[4]);
		qprintf ("\n");
		index = (index + 1) % TPU_TRACEB_LEN;
	}

	if (b->count == 0)
		qprintf ("\r");	/* prevent "No results returned?" message if buffer was empty */
}


/********************************************************************************
 *										*
 *	idbg_tpu_treset: Clear a trace buffer.					*
 *										*
 ********************************************************************************/
static void		
idbg_tpu_treset
	(
	ulong_t		arg
	)
{
	tpud_traceb_t *	b;

	if ((arg == 0) || (arg == -1)) {
		b = tpu_global_trace_buffer;
	}
	else if (arg < VHDL_MAX) {
		b = (TPU_SOFT_GET ((vertex_hdl_t) arg))->ls_tbuffer;
	}
	else {
		b = (tpud_traceb_t *) arg;
	}

	if (b->magic != TPU_M_TRACE) {
		qprintf ("invalid trace buffer address\n");
		return;
	}

	b->count = 0;

	qprintf ("\r");		/* prevent "No results returned?" message */
}


/********************************************************************************
 *										*
 *	idbg_tpu_soft: Display an TPU soft structure.				*
 *										*
 ********************************************************************************/
static void
idbg_tpu_soft
	(
	ulong_t			arg
	)
{
	vertex_hdl_t		vhdl;
	tpu_soft_t *		soft;
	char			b[128];

	if ((arg == 0) || (arg == -1)) {
		qprintf ("usage: tpu_soft vhdl|softaddr\n");
		return;
	}
	else if (arg < VHDL_MAX) {
		vhdl = (vertex_hdl_t) arg;
		soft = TPU_SOFT_GET (vhdl);
	}
	else {
		soft = (tpu_soft_t *) arg;
		vhdl = soft->ls_vhdl;
	}

	qprintf ("0x%x:\n\n", soft);

	qprintf ("ls_magic:                0x%x (%s)\n", soft->ls_magic, &soft->ls_magic);
	qprintf ("ls_size:                 0x%x\n", soft->ls_size);

	qprintf ("ls_name:                 %s\n", soft->ls_name);

	qprintf ("ls_conn:                 %d\n", soft->ls_conn);
	qprintf ("ls_vhdl:                 %d\n", soft->ls_vhdl);
	qprintf ("ls_admin:                %d\n", soft->ls_admin);

	qprintf ("ls_state:                %s (%d)\n", statename (soft->ls_state), soft->ls_state);
	qprintf ("ls_online:               %d\n", soft->ls_online);
	qprintf ("ls_exclusive:            %d\n", soft->ls_exclusive);
	qprintf ("ls_open:                 %d\n", soft->ls_open);
	qprintf ("ls_assigned:             %d\n", soft->ls_assigned);
	qprintf ("ls_status:               %d\n", soft->ls_status);

	qprintf ("ls_module:               %d\n", soft->ls_module);
	qprintf ("ls_slot:                 %d\n", soft->ls_slot);

	qprintf ("ls_user_flag:            %d\n", soft->ls_user_flag);

	if (soft->ls_service == &tpu_map_xtalk)
		qprintf ("ls_service:              xtalk\n");
	else if (soft->ls_service == &tpu_map_sim)
		qprintf ("ls_service:              tpusim\n");
	else
		qprintf ("ls_service:              unknown! (0x%x)\n", soft->ls_service);

	qprintf ("ls_ldi_offset:           %d\n", soft->ls_ldi_offset);
	qprintf ("ls_timeout:              %d\n", soft->ls_timeout);
	qprintf ("ls_page_size:            %d\n", soft->ls_page_size);
	qprintf ("ls_page_count:           %d\n", soft->ls_page_count);
	qprintf ("ls_al:                   *0x%x\n", &soft->ls_al);

	qprintf ("ls_dmaErrorIntStatus:    0x%s\n", LZX (soft->ls_dmaErrorIntStatus, b));
	qprintf ("ls_dmaErrorD0Status:     0x%s\n", LZX (soft->ls_dmaErrorD0Status, b));
	qprintf ("ls_dmaErrorD1Status:     0x%s\n", LZX (soft->ls_dmaErrorD1Status, b));

	qprintf ("ls_ldiBarrierIntStatus:  0x%s\n", LZX (soft->ls_ldiBarrierIntStatus, b));
	qprintf ("ls_ldiBarrierLdiStatus:  0x%s\n", LZX (soft->ls_ldiBarrierLdiStatus, b));
	qprintf ("ls_ldiBarrierLdiSource:  0x%s\n", LZX (soft->ls_ldiBarrierLdiSource, b));

	qprintf ("ls_ldiErrorIntStatus:    0x%s\n", LZX (soft->ls_ldiErrorIntStatus, b));
	qprintf ("ls_ldiErrorLdiStatus:    0x%s\n", LZX (soft->ls_ldiErrorLdiStatus, b));

	qprintf ("ls_ldiCErrorIntStatus:   0x%s\n", LZX (soft->ls_ldiCErrorIntStatus, b));
	qprintf ("ls_ldiCErrorLdiStatus:   0x%s\n", LZX (soft->ls_ldiCErrorLdiStatus, b));
	qprintf ("ls_ldiCErrorD0Status:    0x%s\n", LZX (soft->ls_ldiCErrorD0Status, b));
	qprintf ("ls_ldiCErrorD1Status:    0x%s\n", LZX (soft->ls_ldiCErrorD1Status, b));

	qprintf ("ls_ldiIntStatus:         0x%s\n", LZX (soft->ls_ldiIntStatus, b));
	qprintf ("ls_ldiLdiStatus:         0x%s\n", LZX (soft->ls_ldiLdiStatus, b));
	qprintf ("ls_ldiLdiSource:         0x%s\n", LZX (soft->ls_ldiLdiSource, b));
	qprintf ("ls_ldiD0Status:          0x%s\n", LZX (soft->ls_ldiD0Status, b));
	qprintf ("ls_ldiD1Status:          0x%s\n", LZX (soft->ls_ldiD1Status, b));
	qprintf ("ls_dma0Count:            0x%s\n", LZX (soft->ls_dma0Count, b));
	qprintf ("ls_dma0Ticks:            0x%s\n", LZX (soft->ls_dma0Ticks, b));
	qprintf ("ls_dma1Count:            0x%s\n", LZX (soft->ls_dma1Count, b));
	qprintf ("ls_dma1Ticks:            0x%s\n", LZX (soft->ls_dma1Ticks, b));

	qprintf ("ls_dmaError_intr:        *0x%x  (tpu_xintr)\n", soft->ls_dmaError_intr);
	qprintf ("ls_ldiBarrier_intr:      *0x%x  (tpu_xintr)\n", soft->ls_ldiBarrier_intr);
	qprintf ("ls_ldiError_intr:        *0x%x  (tpu_xintr)\n", soft->ls_ldiError_intr);
	qprintf ("ls_ldiCError_intr:       *0x%x  (tpu_xintr)\n", soft->ls_ldiCError_intr);

	qprintf ("ls_xregs:                *0x%x  (tpu_xregs)\n", soft->ls_xregs);
	qprintf ("ls_aregs:                *0x%x  (tpu_aregs)\n", soft->ls_aregs);
	qprintf ("ls_d0regs:               *0x%x  (tpu_d0regs)\n", soft->ls_d0regs);
	qprintf ("ls_d1regs:               *0x%x  (tpu_d1regs)\n", soft->ls_d1regs);
	qprintf ("ls_lregs:                *0x%x  (tpu_lregs)\n", soft->ls_lregs);

	qprintf ("ls_mlock:                *0x%x  (mutex)\n", &soft->ls_mlock);
	qprintf ("ls_sv:                   *0x%x  (sv)\n", &soft->ls_sv);

	qprintf ("ls_timeout_id:           0x%x\n", soft->ls_timeout_id);
	qprintf ("ls_sim_id:               0x%x\n", soft->ls_sim_id);

	qprintf ("ls_tbuffer:              *0x%x\n", soft->ls_tbuffer);

	qprintf ("ls_fault.type:           %d\n", soft->ls_fault.type);
	qprintf ("ls_fault.cycles:         %d\n", soft->ls_fault.cycles);
	qprintf ("ls_fault.interval:       %d\n", soft->ls_fault.interval);
	qprintf ("ls_fault.duration:       %d\n", soft->ls_fault.duration);
	qprintf ("ls_fault.tpu_status:     %d\n", soft->ls_fault.tpu_status);

	qprintf ("ls_nic_string:           %s\n", soft->ls_nic_string);

	qprintf ("\n");
}


/********************************************************************************
 *										*
 *	idbg_tpu_stats: Display TPU device statistics.				*
 *										*
 ********************************************************************************/
static void
idbg_tpu_stats
	(
	ulong_t			arg
	)
{
	vertex_hdl_t		vhdl;
	tpu_soft_t *		soft;
	char			b[128];

	if ((arg == 0) || (arg == -1)) {
		qprintf ("usage: tpu_stats vhdl|softaddr\n");
		return;
	}
	else if (arg < VHDL_MAX) {
		vhdl = (vertex_hdl_t) arg;
		soft = TPU_SOFT_GET (vhdl);
	}
	else {
		soft = (tpu_soft_t *) arg;
		vhdl = soft->ls_vhdl;
	}

	qprintf ("0x%x:\n\n", &soft->ls_stat);

	qprintf ("open calls:              %d\n", soft->ls_stat.open.count);
	qprintf ("close calls:             %d\n", soft->ls_stat.close.count);
	qprintf ("mmap calls:              %d\n", soft->ls_stat.mmap.count);
	qprintf ("munmap calls:            %d\n", soft->ls_stat.munmap.count);
	qprintf ("ioctlRun calls:          %d\n", soft->ls_stat.ioctlRun.count);
	qprintf ("ioctlResume calls:       %d\n", soft->ls_stat.ioctlResume.count);
	qprintf ("ioctlHalt calls:         %d\n", soft->ls_stat.ioctlHalt.count);
	qprintf ("ioctlRegs calls:         %d\n", soft->ls_stat.ioctlRegs.count);
	qprintf ("ioctlInst calls:         %d\n", soft->ls_stat.ioctlInst.count);
	qprintf ("ioctlStat calls:         %d\n", soft->ls_stat.ioctlStat.count);
	qprintf ("ioctlStatList calls:     %d\n", soft->ls_stat.ioctlStatList.count);
	qprintf ("ioctlStats calls:        %d\n", soft->ls_stat.ioctlStats.count);
	qprintf ("ioctlConfig calls:       %d\n", soft->ls_stat.ioctlConfig.count);
	qprintf ("ioctlSetFlag calls:      %d\n", soft->ls_stat.ioctlSetFlag.count);
	qprintf ("ioctlGetFlag calls:      %d\n", soft->ls_stat.ioctlGetFlag.count);
	qprintf ("ioctlSetFault calls:     %d\n", soft->ls_stat.ioctlSetFault.count);
	qprintf ("ioctlGetFault calls:     %d\n", soft->ls_stat.ioctlGetFault.count);
	qprintf ("ioctlGetSoft calls:      %d\n", soft->ls_stat.ioctlGetSoft.count);
	qprintf ("ioctlGetTrace calls:     %d\n", soft->ls_stat.ioctlGetTrace.count);
	qprintf ("ioctlGetGTrace calls:    %d\n", soft->ls_stat.ioctlGetGTrace.count);
	qprintf ("ioctlExtTest calls:      %d\n", soft->ls_stat.ioctlExtTest.count);
	qprintf ("16k pages loaded:        %d\n", soft->ls_stat.page[0].count);
	qprintf ("64k pages loaded:        %d\n", soft->ls_stat.page[1].count);
	qprintf ("256k pages loaded:       %d\n", soft->ls_stat.page[2].count);
	qprintf ("1m pages loaded:         %d\n", soft->ls_stat.page[3].count);
	qprintf ("4m pages loaded:         %d\n", soft->ls_stat.page[4].count);
	qprintf ("16m pages loaded:        %d\n", soft->ls_stat.page[5].count);
	qprintf ("timeouts:                %d\n", soft->ls_stat.timeout.count);
	qprintf ("dmaErrors:               %d\n", soft->ls_stat.dmaError.count);
	qprintf ("ldiBarriers:             %d\n", soft->ls_stat.ldiBarrier.count);
	qprintf ("ldiErrors:               %d\n", soft->ls_stat.ldiError.count);
	qprintf ("ldiCErrors:              %d\n", soft->ls_stat.ldiCError.count);

	qprintf ("dma0Count:               %d\n", soft->ls_stat.dma0Count);
	qprintf ("dma0Ticks                %d\n", soft->ls_stat.dma0Ticks);

	qprintf ("Output bytes:            %d.%s MB\n",	soft->ls_stat.dma0Count*8/1000000,
							LZD ((soft->ls_stat.dma0Count*8/1000 % 1000), 3, b));

	qprintf ("Output rate:             %d MB/s\n", soft->ls_stat.dma0Ticks ?
							(soft->ls_stat.dma0Count*800 / soft->ls_stat.dma0Ticks) : 0);

	qprintf ("dma1Count:               %d\n", soft->ls_stat.dma1Count);
	qprintf ("dma1Ticks:               %d\n", soft->ls_stat.dma1Ticks);

	qprintf ("Input bytes:             %d.%s MB\n",	soft->ls_stat.dma1Count*8/1000000,
							LZD ((soft->ls_stat.dma1Count*8/1000 % 1000), 3, b));

	qprintf ("Input rate:              %d MB/s\n", soft->ls_stat.dma1Ticks ?
							(soft->ls_stat.dma1Count*800 / soft->ls_stat.dma1Ticks) : 0);

	qprintf ("\n");
}


/********************************************************************************
 *										*
 *	idbg_tpu_stats: Display global statistics.				*
 *										*
 ********************************************************************************/
static void
idbg_tpu_gstats (void)
{
	qprintf ("0x%x:\n\n", &tpu_gstats);

	qprintf ("Specific device assign:  %d\n", tpu_gstats.assign_spec.count);
	qprintf ("Any device assign:       %d\n", tpu_gstats.assign_any.count);
	qprintf ("Devices searched:        %d\n", tpu_gstats.assign_chk.count);
	qprintf ("No device available:     %d\n", tpu_gstats.assign_none.count);
	qprintf ("\n");
}


/********************************************************************************
 *										*
 *	idbg_tpu_xregs: Display TPU xtalk registers.				*
 *										*
 ********************************************************************************/
static void
idbg_tpu_xregs
	(
	ulong_t			arg
	)
{
	tpu_xtk_regs_t *	xregs;
	char			b[128];

	if ((arg == 0) || (arg == -1)) {
		qprintf ("usage: tpu_xregs vhdl|regaddr\n");
		return;
	}
	else if (arg < VHDL_MAX) {
		xregs = (TPU_SOFT_GET ((vertex_hdl_t) arg))->ls_xregs;
	}
	else {
		xregs = (tpu_xtk_regs_t *) arg;
	}

	qprintf ("0x%x:\n\n", xregs);

	qprintf ("x_id:                  0x%s\n", LZX (xregs->x_id, b));
	qprintf ("x_status:              0x%s\n", LZX (xregs->x_status, b));
	qprintf ("x_err_upper_addr:      0x%s\n", LZX (xregs->x_err_upper_addr, b));
	qprintf ("x_err_lower_addr:      0x%s\n", LZX (xregs->x_err_lower_addr, b));
	qprintf ("x_control:             0x%s\n", LZX (xregs->x_control, b));
	qprintf ("x_req_timeout:         0x%s\n", LZX (xregs->x_req_timeout, b));
	qprintf ("x_intdest_upper_addr:  0x%s\n", LZX (xregs->x_intdest_upper_addr, b));
	qprintf ("x_intdest_lower_addr:  0x%s\n", LZX (xregs->x_intdest_lower_addr, b));
	qprintf ("x_err_cmd_word:        0x%s\n", LZX (xregs->x_err_cmd_word, b));
	qprintf ("x_llp_cfg:             0x%s\n", LZX (xregs->x_llp_cfg, b));
	qprintf ("\n");
	qprintf ("x_ede:                 0x%s\n", LZX (xregs->x_ede, b));
	qprintf ("x_int_status:          0x%s\n", LZX (xregs->x_int_status, b));
	qprintf ("x_int_enable:          0x%s\n", LZX (xregs->x_int_enable, b));
	qprintf ("x_int_addr0:           0x%s\n", LZX (xregs->x_int_addr0, b));
	qprintf ("x_int_addr1:           0x%s\n", LZX (xregs->x_int_addr1, b));
	qprintf ("x_int_addr2:           0x%s\n", LZX (xregs->x_int_addr2, b));
	qprintf ("x_int_addr3:           0x%s\n", LZX (xregs->x_int_addr3, b));
	qprintf ("x_int_addr4:           0x%s\n", LZX (xregs->x_int_addr4, b));
	qprintf ("x_int_addr5:           0x%s\n", LZX (xregs->x_int_addr5, b));
	qprintf ("x_int_addr6:           0x%s\n", LZX (xregs->x_int_addr6, b));
	qprintf ("\n");
	qprintf ("x_sense:               0x%s\n", LZX (xregs->x_sense, b));
	qprintf ("x_leds:                0x%s\n", LZX (xregs->x_leds, b));
	qprintf ("\n");
}


/********************************************************************************
 *										*
 *	idbg_tpu_aregs: Display TPU address translation registers.		*
 *										*
 ********************************************************************************/
static void
idbg_tpu_aregs
	(
	ulong_t			arg
	)
{
	int			i;
	char			b[128];
	char			b2[128];
	tpu_att_regs_t *	aregs;

	if ((arg == 0) || (arg == -1)) {
		qprintf ("usage: tpu_aregs vhdl|regaddr\n");
		return;
	}
	else if (arg < VHDL_MAX) {
		aregs = (TPU_SOFT_GET ((vertex_hdl_t) arg))->ls_aregs;
	}
	else {
		aregs = (tpu_att_regs_t *) arg;
	}

	qprintf ("0x%x:\n\n", aregs);

	qprintf ("a_config:              0x%s\n", LZX (aregs->a_config, b));
	qprintf ("a_diag:                0x%s\n", LZX (aregs->a_diag, b));

	for (i = 0; i < 128; i++)
		qprintf ("a_atte[%s]:           0x%s\n", 
				LZD (i, 3, b), 
				LZX (aregs->a_atte[i], b2));
	qprintf ("\n");
}


/********************************************************************************
 *										*
 *	idbg_tpu_d0regs: Display TPU DMA0 registers.				*
 *										*
 ********************************************************************************/
static void
idbg_tpu_d0regs
	(
	ulong_t			arg
	)
{
	tpu_dma_regs_t *	dregs;

	if ((arg == 0) || (arg == -1)) {
		qprintf ("usage: tpu_d0regs vhdl|regaddr\n");
		return;
	}
	else if (arg < VHDL_MAX) {
		dregs = (TPU_SOFT_GET ((vertex_hdl_t) arg))->ls_d0regs;
	}
	else {
		dregs = (tpu_dma_regs_t *) arg;
	}

	idbg_tpu_dregs (dregs);
}

/********************************************************************************
 *										*
 *	idbg_tpu_d1regs: Display TPU DMA1 registers.				*
 *										*
 ********************************************************************************/
static void
idbg_tpu_d1regs
	(
	ulong_t			arg
	)
{
	tpu_dma_regs_t *	dregs;

	if ((arg == 0) || (arg == -1)) {
		qprintf ("usage: tpu_d1regs vhdl|regaddr\n");
		return;
	}
	else if (arg < VHDL_MAX) {
		dregs = (TPU_SOFT_GET ((vertex_hdl_t) arg))->ls_d1regs;
	}
	else {
		dregs = (tpu_dma_regs_t *) arg;
	}

	idbg_tpu_dregs (dregs);
}

/********************************************************************************
 *										*
 *	idbg_tpu_dregs: Display TPU DMA registers.				*
 *										*
 ********************************************************************************/
static void
idbg_tpu_dregs
	(
	tpu_dma_regs_t *	dregs
	)
{
	char			b[128];

	qprintf ("0x%x:\n\n", dregs);

	qprintf ("d_config0:             0x%s\n", LZX (dregs->d_config0, b));
	qprintf ("d_config1:             0x%s\n", LZX (dregs->d_config1, b));
	qprintf ("d_status:              0x%s\n", LZX (dregs->d_status, b));
	qprintf ("d_diag_mode:           0x%s\n", LZX (dregs->d_diag_mode, b));
	qprintf ("d_perf_timer:          0x%s\n", LZX (dregs->d_perf_timer, b));
	qprintf ("d_perf_count:          0x%s\n", LZX (dregs->d_perf_count, b));
	qprintf ("d_perf_config:         0x%s\n", LZX (dregs->d_perf_config, b));
	qprintf ("\n");
}


/********************************************************************************
 *										*
 *	idbg_tpu_lregs: Display TPU LDI registers.				*
 *										*
 ********************************************************************************/
static void
idbg_tpu_lregs
	(
	ulong_t			arg
	)
{
	int			i;
	char			b[128];
	tpu_ldi_regs_t *	lregs;

	if ((arg == 0) || (arg == -1)) {
		qprintf ("usage: tpu_lregs vhdl|regaddr\n");
		return;
	}
	else if (arg < VHDL_MAX) {
		lregs = (TPU_SOFT_GET ((vertex_hdl_t) arg))->ls_lregs;
	}
	else {
		lregs = (tpu_ldi_regs_t *) arg;
	}

	qprintf ("0x%x:\n\n", lregs);

	qprintf ("riscIOpcode:           0x%s\n", LZX (lregs->riscIOpcode, b));
	qprintf ("riscMode:              0x%s\n", LZX (lregs->riscMode, b));
	qprintf ("xRiscSadr:             0x%s\n", LZX (lregs->xRiscSadr, b));
	qprintf ("coefBs:                0x%s\n", LZX (lregs->coefBs, b));
	qprintf ("coefInc:               0x%s\n", LZX (lregs->coefInc, b));
	qprintf ("coefInit:              0x%s\n", LZX (lregs->coefInit, b));
	qprintf ("dataBs:                0x%s\n", LZX (lregs->dataBs, b));
	qprintf ("saBlkInc:              0x%s\n", LZX (lregs->saBlkInc, b));
	qprintf ("saBlkSize:             0x%s\n", LZX (lregs->saBlkSize, b));
	qprintf ("saBlkInit:             0x%s\n", LZX (lregs->saBlkInit, b));
	qprintf ("statusReg:             0x%s\n", LZX (lregs->statusReg, b));
	qprintf ("sampInit:              0x%s\n", LZX (lregs->sampInit, b));
	qprintf ("vectInitInc:           0x%s\n", LZX (lregs->vectInitInc, b));
	qprintf ("vectInit:              0x%s\n", LZX (lregs->vectInit, b));
	qprintf ("vectInc:               0x%s\n", LZX (lregs->vectInc, b));
	qprintf ("errorMask:             0x%s\n", LZX (lregs->errorMask, b));
	qprintf ("lbaBlkSize:            0x%s\n", LZX (lregs->lbaBlkSize, b));
	qprintf ("loopCnt:               0x%s\n", LZX (lregs->loopCnt, b));
	qprintf ("diagReg:               0x%s\n", LZX (lregs->diagReg, b));
	qprintf ("iSource:               0x%s\n", LZX (lregs->iSource, b));

	for (i = 0; i < 8; i++)
		qprintf ("pipeInBus[%d]:          0x%s\n", i, LZX (lregs->pipeInBus[i], b));

	qprintf ("rRiscSadr:             0x%s\n", LZX (lregs->rRiscSadr, b));
	qprintf ("sampInc:               0x%s\n", LZX (lregs->sampInc, b));
	qprintf ("oLineSize:             0x%s\n", LZX (lregs->oLineSize, b));
	qprintf ("iMask:                 0x%s\n", LZX (lregs->iMask, b));
	qprintf ("tFlag:                 0x%s\n", LZX (lregs->tFlag, b));
	qprintf ("bFlag:                 0x%s\n", LZX (lregs->bFlag, b));
	qprintf ("diagPart:              0x%s\n", LZX (lregs->diagPart, b));
	qprintf ("jmpCnd:                0x%s\n", LZX (lregs->jmpCnd, b));

	qprintf ("\n");
}


/********************************************************************************
 *										*
 *	idbg_tpu_linst: Display TPU LDI instructions.				*
 *										*
 ********************************************************************************/
static void
idbg_tpu_linst
	(
	ulong_t			arg
	)
{
	int			i;
	char			b1[128];
	char			b2[128];
	char			b3[128];
	tpu_ldi_regs_t *	lregs;
	tpu_reg_t		bFlag;
	tpu_reg_t		xRiscSadr;
	tpu_reg_t		inst[TPU_LDI_I_SIZE][2];

	if ((arg == 0) || (arg == -1)) {
		qprintf ("usage: tpu_linst vhdl|regaddr\n");
		return;
	}
	else if (arg < VHDL_MAX) {
		lregs = (TPU_SOFT_GET ((vertex_hdl_t) arg))->ls_lregs;
	}
	else {
		lregs = (tpu_ldi_regs_t *) arg;
	}

	qprintf ("0x%x:\n\n", lregs);

	bFlag = lregs->bFlag;
	xRiscSadr = lregs->xRiscSadr;

	lregs->bFlag = bFlag | LDI_P_OVERRIDE;
	lregs->xRiscSadr = xRiscSadr | LDI_S_READ_BACK;

	for (i = 0; i < TPU_LDI_I_SIZE; i++) {
		inst[i][TPU_LDI_I_UPPER] = lregs->inst[i][TPU_LDI_I_UPPER];
		inst[i][TPU_LDI_I_LOWER] = lregs->inst[i][TPU_LDI_I_LOWER];
	}

	lregs->xRiscSadr = xRiscSadr;
	lregs->bFlag = bFlag;

	for (i = 0; i < TPU_LDI_I_SIZE; i++)
		qprintf ("%s:   0x%s%s\n", 
				LZD (i, 2, b1), 
				LZX (inst[i][TPU_LDI_I_UPPER], b2),
				LZX (inst[i][TPU_LDI_I_LOWER], b3));

	qprintf ("\n");
}


/********************************************************************************
 *										*
 *	idbg_tpu_all: Combined display for a TPU in a single command.		*
 *										*
 ********************************************************************************/
static void
idbg_tpu_all
	(
	ulong_t			arg
	)
{
	vertex_hdl_t		vhdl;
	tpu_soft_t *		soft;
	char			name[MAXDEVNAME];

	if ((arg == 0) || (arg == -1)) {
		qprintf ("usage: tpu_all vhdl|softaddr\n");
		return;
	}
	else if (arg < VHDL_MAX) {
		vhdl = (vertex_hdl_t) arg;
		soft = TPU_SOFT_GET (vhdl);
	}
	else {
		soft = (tpu_soft_t *) arg;
		vhdl = soft->ls_vhdl;
	}

	qprintf("\nFull report for vertex: %d ", vhdl);
	if (hwgraph_vertex_name_get (vhdl, name, MAXDEVNAME) == GRAPH_SUCCESS) {
		qprintf("name: %s\n", name);
	} else {
		qprintf("could not find name\n");
		return;
	}

	qprintf ("\ntpu_soft:\n\n");
	idbg_tpu_soft (arg);

	qprintf ("tpu_stats:\n\n");
	idbg_tpu_stats (arg);

	qprintf ("tpu_xregs:\n\n");
	idbg_tpu_xregs (arg);

	qprintf ("tpu_lregs:\n\n");
	idbg_tpu_lregs (arg);

	qprintf ("tpu_d0regs:\n\n");
	idbg_tpu_d0regs (arg);

	qprintf ("tpu_d1regs:\n\n");
	idbg_tpu_d1regs (arg);

	qprintf ("tpu_aregs:\n\n");
	idbg_tpu_aregs (arg);

	qprintf ("tpu_trace:\n\n");
	idbg_tpu_trace (arg);

	qprintf ("\ntpu_vers:\n\n");
	idbg_tpu_vers ();
}


/* ==============================================================================
 *		Utility functions
 */

/********************************************************************************
 *										*
 *	statename: Convert a TPU state code to an ASCII string.			*
 *										*
 ********************************************************************************/
static char *
statename
	(
	tpu_statecode_t		state		/* tpu state code */
	)
{
	switch (state) {

	case TPU_IDLE:
		return ("IDLE         ");

	case TPU_RUNNING:
		return ("RUNNING      ");

	case TPU_INTERRUPTED_B:
		return ("INTERRUPTED-B");

	case TPU_INTERRUPTED_C:
		return ("INTERRUPTED-C");

	case TPU_INTERRUPTED_L:
		return ("INTERRUPTED-L");

	case TPU_HALTED:
		return ("HALTED       ");

	case TPU_TIMEDOUT:
		return ("TIMEDOUT     ");
	}

	return ("<invalid>    ");
}

#define	LZBS		128

/********************************************************************************
 *										*
 *	lfs - signed binary to ascii with leading fill.				*
 *										*
 *	If qprintf() implemented field widths this wouldn't be needed.		*
 *										*
 ********************************************************************************/

static char * 
lfs
	(
	__int64_t	val,	/* value to convert */
	int		base,	/* radix */
	int		min,	/* minimum number of digits */
	char *		s,	/* output buffer */
	char		f	/* fill character */
	)
{
	__int64_t	sign;
	char		buf[LZBS];
	char *		p = buf;
	char *		q = s;
	int		d;

	if ((sign = val) < 0) {
		val = -val;
		min--;
	}

	do {
		*p++ = ((d = val % base) < 10) ? (d + '0') : (d + 'a' - 10);
		min--;

	} while ((val /= base) > 0);

	if ((sign < 0) && (f == ' '))
		*p++ = '-';

	for (; min > 0; min--)
		*p++ = f;

	if ((sign < 0) && (f != ' '))
		*p++ = '-';

	while (--p >= buf)
		*q++ = *p;

	*q = '\0';

	return s;
}

/********************************************************************************
 *										*
 *	lfu - unsigned binary to ascii with leading fill.			*
 *										*
 *	If qprintf() implemented field widths this wouldn't be needed.		*
 *										*
 ********************************************************************************/

static char * 
lfu
	(
	__uint64_t	val,	/* value to convert */
	int		base,	/* radix */
	int		min,	/* minimum number of digits */
	char *		s,	/* output buffer */
	char		f	/* fill character */
	)
{
	char		buf[LZBS];
	char *		p = buf;
	char *		q = s;
	int		d;

	do {
		*p++ = ((d = val % base) < 10) ? (d + '0') : (d + 'a' - 10);
		min--;

	} while ((val /= base) > 0);

	for (; min > 0; min--)
		*p++ = f;

	while (--p >= buf)
		*q++ = *p;

	*q = '\0';

	return s;
}
