#ifndef __net_soioctl__
#define __net_soioctl__
/*
 * Copyright (c) 1982 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)ioctl.h	6.14 (Berkeley) 8/25/85
 */
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Ioctl definitions
 */
#include <sys/ioctl.h>

/* socket i/o controls */
#define	SIOCSHIWAT	_IOW('s',  0, int)		/* set high watermark */
#define	SIOCGHIWAT	_IOR('s',  1, int)		/* get high watermark */
#define	SIOCSLOWAT	_IOW('s',  2, int)		/* set low watermark */
#define	SIOCGLOWAT	_IOR('s',  3, int)		/* get low watermark */
#define	SIOCATMARK	_IOR('s',  7, int)		/* at oob mark? */
#define	SIOCSPGRP	_IOW('s',  8, int)		/* set process group */
#define	SIOCGPGRP	_IOR('s',  9, int)		/* get process group */
/* 
 * Like FIONREAD except if the socket can't receive more data or the transport
 * layer detected an error, ioctl returns -1 & sets errno to one of
 *	stream socket: ECONNRESET, ETIMEDOUT 
 *	dgram socket:  ECONNRESET, EHOSTUNREACH, ECONNREFUSED (if connected)
 */
#define SIOCNREAD	_IOR('s', 10, int)		/* get #bytes to read*/

#ifndef sgi /* obsolete */
#define	SIOCADDRT	_IOW('r', 10, struct rtentry)	/* add route */
#define	SIOCDELRT	_IOW('r', 11, struct rtentry)	/* delete route */
#define	SIOCSETRTINFO	_IOWR('r', 12, struct fullrtentry) /* change info */
#define	SIOCGETRTINFO	_IOWR('r', 13, struct fullrtentry)  /* get info */
#endif

/* Multicasting */
#define SIOCGETVIFCNT	_IOWR('r',  15, struct sioc_vif_req) /* virt. i/f ct */
#define SIOCGETSGCNT	_IOWR('r',  16, struct sioc_sg_req) /* get source ct */
#define	_SIOCRTSYSCTL	_IOWR('r', 99, struct rtsysctl)	/* temp sysctl */

#define	SIOCSIFADDR	_IOW('i', 12, struct ifreq)	/* set ifnet address */
#define	SIOCGIFADDR	_IOWR('i',13, struct ifreq)	/* get ifnet address */
#define	SIOCSIFDSTADDR	_IOW('i', 14, struct ifreq)	/* set p-p address */
#define	SIOCGIFDSTADDR	_IOWR('i',15, struct ifreq)	/* get p-p address */
#define	OSIOCSIFFLAGS	_IOW('i', 16, struct ifreq)	/* set ifnet flags */
#define	OSIOCGIFFLAGS	_IOWR('i',17, struct ifreq)	/* get ifnet flags */
#define	SIOCGIFCONF	_IOWR('i',20, struct ifconf)	/* get ifnet list */

#ifdef _KERNEL
#define SIOCGIFCONF_INTERNAL _IOWR('i', 20, struct ifreq) /* only for kernel */
#endif

#define	SIOCSARP	_IOW('i', 30, struct arpreq)	/* set arp entry */
#define	SIOCGARP	_IOWR('i',31, struct arpreq)	/* get arp entry */
#define	SIOCDARP	_IOW('i', 32, struct arpreq)	/* delete arp entry */

/*
 * These are SGI's extended arp requests for source routing.
 */
#define	SIOCSARPX	_IOW('i', 30, struct arpreqx)	/* Ext set arp entry */
#define	SIOCGARPX	_IOWR('i',31, struct arpreqx)	/* Ext get arp entry */
#define	SIOCDARPX	_IOW('i', 32, struct arpreqx)	/* Ext del arp entry */

#define SIOCAIFADDR     _IOW('i', 33, struct ifaliasreq) /* add/chg IF alias */
#define SIOCDIFADDR     _IOW('i', 34, struct ifaliasreq) /* delete IF alias */
#define SIOCLIFADDR     _IOWR('i',35, struct ifaliasreq) /* return IF alias */

#define	SIOCGIFBRDADDR	_IOWR('i',23, struct ifreq)	/* get broadcast addr*/
#define	SIOCSIFBRDADDR	_IOW('i', 24, struct ifreq)	/* set broadcast addr*/
#define	SIOCGIFNETMASK	_IOWR('i',25, struct ifreq)	/* get net addr mask */
#define	SIOCSIFNETMASK	_IOW('i', 26, struct ifreq)	/* set net addr mask */
#define	SIOCGIFMETRIC	_IOWR('i',27, struct ifreq)	/* get IF metric */
#define	SIOCSIFMETRIC	_IOW('i', 28, struct ifreq)	/* set IF metric */

#define	SIOCADDMULTI	_IOW('i', 49, struct ifreq)	/* set m/c address */
#define	SIOCDELMULTI	_IOW('i', 50, struct ifreq)	/* clr m/c address */
#define	SIOCSIFSTATS	_IOW('i', 100, struct ifreq)	/* set ifnet stats */
#define	SIOCGIFSTATS	_IOWR('i',101, struct ifreq)	/* get ifnet stats */
#define SIOCSIFHEAD	_IOW('i', 102, struct ifreq)	/* put specified IF at
							   head of list */

