#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include <sys/SN/SN0/IP27.h>
#include <sys/SN/vector.h>
#include <sys/SN/agent.h>
#include <libsk.h>
#include <libkl.h>
#include <rtc.h>

/* This file is _not_ shared with the file of the same name in ml/SN0 */

void rstat(net_vec_t vec)
{
    __uint64_t 			reg, hist;
    struct rr_status_error_fmt	rserr;
    int				i, r;

    if ((r = vector_read(vec, 0, RR_STATUS_REV_ID, &reg)) < 0) {
    neterr:
	printf("Network vector read error: %s\n", net_errmsg(r));
	return;
    }

    if (((reg & NSRI_CHIPID_MASK) >> NSRI_CHIPID_SHFT) != CHIPID_ROUTER) {
	printf("Destination must be a router.\n");
	return;
    }

    printf("Router local status: 0x%lx\n",
	   (reg & RSRI_LOCAL_MASK) >> RSRI_LOCAL_SHFT);

    printf("Port llp Oflw bdDir dedTO tailTO retry cb_err sn_err  "
	   "Bkt3  Bkt2  Bkt1  Bkt0\n");

    for (i = 1; i <= 6; i++) {
	if ((r = vector_read(vec, 0,
			      RR_ERROR_CLEAR(i), (__uint64_t *) &rserr)) < 0)
	    goto neterr;

	if ((r = vector_read(vec, 0,
			      RR_HISTOGRAM(i), &hist)) < 0)
	    goto neterr;

	printf("%-3d   %1x   %1d    %1d     %1x     %1x   %4d   %4d   %4d    "
	       "%4x  %4x  %4x  %4x\n",
	       i,
	       (reg&RSRI_LSTAT_MASK(i))>>RSRI_LSTAT_SHFT(i),
	       rserr.rserr_fifooverflow,
	       rserr.rserr_illegalport,
	       rserr.rserr_deadlockto,
	       rserr.rserr_recvtailto,
	       rserr.rserr_retrycnt,
	       rserr.rserr_cberrcnt,
	       rserr.rserr_snerrcnt,
	       hist >> RHIST_BUCKET_SHFT(3) & 0xffff,
	       hist >> RHIST_BUCKET_SHFT(2) & 0xffff,
	       hist >> RHIST_BUCKET_SHFT(1) & 0xffff,
	       hist >> RHIST_BUCKET_SHFT(0) & 0xffff);

	if ((r = vector_write(vec, 0,
			       RR_HISTOGRAM(i), 0ULL)) < 0)
	    goto neterr;
    }
}

#define	MAX_DEPTH	15

static net_vec_t
try_router(lboard_t *src, lboard_t *dst, int depth, net_vec_t path, int *skip)
{
    net_vec_t	result;
    int		port;
    klrou_t	*info;

    if (!(info = (klrou_t *) find_first_component(src, KLSTRUCT_ROU)))
        return NET_VEC_BAD;

    for (port = 6; port >= 1; port--) {
        lboard_t	*next;

        if (info->rou_port[port].port_nasid == INVALID_NASID)
            continue;

        next = (lboard_t *) NODE_OFFSET_TO_K1(info->rou_port[port].port_nasid,
		info->rou_port[port].port_offset);

        if ((next->brd_nic == dst->brd_nic) && ((*skip)-- == 0))
            return vector_concat(port, path);

        if ((depth > 1) && (next->brd_type == KLTYPE_ROUTER) &&
		((result = try_router(next,
				dst,
				depth - 1,
				vector_concat(port, path),
				skip)) != NET_VEC_BAD))
        return result;
    }

    return NET_VEC_BAD;
}

/*
 * Function:		klcfg_discover_route -> same as discover_route
 *			in stand/arcs/IP27prom/discover.c but operates on
 *			klconfig
 * Refer:		Above file for comments
 */

net_vec_t
klcfg_discover_route(lboard_t *src, lboard_t *dst, int nth_alternate)
{
    int		depth, skip;
    net_vec_t	result;
    klhub_t	*info;
    lboard_t	*next;

    if (src->brd_nic == dst->brd_nic)
        return 0;

    if (src->brd_type == KLTYPE_IP27) {
        if (!(info = (klhub_t *) find_first_component(src, KLSTRUCT_HUB))) {
            printf("No HUB struct on nasid %d\n", src->brd_nasid);
            return NET_VEC_BAD;
        }

        if (info->hub_port.port_nasid == INVALID_NASID)
            return NET_VEC_BAD;

        next = (lboard_t *) NODE_OFFSET_TO_K1(info->hub_port.port_nasid,
			info->hub_port.port_offset);
    }
    else
        next = src;

    if ((next->brd_nic == dst->brd_nic) && !nth_alternate)
        return 0;

    if (next->brd_type == KLTYPE_IP27)
        return NET_VEC_BAD;

    for (depth = 1; depth < MAX_DEPTH; depth++) {
        skip = nth_alternate;

        if ((result = try_router(next, dst, depth, 0, &skip)) != NET_VEC_BAD)
            return vector_reverse(result);

        if (skip < nth_alternate)
            break;	/* In this case, higher-depth paths are meaningless */
    }

    return NET_VEC_BAD;
}
