/*************************************************************************
 *                                                                       *
 *              Copyright (C) 1993, Silicon Graphics, Inc.               *
 *                                                                       *
 * These coded instructions, statements, and computer programs  contain  *
 * unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 * are protected by Federal copyright law.  They  may  not be disclosed  *
 * to  third  parties  or copied or duplicated in any form, in whole or  *
 * in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                       *
 *************************************************************************/
#include <sys/cmn_err.h>
#include "sys/types.h"
#include "sys/param.h"
#include "sys/sysmacros.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/strmp.h"
#include "sys/errno.h"
#include "sys/debug.h"
#include "sys/kmem.h"
#include "sys/strlog.h"
#include "sys/log.h"
#include "sys/cred.h"
#include "sys/dlpi.h"
#include "sys/dlsap_register.h"
#include "sys/snif.h"
#include "sys/ddi.h"
#include "misc/ether.h"
#include "sys/systm.h"

/*
 * ifnet include files
 */
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "net/soioctl.h"
#include "net/raw.h"
#include "net/if.h"
#include "net/if_types.h"
#include "net/netisr.h"
#include "netinet/if_ether.h"

#if defined (FDDI)
#include "sys/fddi.h"
#endif
#if defined (TOKENRING)
#include "sys/tr.h"
#include "sys/tms380.h"
#include "sys/tr_user.h"
#endif

/*
 * Spider include files
 */
#include "sys/snet/uint.h"
#include "sys/snet/dl_control.h"
#include "sys/snet/dl_proto.h"

STATIC int snifopen(queue_t *, dev_t *, int, int, cred_t *);
STATIC int snifclose(queue_t *, int, cred_t *);
STATIC int snifwput(queue_t *, mblk_t *);
STATIC int snifwsrv(queue_t *);
STATIC int snifrsrv(queue_t *);
STATIC void snif_wproto(queue_t *, mblk_t *);

STATIC int snif_getaddr(snif_dev_t *);
STATIC int snif_mtusize(snif_dev_t *);
STATIC ulong snif_mediatype(snif_dev_t *);
STATIC ulong snif_priority(snif_dev_t *);
STATIC int snif_output(snif_dev_t *, mblk_t *);
STATIC void snif_rcvmsg(snif_dev_t *, struct mbuf *, void *);
STATIC int snif_sap_reg(snif_dev_t *, ushort, ushort);
STATIC void snif_sap_unreg(snif_dev_t *, ushort, ushort);
STATIC int snif_soioctl(snif_dev_t *, mac_address_t *, int);

/*
 * The watch dog code tries to handle the lack
 * of buffer problems.  If WDOG_TRIGGER is returned
 * the packet will be placed on the service queue.
 */
#define WDOG_TRIGGER    ENOSR
STATIC void start_wdog(snif_dev_t *);
STATIC void stop_wdog(snif_dev_t *);

int snifdevflag = D_MP;

#if defined (SINK)
STATIC int snif_sink_in = 0;
STATIC int snif_sink_out = 0;
#endif

STATIC struct module_info minfo = {
    SNIF_ID, "snif", 0, INFPSZ, 0x2000, 0x800
};

STATIC struct qinit rinit = {
    NULL, snifrsrv, snifopen, snifclose, NULL, &minfo, NULL
};

STATIC struct qinit winit = {
    snifwput, snifwsrv, NULL, NULL, NULL, &minfo, NULL
};

struct streamtab snifinfo = {
    &rinit, &winit, NULL, NULL
};

STATIC snif_dev_t *snif_snif;

#ifdef SNIFDBG
#define DSP(a)        printf a
#define HEXDUMP(a,b)    snif_hexdump(a,b)
#define SHEXDUMP(a)	snif_shexdump(a)

void
SNIF_BRK()
{
    printf("snif_brk\n");
}
#else
#define DSP(a)
#define HEXDUMP(a,b)
#define SHEXDUMP(a)
#define SNIF_BRK()
#endif

void
snif_hexdump(char *cp, int len)
{
    register int idx;
    register int qqq;
    char qstr[512];
    static char digits[] = "0123456789abcdef";

    for (idx = 0, qqq = 0; qqq<len;) {
        qstr[idx++] = digits[cp[qqq] >> 4];
        qstr[idx++] = digits[cp[qqq] & 0xf];
        qstr[idx++] = ' ';
        if ((++qqq%16) == 0)
            qstr[idx++] = '\n';
    }
    qstr[idx] = 0;
    if (qqq%16)
        printf("%s\n", qstr);
}