#define	SIOCGETLABEL	_IO('i',  103)			/* get socket label */
#define	SIOCSETLABEL	_IO('i',  104)			/* set socket label */

#define	SIOCSIFMEM	_IOW('i', 18, struct ifreq)	/* set interface mem */
#define	SIOCGIFMEM	_IOWR('i',19, struct ifreq)	/* get interface mem */
#define	SIOCSIFMTU	_IOW('i', 21, struct ifreq)	/* set if_mtu */
#define	SIOCGIFMTU	_IOWR('i',22, struct ifreq)	/* get if_mtu */

#define	SIOCUPPER       _IOW('i', 40, struct ifreq)	/* attach upper layer*/
#define	SIOCLOWER       _IOW('i', 41, struct ifreq)	/* attach lower layer*/

#define	SIOCSETSYNC	_IOW('i', 44, struct ifreq)	/* set syncmode */
#define	SIOCGETSYNC	_IOWR('i',45, struct ifreq)	/* get syncmode */
#define	SIOCSSDSTATS	_IOWR('i',46, struct ifreq)	/* sync data stats */
#define	SIOCSSESTATS	_IOWR('i',47, struct ifreq)	/* sync error stats */
#define	SIOCSPROMISC	_IOW('i', 48, int)    /* request promisc mode on/off */

/* STREAMS based socket emulation */
#define SIOCPROTO	_IOW('s', 51, struct socknewproto) /* link proto */
#define SIOCGETNAME	_IOR('s', 52, struct sockaddr)	/* getsockname */
#define SIOCGETPEER	_IOR('s', 53, struct sockaddr)	/* getpeername */
#define IF_UNITSEL	_IOW('s', 54, int)		/* set unit number */
#define SIOCXPROTO	_IO('s',  55)			/* empty proto table */

#define SIOCIFDETACH	_IOW('i', 56, struct ifreq)	/* detach interface */
#define SIOCGENPSTATS	_IOWR('i',57, struct ifreq)	/* get ENP stats */
#define SIOCX25XMT	_IOWR('i',59, struct ifreq)	/* start x25if */
#define SIOCX25RCV	_IOWR('i',60, struct ifreq)	/* start x25if */
#define SIOCX25TBL	_IOWR('i',61, struct ifreq)	/* xfer lun to kernel*/
#define SIOCSLGETREQ	_IOWR('i',71, struct ifreq)	/* wait 4 SLIP req */
#define SIOCSLSTAT	_IOW('i', 72, struct ifreq)	/* pass SLIP info */
#define SIOCSIFNAME	_IOW('i', 73, struct ifreq)	/* set interface name*/
#define SIOCGENADDR	_IOWR('i',85, struct ifreq)	/* Get ethernet addr */
#define SIOCSOCKSYS	_IOW('i', 86, struct socksysreq) /* Pseudo socket */

/* IRIX extensions */
#define SIOCTOSTREAM    _IO('s',  99)      /* convert socket to TPI stream */

/* Trusted IRIX extensions */
#define SIOCGETACL	_IOR('s', 107, struct soacl)	/* get socket ACL */
#define SIOCSETACL	_IOW('s', 108, struct soacl)	/* set socket ACL */
#define SIOCGETUID	_IOR('s', 109, uid_t)		/* get socket uid */
#define SIOCSETUID	_IOW('s', 110, uid_t)		/* set socket uid */
#define SIOCGETRCVUID	_IOR('s', 113, uid_t)		/* get socket rcvuid */
#define SIOCGIFUID	_IOWR('i', 111, struct ifreq)	/* get interface uid */
#define SIOCSIFUID	_IOW('i',  112, struct ifreq)	/* set interface uid */

/* Logical interface operations */
#define SIOCVIF		_IOW('i', 113, struct ifreq) /* set logical i/f */
#define SIOCVIFMAP	_IOW('i', 114, struct ifreq) /* map of logical i/f */
#define SIOCVIFSTAT	_IOW('i', 115, struct ifreq) /* stat of logical i/f */
#define SIOCGIFRECV	_IOWR('i', 116, struct ifreq) /* get recvspace */
#define SIOCSIFRECV	_IOW('i', 117, struct ifreq) /* set recvspace */
#define SIOCGIFSEND	_IOWR('i', 118, struct ifreq) /* get sendspace */
#define SIOCSIFSEND	_IOW('i', 119, struct ifreq) /* set sendspace */

/* load sharing configuration */
#define SIOCSLSCONF	_IOW('i', 120, struct ls_conf)
#define SIOCGLSCONF	_IOWR('i', 121, struct ls_conf)

#define	SIOCSIFFLAGS	_IOW('i', 122, struct ifreq)	/* set ifnet flags */
#define	SIOCGIFFLAGS	_IOWR('i', 123, struct ifreq)	/* get ifnet flags */
#define	SIOCGIFDATA	_IOWR('i', 124, struct ifdatareq) /* get ifnet data */

#define	SIOCATMARP	_IOWR('i', 128, char[IOCPARM_MASK])


#ifdef __cplusplus
}
#endif
#endif	/* __net_soioctl__ */
