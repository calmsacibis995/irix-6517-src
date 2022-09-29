/*
 * Server side interface to the virtual socket abstraction.
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 */

#ident "$Revision: 1.3 $"

#include <sys/types.h>
#include <limits.h>
#include <sys/fs_subr.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/idbg.h>
#include <sys/vsocket.h>

void qprintf(char *f, ...);

#define VD (void (*)())

void idbg_vsock_trace (int);
void idbg_addfunc(char *, void    (*)());

static struct vsockf {
	char    *comm_name;
	void    (*comm_func)();
	char    *comm_descrip;
} vsock_funcs[] = {
	"vsock", VD idbg_vsocket, "Print vsocket struct",
	"vsocktrace", VD idbg_vsock_trace, "Print vsocket trace",
	0, 0, 0, 0
};

struct vsocktrace {
    enum vsockop tr_op;
    short	tr_cpu;
    short	tr_cell;
    bhv_desc_t	*tr_vsi;
    vsock_t	*tr_vso;
    u_int	tr_flags;
    long	tr_arg1;
    long	tr_arg2;
    long	tr_arg3;
};
#define	MAXTRACE	200
struct vsocktrace *vsocktrace;
int	vsocktrace_index = -1;
int	vsocktrace_enable = 1;
mutex_t	vsocktrace_lock;

void
vsock_trace_init ()
{
	struct vsockf	*vf;

	vsocktrace = (struct vsocktrace *)
		kern_malloc (MAXTRACE * sizeof (struct vsocktrace));
	mutex_init(&vsocktrace_lock, MUTEX_DEFAULT, "vsock trace");
	for (vf = vsock_funcs; vf->comm_name; vf++) {
		idbg_addfunc(vf->comm_name, vf->comm_func);
	}
}

void
vsock_trace (enum vsockop op,
	bhv_desc_t *vsi,
	struct vsocket *vso,
	u_int flags,
	long arg1,
	long arg2,
	long arg3)
{
    struct vsocktrace *vf;

    if (vsocktrace_enable == 0) return;
    if ( ! vsocktrace ) vsock_trace_init ();

    mutex_lock(&vsocktrace_lock, PZERO);
    if (vsocktrace_index == -1)
	bzero ((void *)vsocktrace, MAXTRACE * sizeof (struct vsocktrace));
    if (vsocktrace_index >= (MAXTRACE-1))
	vsocktrace_index = 0;
    else
	vsocktrace_index++;
    vf = vsocktrace + vsocktrace_index;
    bzero((void *)vf, sizeof (struct vsocktrace));

    vf->tr_op = op;
    vf->tr_cpu = cpuid();
    vf->tr_cell = cellid();
    vf->tr_vsi = vsi;
    vf->tr_vso = vso;
    vf->tr_flags = flags;
    vf->tr_arg1 = arg1;
    vf->tr_arg2 = arg2;
    vf->tr_arg3 = arg3;
    mutex_unlock(&vsocktrace_lock);
}

void
print_vsock_trace (int index)
{
    struct vsocktrace	*tr = &vsocktrace[index];
    
    qprintf ("[%3d]  %d  %2d  %2d%s  ",
	index, tr->tr_cell, tr->tr_cpu,
	(tr->tr_op >= VS_MAX) ? tr->tr_op - VS_MAX : tr->tr_op,
	(tr->tr_op >= VS_MAX) ? "*" : " ");
    qprintf ("0x%lx  0x%lx 0x%lx  0x%lx  0x%lx  0x%lx\n",
	tr->tr_vsi, tr->tr_vso, tr->tr_flags,
	tr->tr_arg1, tr->tr_arg2, tr->tr_arg3);
}

void
vsock_trace_header ()
{
    qprintf ("op\t\tvsi\t\tvso\t\targ1\t\targ2\t\targ3\n");

}

void
idbg_vsock_trace (int count)
{
    int		index;
    int		start_index = 0;

    if (vsocktrace_index == -1) return;
    vsock_trace_header();
    if (count == -1) count = MAXTRACE;
    printf ("idbg_vsock_trace: count %d vsocktrace_index %d\n",
	count, vsocktrace_index);
    if (vsocktrace_index < count) {
	start_index = MAXTRACE - (count - vsocktrace_index);
	printf ("idbg_vsock_trace: start_index %d\n", start_index);
	for (index = start_index; index < MAXTRACE; index++) {
	    print_vsock_trace (index);
	}
	start_index = 0;
    }
    if (vsocktrace_index > count) start_index = vsocktrace_index - count;
    printf ("idbg_vsock_trace: start_index %d\n", start_index);
    for (index = start_index; index <= vsocktrace_index; index++) {
	print_vsock_trace (index);
    }
}

/* idbg interfaces */

void
idbg_vsocket(struct vsocket *v)
{

    qprintf ("\nvsocket 0x%lx chain 0x%x\n\n", v, v->vs_bh.bh_first);
    if (v->vs_magic != VS_MAGIC) qprintf ("Bad magic 0x%x ", v->vs_magic);
    if (v->vs_bh.bh_first) {
	qprintf ("\t   magic\t\tobj\t\tops\n\t0x%lx\t0x%lx\t0x%lx\n",
	    v->vs_magic, v->vs_bh.bh_first->bd_vobj, v->vs_bh.bh_first->bd_ops);
	qprintf ("\t   data\t\tchain\n\t0x%lx\t0x%lx\n",
	    v->vs_bh.bh_first->bd_pdata, v->vs_bh.bh_first);
    } else {
	qprintf ("Unconnected!\n");
    }
    qprintf ("\t   type\tproto\tdom\tref\n\t%d\t%d\t%d\t%d\n",
	v->vs_type, v->vs_protocol, v->vs_domain, v->vs_refcnt);
}

void
idbg_intnode(bhv_desc_t *vsi)
{
    struct vsocket *vso = INT_to_VS(vsi);

    qprintf ("intnode 0x%lx\n", vsi);
    idbg_vsocket(vso);
}