void
snif_shexdump(mblk_t *bp)
{
    register mblk_t *tp = bp;
    register int i = msgdsize(tp);
    register int j;

    i = (i > 18 ? 18 : i);
    while (tp && i > 0)
	j = tp->b_wptr - tp->b_rptr;
        snif_hexdump((char*)(tp->b_rptr), j);
	i -= j;
	tp = tp->b_cont;
}



int
snifinit()
{
    STRLOG((SNIF_ID, -1, 9, SL_TRACE, "dlpi ifnet driver"));

    snif_snif = kmem_zalloc(SNIF_DEV_SIZE * snif_devcnt, KM_NOSLEEP);
    DSP(("snifinit: snif_snif=%x\n",snif_snif));
    return 0;
}

int
snifunload(void)
{
    int    i;

    STRLOG((SNIF_ID, -1, 9, SL_TRACE, "unload called"));
    for (i = 0; i < snif_devcnt; i++)
        if (snif_snif[i].rq)
            return(EBUSY);
    /*
     * Return memory to system
     */
    kmem_free(snif_snif, SNIF_DEV_SIZE * snif_devcnt);
    return(0);
}

/* ARGSUSED */
STATIC int
snifopen(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
    minor_t    minor = getminor(*devp);
    snif_dev_t    *sp;
    struct ifnet    *ifp;
    struct rawif    *rifp;

    STRLOG((SNIF_ID, minor, 9, SL_TRACE, "open called"));

    if (drv_priv(credp))
        return(EPERM);

    if (!snif_snif) {
        STRLOG((SNIF_ID, minor, 0, SL_TRACE, "Problem initalising snif"));
        return(ENXIO);
    }

    /*
     * Do not allow CLONEOPEN or MODOPEN
     */
    if (sflag)
        return(ENXIO);

    /*
     * Only one open/interface.
     * XXX - Should this return EBUSY.
     */
    if (q->q_ptr)
        return(0);

    /*
     * Check minor number is in range
     */
    if (minor >= snif_devcnt) {
        STRLOG((SNIF_ID, minor, 4, SL_TRACE, "Bad minor device"));
        return(ERANGE);
    }

    /*
     * Check mapping of minor number to ifp name
     */

    if ((ifp = ifunit(iftab[minor])) == NULL) {
        STRLOG((SNIF_ID, minor, 4, SL_TRACE, "Bad interface minor"));
        return(ENXIO);
    }

    sp = (snif_dev_t *)&snif_snif[minor];
    sp->ifp = ifp;
    sp->id = minor;
    sp->rq = q;
    sp->saps = NULL;

    sp->mtu = snif_mtusize(sp);
    sp->media = snif_mediatype(sp);
    sp->priority = snif_priority(sp);
    sp->queuing = 0;
    sp->wdogid = 0;
    DSP(("snifopen: sp=%x, minor=%d\n",sp,minor));
    rifp = rawif_lookup(iftab[sp->id]);
    ASSERT(rifp);

    sp->factory_mac.len = rifp->rif_addrlen;
    DSP(("snifopen: media=%d, ifp=%x\n", sp->media, ifp));
    switch (sp->media) {
    case DL_CSMACD:
    case DL_ETHER: {
	bcopy(ifptoeif(ifp)->eif_addr.ea_vec, sp->factory_mac.addr,
	    sp->factory_mac.len);
	break;
	}
    case DL_TPR: {
	DSP(("snifopen: tif_bia=%x, %x, %d\n", &ifptotif(ifp)->tif_bia[0], 
		ifptotif(ifp)->tif_bia, sizeof(struct arpcom)));
    	bcopy(&ifptotif(ifp)->tif_bia[0],sp->factory_mac.addr,
	    sp->factory_mac.len);
	break;
	}
    default:
	ASSERT(1);
    case DL_FDDI:
    	bcopy(rifp->rif_addr,sp->factory_mac.addr,sp->factory_mac.len);
	break;
    }

    if (snif_getaddr(sp) < 0) {
        STRLOG((SNIF_ID, minor, 4, SL_TRACE, "No interface address"));
        return(ENXIO);
    }

#ifdef _MP_NETLOCKS
    SNIF_INITLOCK(sp);
#endif /* _MP_NETLOCKS */

    /*
     * Register NULL and global llc saps.
     */
    snif_sap_reg(sp, 0x00, DL_LLC_ENCAP);
    snif_sap_reg(sp, 0xFF, DL_LLC_ENCAP);

    /* This will be enabled if needed */
    noenable(WR(q));
    q->q_ptr = (caddr_t)sp;
    WR(q)->q_ptr = (caddr_t)sp;

    return(0);
}

