/**************************************************************************
 *									  *
 *	 	Copyright (C) 1994-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Header: /proj/irix6.5.7m/isms/irix/kern/io/grio/RCS/grioidbg.c,v 1.10 1997/04/25 22:46:08 singal Exp $"

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/ktrace.h>
#include <sys/sema.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/uuid.h>
#include <sys/scsi.h>
#include <sys/bpqueue.h>
#include <sys/grio.h>

/*
 * External functions & data not in header files.
 */
#ifdef DEBUG
extern ktrace_entry_t	*ktrace_first(ktrace_t *, ktrace_snap_t *);
extern int		ktrace_nentries(ktrace_t *);
extern ktrace_entry_t	*ktrace_next(ktrace_t *, ktrace_snap_t *);
extern ktrace_entry_t	*ktrace_skip(ktrace_t *, int, ktrace_snap_t *);
#endif

extern grio_cmd_queue_t	*grio_q;
extern bp_queue_t	grio_subreq_bpqueue;
extern grio_stream_info_t *grio_stream_table[];
extern	grio_idbg_disk_info_t	*griodp_idbg_head, *griodp_idbg_tail;


static void idbg_griosum( void );
static void idbg_griodsum( void );
static void idbg_griossum( void );
static void idbg_griohelp( void );
static void idbg_griodisksum(grio_disk_info_t *griodp);
static void idbg_griodiskstreamsum( grio_stream_disk_info_t *griodsp);
static void idbg_griostrsum(grio_stream_info_t *griosp);
static void idbg_griobuf( buf_t * );
static void idbg_griodevtodsum( dev_t );

#define	VD	(void (*)())

static struct xif {
	char	*name;
	void	(*func)();
	char	*help;
} grioidbg_funcs[] = {
    "grhelp",	VD idbg_griohelp,	"Display GRIO IDBG Help",
    "grsum",	VD idbg_griosum,	"Dump GRIO Info summary",
    "grtssum",	VD idbg_griossum,	"Dump ALL GRIO stream summaries",
    "grtdsum",	VD idbg_griodsum,	"Dump ALL GRIO disk summaries",
    "grdsum",	VD idbg_griodisksum,	"Dump ONE GRIO disk summary",
    "grssum",	VD idbg_griostrsum,	"Dump ONE GRIO stream summary",
    "grbuf",   	VD idbg_griobuf,	"Dump GRIO info for this buf",
    "grdvtod",	VD idbg_griodevtodsum,	"Dump GRIO disk summary for given dev",
    0,		0,	0
};


/*
 *  Total size must be 128 bits.  N.B. definition of uuid_t in uuid.h!
 */
#define NODE_SIZE	6

typedef struct {
        u_int32_t       uu_timelow;     /* time "low" */
        u_int16_t       uu_timemid;     /* time "mid" */
        u_int16_t       uu_timehi;      /* time "hi" and version */
        u_int16_t       uu_clockseq;    /* "reserved" and clock sequence */
        u_int16_t       uu_node [NODE_SIZE / 2];/* ethernet hardware address*/
} uu_t;

/*
 *  A string uuid looks like: 7e0c8f5f-113e-101d-8850-010203040506
 */

#define UUID_STR_LEN    36      /* does not include trailing \0 */
#define UUID_FORMAT_STR "%08x-%04hx-%04hx-%04hx-%04x%04x%04x"

static char *uuid_format_str  = { UUID_FORMAT_STR };


static void
uuid_to_str (uuid_t *uuid, char **string_uuid, uint_t *status)
{
	uu_t    *uu = (uu_t *) uuid;

	*status = uuid_s_ok;
	if (string_uuid == NULL)
		return;
	*string_uuid = (char *) kmem_alloc (UUID_STR_LEN+1, KM_NOSLEEP);
	if (uuid == NULL) {
		sprintf (*string_uuid, uuid_format_str,
			0, 0, 0, 0, 0, 0, 0);
		return;
	}

	sprintf (*string_uuid, uuid_format_str,
		uu->uu_timelow,
		uu->uu_timemid,
		uu->uu_timehi,
		uu->uu_clockseq,
		uu->uu_node [0],
		uu->uu_node [1],
		uu->uu_node [2]);
}

static void
free_uuid_str( char * uuidptr) 
{
	kmem_free(uuidptr, UUID_STR_LEN+1);
}


/*
 * Initialization routine.
 */
void
grioidbginit(void)
{
	struct xif	*p;

	for (p = grioidbg_funcs; p->name; p++)
		idbg_addfunc(p->name, p->func);
}

int
grioidbgunload(void)
{
	struct xif	*p;

	for (p = grioidbg_funcs; p->name; p++)
		idbg_delfunc(p->func);
	return 0;
}

static void
idbg_griohelp(void)
{
	struct xif      *p;

	for (p = grioidbg_funcs; p->name; p++)
		qprintf("%s %s\n", padstr(p->name, 16), p->help);
}


static void
idbg_griodevtodsum(dev_t physdev)
{
	grio_disk_info_t	*griodp;
	
	qprintf("\n\n   grio disk info summary \n");
	griodp = grio_disk_info(physdev);
	qprintf("\ndisk %x (%x)\n", physdev, griodp);
	if(griodp)  {
		idbg_griodisksum(griodp);
	}   
}

static void
idbg_griosum(void)
{
	qprintf("grio data summary \n");

	if (grio_q) {
		;
	} else {
		qprintf("grio_q is empty\n");
	}

	if (grio_subreq_bpqueue.queuep) {
		qprintf("%llx on grio_subreq_bpqueue \n",\
			grio_subreq_bpqueue.queuep);
	} else {
		qprintf("no requests on grio_subreq_bpqueue \n");
	}


	idbg_griodsum();
	idbg_griossum();
}