/* ARGSUSED */
STATIC int
snifclose(queue_t *q, int flag, cred_t *credp)
{
    snif_dev_t    *sp;
    snif_sap_t    *sap;
    snif_sap_t    *next;
    dlsap_family_t    *dp;

    sp = (snif_dev_t *)q->q_ptr;

    /* sp could be NULL if snifopen was not completed successfully */
    if (!sp)
	return 0;

    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "close called"));
    /*
     * Un-register all saps for this interface including
     * NULL (0x00) and Global (0xFF) saps.
     */
    for (sap = sp->saps; sap; sap = next) {
        dp = sap->dp;
        next = sap->next;
	DSP(("snifclose: unregister sap=%x, dp=%x\n",sap,dp));
        snif_sap_unreg(sp, dp->dl_ethersap, dp->dl_encap);
    }

    sp->rq = NULL;
    stop_wdog(sp);

    return(0);
}

STATIC int
snifwput(queue_t *q, mblk_t *mp)
{
#ifdef DEBUG
    snif_dev_t      *sp = (snif_dev_t *)q->q_ptr;
#endif

    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snifwput called"));

    switch(mp->b_datap->db_type) {
    case M_PCPROTO:
    case M_PROTO:
        snif_wproto(q, mp);
        break;
    case M_FLUSH:
        if (*mp->b_rptr & FLUSHW)
            flushq(q, FLUSHALL);

        if (*mp->b_rptr & FLUSHR) {
            *mp->b_rptr &= ~FLUSHW;
            qreply(q, mp);
        } else
            freemsg(mp);
        break;
    default:
        freemsg(mp);
        break;
    }
    return 0;
}

STATIC int
snifwsrv(queue_t *q)
{
    snif_dev_t    *sp;
    mblk_t        *mp;
    int        ret;

    sp = (snif_dev_t *)q->q_ptr;
    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snifwsrv called"));

    stop_wdog(sp);
    if (!sp->queuing)
        return 0;

    while (mp = getq(q)) {
        if (!(ret = snif_output(sp, mp)))
            continue;

        if (ret == WDOG_TRIGGER) {
            start_wdog(sp);
            putbq(q, mp);
        }
        return 0;
    }
    /*
     * Emptied the queue!
     */
    sp->queuing = 0;
    return 0;
}

STATIC int
snifrsrv(queue_t *q)
{
    mblk_t        *mp;
#ifdef DEBUG
    snif_dev_t      *sp = (snif_dev_t *)q->q_ptr;
#endif

    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snifrsrv called"));

    while (mp = getq(q)) {
        if (canput(q->q_next)) {
            STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "putnext"));
            putnext(q, mp);
        } else {
            STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "putbq"));
            putbq(q, mp);
            break;
        }
    }
    return 0;
}

STATIC int
snif_getaddr(snif_dev_t *sp)
{
    struct rawif    *rifp;

    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snif_getaddr called"));
    ASSERT(sp->ifp);

    rifp = rawif_lookup(iftab[sp->id]);
    ASSERT(rifp);

    sp->current_mac.len = rifp->rif_addrlen;
    bcopy(((struct arpcom *)(sp->ifp))->ac_enaddr,
		sp->current_mac.addr,sp->current_mac.len);
    return(sp->current_mac.len);
}

STATIC void
snif_wproto(queue_t *q, mblk_t *mp)
{
    snif_dev_t          *sp;
    union datal_proto    *dp;

    sp = (snif_dev_t *)q->q_ptr;
    dp = (union datal_proto *)mp->b_rptr;

    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snif_wproto called"));

    switch (dp->type) {
    case DATAL_REG: {
            S_DATAL_REG    *dr;
            dr = (S_DATAL_REG *)mp->b_rptr;

    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "reg sap 0x%x", dr->sap));
            snif_sap_reg(sp, dr->sap, dr->encap);
            freemsg(mp);
        }
        break;
    case DATAL_UNREG: {
            S_DATAL_UNREG *unreg;

            unreg = (S_DATAL_UNREG *)mp->b_rptr;
    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "unreg sap 0x%x", unreg->sap));
            snif_sap_unreg(sp, unreg->sap, unreg->encap);
            freemsg(mp);
        }
        break;
    case DATAL_GET_CUR_ADDR: {
            S_DATAL_ADDRESS    *da;

	    (void)snif_getaddr(sp);
            da = (S_DATAL_ADDRESS *)mp->b_rptr;
            da->addr_len = sp->current_mac.len;
            bcopy(sp->current_mac.addr, da->addr, sp->current_mac.len);
            da->error_field = 0;
            qreply(q, mp);
	}
	break;
    case DATAL_GET_PHYS_ADDR: {
            S_DATAL_ADDRESS    *da;

            da = (S_DATAL_ADDRESS *)mp->b_rptr;
            da->addr_len = sp->factory_mac.len;
            bcopy(sp->factory_mac.addr, da->addr, sp->factory_mac.len);
            da->error_field = 0;
            qreply(q, mp);
        }
        break;
    case DATAL_SET_CUR_ADDR: {
            S_DATAL_ADDRESS    *da;
            mac_address_t    mac;

            da = (S_DATAL_ADDRESS *)mp->b_rptr;
            mac.len = da->addr_len;
            bcopy(da->addr, mac.addr, mac.len);
            da->error_field = snif_soioctl(sp, &mac, SIOCSIFADDR);
            /* Get the new address */
            (void)snif_getaddr(sp);
            qreply(q, mp);
        }
        break;
    case DATAL_ENAB_MULTI: {
            S_DATAL_ADDRESS    *da;
            mac_address_t    mac;

            da = (S_DATAL_ADDRESS *)mp->b_rptr;
            mac.len = da->addr_len;
            bcopy(da->addr, mac.addr, mac.len);
            da->error_field = snif_soioctl(sp, &mac, SIOCADDMULTI);
            qreply(q, mp);
        }
        break;
    case DATAL_DISAB_MULTI: {
            S_DATAL_ADDRESS    *da;
            mac_address_t    mac;

            da = (S_DATAL_ADDRESS *)mp->b_rptr;
            mac.len = da->addr_len;
            bcopy(da->addr, mac.addr, mac.len);
            da->error_field = snif_soioctl(sp, &mac, SIOCDELMULTI);
            qreply(q, mp);
        }
        break;
    case DATAL_TYPE: {
            S_DATAL_RESPONSE    *dr;
            S_DATAL_TYPE        *dt;
            mblk_t             *bp;
            int            size;

            size = sizeof(S_DATAL_TYPE) + sp->current_mac.len -1;
            if ((mp->b_wptr - mp->b_rptr) < size) {
                mp->b_wptr = mp->b_rptr = mp->b_datap->db_base;
                mp->b_datap->db_type = M_ERROR;
                *mp->b_wptr++ = EINVAL;
                qreply(q, mp);
                break;
            }

            size = sizeof(S_DATAL_RESPONSE) + sp->current_mac.len -1;
            if (!(bp = allocb(size, BPRI_HI))) {
                mp->b_wptr = mp->b_rptr = mp->b_datap->db_base;
                mp->b_datap->db_type = M_ERROR;
                *mp->b_wptr++ = ENOSR;
                qreply(q, mp);
                break;
            }

            dr = (S_DATAL_RESPONSE *)bp->b_rptr;
            dt = (S_DATAL_TYPE *)dp;

            /*
             * Register the normal_sap for Spider interface.
             * Both the lwb and upb are the same value.
             */
            snif_sap_reg(sp, dt->lwb, DL_LLC_ENCAP);

            bcopy(sp->current_mac.addr, dr->addr, sp->current_mac.len);
            dr->prim_type = DATAL_RESPONSE;
            dr->frgsz = sp->mtu;
            dr->mac_type = sp->media;
            dr->addr_len = sp->current_mac.len;
            dr->version = DL_VERSION;

            bp->b_datap->db_type = M_PCPROTO;
            bp->b_wptr += size;

            qreply(q, bp);
        }
        break;
    case DATAL_PARAMS: {
            S_DATAL_PARAMS    *dr;
            int        size;

            size = sizeof(S_DATAL_PARAMS) + sp->current_mac.len -1;
            if ((mp->b_wptr - mp->b_rptr) < size) {
                mp->b_wptr = mp->b_rptr = mp->b_datap->db_base;
                mp->b_datap->db_type = M_ERROR;
                *mp->b_wptr++ = EINVAL;
                qreply(q, mp);
                break;
            }

            dr = (S_DATAL_PARAMS *)dp;
            bcopy(sp->current_mac.addr, dr->addr, sp->current_mac.len);
            dr->frgsz = sp->mtu;
            dr->version = DL_VERSION;
            dr->mac_type = sp->media;
            dr->addr_len = sp->current_mac.len;
            qreply(q, mp);
        }
        break;
    case DATAL_TX: 
        if (msgdsize(mp->b_cont) > sp->mtu) {
            freemsg(mp);
            putctl1(RD(q)->q_next, M_ERROR, EINVAL);
            break;
        }

        if (!sp->queuing) {
            if (snif_output(sp, mp) == WDOG_TRIGGER) {
                sp->queuing = 1;
                start_wdog(sp);
                putq(q, mp);
            }
        } else
            putq(q, mp);
        break;
    default:
        mp->b_datap->db_type = M_ERROR;
        mp->b_rptr = mp->b_wptr = mp->b_datap->db_base;
        *mp->b_wptr++ = EINVAL;
        qreply(q, mp);
        break;
    }
}