static void
idbg_griodsum( void )
{
	grio_disk_info_t	*griodp;
	grio_idbg_disk_info_t	*cur_disk_info;
	int			i = 0;
	
	qprintf("\n\n   grio disk info summary \n");
	cur_disk_info = griodp_idbg_head;
	while(cur_disk_info)  {
		griodp = cur_disk_info->griodp_ptr;
		if ( griodp ) {
			qprintf("\ndisk %x:\n", i);
			idbg_griodisksum( griodp );
			DELAY( 100000 );
		}
		cur_disk_info = cur_disk_info->next;
		i++;
	}
}

static void
idbg_griossum( void )
{
	int i;
	grio_stream_info_t	*griosp;
	

	qprintf("\n\n   grio stream info summary \n");
	for ( i = 0; i < GRIO_STREAM_TABLE_SIZE ; i++ ) {
		griosp = grio_stream_table[i];
		if (griosp) {
			qprintf("stream table entry %d \n",i);
			while ( griosp ) {
				idbg_griostrsum( griosp );
				DELAY( 100000 );
				griosp = griosp->nextstream;
			}
		}
	}
}
	

static void
idbg_griodisksum(grio_disk_info_t *griodp)
{
	grio_stream_disk_info_t	 *griodsp;

	qprintf("\tgrio_disk_info: addr %llx \n",griodp);

	qprintf("\tmax iops: %d, rsv_iops: %d, opt io size: %d, active: %d \n",
		griodp->num_iops_max, griodp->num_iops_rsv, 
		griodp->opt_io_size, griodp->active);
	qprintf("\tops issued: %d, complete: %d \n",
		griodp->ops_issued, griodp->ops_complete);
	qprintf("\tsubops issued: %d, complete: %d \n",
		griodp->subops_issued, griodp->subops_complete);
	qprintf("\trotate_position: %d \n",
		griodp->rotate_position);
	qprintf("\ttimeout_time %d, timeout_id %d \n",griodp->timeout_time, 
		griodp->timeout_id);

	griodsp = griodp->diskstreams;
	while ( griodsp ) {
		idbg_griodiskstreamsum( griodsp );
		griodsp = griodsp->nextstream;
		DELAY( 100000 );
	}
}

static void
idbg_griodiskstreamsum( grio_stream_disk_info_t *griodsp)
{
	char 		*str;
	buf_t		*bp;
	uint_t		status;

	qprintf("\tgrio_stream_disk_info: addr %llx \n",griodsp);
	uuid_to_str(&(griodsp->thisstream->stream_id), &str, &status);
	qprintf("\tstream id: %s \n",str);
	free_uuid_str(str);

	if ( griodsp->realbp ) {
		qprintf("\t\tactive bp: %llx \n",griodsp->realbp);
		bp = griodsp->bp_list;
		qprintf("\t\tsub bp list:\n ");
		while ( bp ) {
			qprintf("\t\t\t %llx\n ",bp);
			bp = bp->b_grio_list;
			DELAY( 1000 );
		}
		bp = griodsp->issued_bp_list;
		qprintf("\t\tissued sub bp list:\n ");
		while ( bp ) {
			qprintf("\t\t\t %llx\n ",bp);
			bp = bp->b_grio_list;
			DELAY( 1000 );
		}
	} else {
		qprintf("\t\tno active bp \n");
	}

	if ( griodsp->queuedbps_front ) {
		bp = griodsp->queuedbps_front;
		qprintf("\t\tqueued bps list:\n");
		while ( bp ) {
			qprintf("\t\t\t %llx\n",bp);
			bp = bp->av_back;
			DELAY( 1000 );
		}
	} else {
		qprintf("\t\tno queued bps \n");
	}

	qprintf("\t\tnow; %d, period_end: %d, iops_time: %d \n",lbolt,
		griodsp->period_end, griodsp->iops_time);

	qprintf("\t\tiops_remain: %d, num_iops: %d, opt_io_size: %d\n",
		griodsp->iops_remaining_this_period,
		griodsp->num_iops, griodsp->opt_io_size);

	qprintf("\t\tlast_op_time: 0x%x\n",
		griodsp->last_op_time);
}


static void
idbg_griostrsum(grio_stream_info_t *griosp)
{
	char 	*str;
	uint_t	status;

	qprintf("grio_stream_info: addr %llx \n",griosp);
	
	uuid_to_str(&(griosp->stream_id), &str, &status);
	qprintf("stream id: %s \n",str);
	free_uuid_str(str);

	qprintf("fs dev: 0x%x, ino: 0x%llx, procid: %d, flags: 0x%x \n",
		griosp->fs_dev, griosp->ino, griosp->procid, griosp->flags);
	
	qprintf("total_slots: %d, max_count_per_slot: %d, rotate_slot: %d \n",
		griosp->total_slots, griosp->max_count_per_slot, 
		griosp->rotate_slot);
}

static void
idbg_griobuf(buf_t *bp)
{
	char 	*str;
	uint_t	status;
	grio_buf_data_t *griobdp;

	qprintf("grio buf info\n");

	griobdp = (grio_buf_data_t *)(bp->b_grio_private);
	uuid_to_str(&(griobdp->grio_id), &str, &status);
	qprintf("stream id: %s \n",str);
	free_uuid_str(str);

	qprintf("b_grio_list: 0x%llx \n",bp->b_grio_list);
	
	return;

}