STATIC int
snif_mtusize(snif_dev_t *sp)
{
    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snif_mtusize called"));
    ASSERT(sp->ifp);
    return(sp->ifp->if_mtu);
}

STATIC ulong
snif_mediatype(snif_dev_t *sp)
{
    ulong    type;

    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snif_mediatype called"));
    ASSERT(sp->ifp);

    switch(sp->ifp->if_type) {
#if defined (FDDI)
    case IFT_FDDI:
        type = DL_FDDI;
        break;
#endif
#if defined (TOKENRING)
    case IFT_ISO88025:
        type = DL_TPR;
        break;
#endif
    case IFT_ETHER:
        type = DL_ETHER;
        break;
    case IFT_ISO88023:
        type = DL_CSMACD;
        break;
    default:
        STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "Unknown media type"));
        type = DL_OTHER;
    }
    return(type);
}

/* ARGSUSED */
STATIC int
snif_output(snif_dev_t *sp, mblk_t *mp)
{
    mblk_t            *bp;
    struct mbuf        *m;
    struct sockaddr_sdl    sdl;
    S_DATAL_TX        *dt;
    int            ret;

    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snif_output called"));
#if defined (SINK)
    if (snif_sink_out) {
        STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "Output Dumping..."));
        freemsg(mp);
        return(0);
    }
#endif
    ASSERT(sp->ifp);

    dt = (S_DATAL_TX *)mp->b_rptr;
    if (!(bp = unlinkb(mp))) {
	cmn_err(CE_WARN,"snif_output: no user data attached, type=%d\n",
		dt->type);
	freemsg(mp);
	return(0);
    }

    m = mblk_to_mbuf(bp, M_DONTWAIT, MT_DATA, 0);
    if (!m) {
        /*
         * Try and queue this packet for later
         */
	STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, 
		"\tFailed in mblk_to_mbuf, put it back to queue\n"));
        linkb(mp, bp);
        return(WDOG_TRIGGER);
    }

    sdl.ssdl_family = AF_SDL;
    sdl.ssdl_addr_len = sp->current_mac.len + dt->route_length;

    /*
     * Src and Dest address length must be the same.
     * Thus sp->current_mac.len is OK to use.
     */
    bcopy(dt->dst, sdl.ssdl_addr, sdl.ssdl_addr_len);
    sdl.ssdl_ethersap = dt->type;
    sdl.ssdl_macpri = sp->priority;
    freeb(mp);

    ASSERT(sp->ifp->if_output);
    IFNET_UPPERLOCK(sp->ifp);
    ret = (*sp->ifp->if_output)(sp->ifp, m, (struct sockaddr *)&sdl, 0);
    IFNET_UPPERUNLOCK(sp->ifp);
#ifdef DEBUG
    if (ret) {
        STRLOG((SNIF_ID, sp->id, 9, SL_TRACE,"snif_output: ret=%d",ret));
    }
#endif
    return(ret);
}

/* ARGSUSED */
STATIC int
snif_soioctl(snif_dev_t *sp, mac_address_t *mac, int cmd)
{
    struct ifaddr	ifa;
    struct sockaddr	sa;
    int		ret;
    caddr_t	data;

    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snif_soioctl called"));
    ASSERT(sp->ifp);
    switch (cmd) {
	case SIOCSIFADDR: 
	    ifa.ifa_addr = &sa;
	    data = (caddr_t)&ifa;
	    break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	    data = (caddr_t)&sa;
	    break;
	default:
	    cmn_err(CE_WARN,"snif_soioctl: IOCTL cmd 0x%x not supported\n",
		cmd);
	    return EINVAL;
    }
    sa.sa_family = AF_RAW;
    bcopy(mac->addr, sa.sa_data, mac->len);
    IFNET_LOCK(sp->ifp);
    ret = (*sp->ifp->if_ioctl)(sp->ifp, cmd, data);
    IFNET_UNLOCK(sp->ifp);
    return (ret);
}

/*
 * This routine is to handle medias that have the concept of
 * priority.  This value is or'ed into the FC field for FDDI
 * and token ring.
 */
STATIC ulong
snif_priority(snif_dev_t *sp)
{
    ulong    pri;

    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snif_priority called"));

    switch(sp->media) {
#if defined (FDDI)
    case DL_FDDI:
        pri = 0;
        break;
#endif
#if defined (TOKENRING)
    case DL_TPR:
        pri = 0;
        break;
#endif
    default:
        pri = 0;
        break;
    }
    return(pri);
}

/*
 * This routine returns zero when the interface is not interested in
 * the sap that has just been passed or if no saps have been registered
 * for this interface.  If the packet is consumed then  a non-zero 
 * value is returned.  If you feel the need to change this routine do
 * NOT mess with the mbuf unless you plan on returning a non-zero value.
 */
int
snif_input(dlsap_family_t *dp, struct ifnet *ifp, struct mbuf *m, void *hdr)
{
    snif_dev_t        *sp;
    snif_sap_t        *sap;
    int            i;
    int            s;

    ASSERT(dp); ASSERT(ifp); ASSERT(m); ASSERT(hdr);

    STRLOG((SNIF_ID, -1, 9, SL_TRACE, "snif_input called"));
    STRLOG((SNIF_ID, -1, 9, SL_TRACE, "dp->dl_ethersap = 0x%x", 
	dp->dl_ethersap));
    DSP(("snif_input called, dp=%x, sap=%x\n",dp,dp->dl_ethersap));
    for (i = 0; i < snif_devcnt; i++)
        if (snif_snif[i].ifp == ifp)
            break;
    /*
     * This interface is not registered.  Give back to drain
     * bacause someone may have a drain socket bound to this
     * interface.
     */
    if (i >= snif_devcnt) {
        STRLOG((SNIF_ID, -1, 9, SL_ERROR, "ifp not registered. snif_devcnt=%d",
		snif_devcnt));
	DSP(("\tifp not registered. snif_devcnt=%d\n", snif_devcnt));
        return(0);
    }

    sp = (snif_dev_t *)&snif_snif[i];
    /*
     * Need to check list of sap's. Just in case this sap has not
     * been registered for this interface.
     */
#ifdef _MP_NETLOCKS
    SNIF_LOCK(sp,s);
#else /* _MP_NETLOCKS */
    s = splimp();
#endif /* _MP_NETLOCKS */
    for (sap = sp->saps; sap; sap = sap->next)
        if (sap->dp == dp)
            break;
#ifdef _MP_NETLOCKS
    SNIF_UNLOCK(sp,s);
#else /* _MP_NETLOCKS */
    splx(s);
#endif /* _MP_NETLOCKS */
    if (!sap) {
        STRLOG((SNIF_ID, -1, 9, SL_ERROR, "SAP not registered"));
	DSP(("\tSAP not registered, first saps=%x\n",sp->saps));
        return(0);    /* This interface not interested in this sap */
    }

    STRLOG((SNIF_ID, -1, 9, SL_TRACE, "0x%x 0x%x", dp->dl_ethersap, sap));
    DSP(("\t0x%x 0x%x\n", dp->dl_ethersap, sap));
#if defined (SINK)
    if (snif_sink_in) {
        STRLOG((SNIF_ID, -1, 9, SL_TRACE, "Input Dumping..."));
	DSP(("\tInput Dumping...\n"));
        m_freem(m);
        return(1);
    }
#endif
    streams_interrupt((strintrfunc_t)snif_rcvmsg, sp, m, hdr);
    return(1);
}

STATIC void
snif_rcvmsg(snif_dev_t *sp, struct mbuf *m, void *hdr)
{
    mblk_t            *mp, *data;
    S_DATAL_RX        *dl;
    ulong            rlen = 0;

    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snif_rcvmsg called"));
    DSP(("snif_rcvmsg called"));

    ASSERT(sp); ASSERT(m);  ASSERT(hdr);

    mp = allocb(64, BPRI_MED);
    if (!mp) {
	DSP(("\tsnif_rcvmsg: fail to allocb, exit\n"));
        m_freem(m);
        return;
    }

    mp->b_datap->db_type = M_PROTO;
    dl = (S_DATAL_RX *)mp->b_rptr;
    dl->prim_type = DATAL_RX;
    dl->version = DL_VERSION;
    dl->addr_length = sp->current_mac.len;
    switch (sp->media) {
#if defined (FDDI)
    case DL_FDDI:
        bcopy(&((struct lmac_hdr *)hdr)->mac_sa,
            dl->src, dl->addr_length);
        bcopy(&((struct lmac_hdr *)hdr)->mac_da,
            dl->src + dl->addr_length, dl->addr_length);
        dl->type = 0;
        break;
#endif
    case DL_CSMACD:        /* FALL THROUGH */
    case DL_ETHER:
	DSP(("\treceived Ethernet input\n"));
        bcopy(((struct ether_header *)hdr)->ether_shost,
            dl->src, dl->addr_length);
        bcopy(((struct ether_header *)hdr)->ether_dhost,
            dl->src + dl->addr_length, dl->addr_length);
        dl->type = ((struct ether_header *)hdr)->ether_type;
	HEXDUMP((char*)hdr,sizeof(struct ether_header));
        break;
#if defined (TOKENRING)
    case DL_TPR:
	DSP(("\treceived Tokenring input\n"));
        bcopy(((TR_MAC_HDR *)hdr)->t_mac_sa,
            dl->src, dl->addr_length);
        bcopy(((TR_MAC_HDR *)hdr)->t_mac_da,
            dl->src + dl->addr_length, dl->addr_length);
        dl->type = 0;
        /*
         * The routing length is VERY dependent on the token
         * ring driver.
         */
        if (((TR_MAC *)hdr)->mac_sa[0] & 0x80) {
            rlen = (((TR_MAC_HDR *)hdr)->ri.rii & TR_RIF_LENGTH) >> 8;
            bcopy((char*)&((TR_MAC_HDR*)hdr)->ri,
            dl->src + 2*dl->addr_length,rlen);
            DSP(("snif_rcvmsg: rlen=%d\n",rlen));
            HEXDUMP((char*)&((TR_MAC_HDR*)hdr)->ri, rlen);
        }
        STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snif_tr: rlen=%d",rlen));
        break;
#endif
    default:
        ASSERT(1);
        break;
    }
    dl->route_length = rlen;
    mp->b_wptr += sizeof(S_DATAL_RX) + 2*sp->current_mac.len + rlen;

    ASSERT(m);
    M_SKIPIFHEADER(m);
    data = mbuf_to_mblk(m, BPRI_HI);
    if (!data) {
        STRLOG((SNIF_ID, sp->id, 4, SL_TRACE, "mbuf_to_mblk failed"));
	DSP(("\tmbuf_to_mblk failed\n"));
        freemsg(mp);
        m_freem(m);
        return;
    }
    HEXDUMP((char*)(data->b_rptr), 12);

    /*
     * Adjust for ethernet padding.
     */
    if (sp->media == DL_ETHER || sp->media == DL_CSMACD) {
        int dsize;
        if (dl->type < IEEE_8023_LEN) {
            dsize = msgdsize(data);
            if (dsize >= dl->type)
                adjmsg(data, dl->type - dsize);
        }
    }

    linkb(mp, data);
    DSP(("\tputq for DLPI\n"));
    putq(sp->rq, mp);
}

STATIC int
snif_sap_reg(snif_dev_t *sp, ushort newsap, ushort encap)
{
    snif_sap_t    *sap;
    int        s;
    dlsap_family_t *first;

    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snif_sap_reg called"));
    DSP(("snif_sap_reg: sp=%x, newsap=%d, encap=%d\n",sp,newsap,encap));

    sap = kmem_zalloc(SNIF_SAP_SIZE, KM_NOSLEEP);
    if (!sap)
        return(1);

    sap->dp = kmem_zalloc(DLSAP_FAMILY_SIZE, KM_NOSLEEP);
    if (!sap->dp) {
        kmem_free(sap, SNIF_SAP_SIZE);
        return(1);
    }

    sap->dp->dl_family = AF_SDL;
    sap->dp->dl_ethersap = newsap;
    sap->dp->dl_encap = encap;
    sap->dp->dl_infunc = snif_input;

    /*
     * Put at head of list
     */
#ifdef _MP_NETLOCKS
    SNIF_LOCK(sp,s);
#else /* _MP_NETLOCKS */
    s = splimp();    /* snif_input is a splimp() */
#endif /* _MP_NETLOCKS */
    if (sp->saps) 
        sap->next = sp->saps;
    sp->saps = sap;
#ifdef _MP_NETLOCKS
    SNIF_UNLOCK(sp,s);
#else /* _MP_NETLOCKS */
    splx(s);
#endif /* _MP_NETLOCKS */

    DSP(("\tsap=%x, dlsap_family=%x\n",sap, sap->dp));
#ifdef SNIFDBG
    {
    snif_sap_t	*t=sp->saps;
    while (t) {
	DSP(("%x  ",t));
	t = t->next;
	}
    DSP(("\n"));
    }
#endif
    if (register_dlsap(sap->dp, &first) == DLSAP_BUSY) {
	kmem_free(sap->dp,DLSAP_FAMILY_SIZE);
	sap->dp = first;
    }
    return(0);
}

STATIC void
snif_sap_unreg(snif_dev_t *sp, ushort oldsap, ushort encap)
{
    snif_sap_t    *prev = NULL;
    snif_sap_t    *sap;
    int        s;

    STRLOG((SNIF_ID, sp->id, 9, SL_TRACE, "snif_sap_unreg called"));
    DSP(("snif_sap_unreg: sap=%d, encap=%d, id=%d\n",oldsap,encap,sp->id));

#ifdef _MP_NETLOCKS
    SNIF_LOCK(sp,s);
#else /* _MP_NETLOCKS */
    s = splimp();
#endif /* _MP_NETLOCKS */
    for (sap = sp->saps; sap; prev = sap, sap = sap->next)
        if (sap->dp->dl_ethersap == oldsap && sap->dp->dl_encap == encap)
            break;
    
#ifdef _MP_NETLOCKS
    SNIF_UNLOCK(sp,s);
#else /* _MP_NETLOCKS */
    splx(s);
#endif /* _MP_NETLOCKS */
    if (!sap)
        return;
#if 0
    ASSERT(sap);
#endif
#ifdef _MP_NETLOCKS
    SNIF_LOCK(sp,s);
#else /* _MP_NETLOCKS */
    s = splimp();
#endif /* _MP_NETLOCKS */
    if (prev)
        prev->next = sap->next;
    else
        sp->saps = sap->next;
#ifdef _MP_NETLOCKS
    SNIF_UNLOCK(sp,s);
#else /* _MP_NETLOCKS */
    splx(s);
#endif /* _MP_NETLOCKS */

    if (sap->dp->ref_count) {
	DSP(("\tref_count=%d\n",sap->dp->ref_count));
    	unregister_dlsap(sap->dp);
    } else {
    	unregister_dlsap(sap->dp);
	DSP(("\tfree dlsap_family %x\n",sap->dp));
    	kmem_free(sap->dp, DLSAP_FAMILY_SIZE);
    }
    DSP(("\tfree snif_sap entry %x\n",sap));
    kmem_free(sap, SNIF_SAP_SIZE);
}

STATIC void
snif_enable(snif_dev_t *sp)
{
    if (sp->queuing)
        qenable(WR(sp->rq));
}

/*
 * Watchdog timer set to 1 second
 */
#define WDOG_INT drv_usectohz(1000000 * 1)

STATIC void
start_wdog(snif_dev_t *sp)
{
    if (!sp->wdogid)
        sp->wdogid = STREAMS_TIMEOUT(snif_enable, sp, WDOG_INT);
}

STATIC void
stop_wdog(snif_dev_t *sp)
{
    if (sp->wdogid) {
        untimeout(sp->wdogid);
        sp->wdogid = 0;
    }
}
