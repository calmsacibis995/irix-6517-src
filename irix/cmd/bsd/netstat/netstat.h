/*
 * Copyright (c) 1992, 1993
 *	Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)netstat.h	8.2 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>

#ifdef sgi
#ident "$Revision: 1.12 $"

#include <sys/types.h>
#if (_MIPS_SIM == _ABIN32)
#define ns_off_t	unsigned long
#else
#define ns_off_t	off_t
#endif

#include <sys/mac_label.h>

/*
 * Parameter limits
 */
#define MAX_INTERVAL_PAR	1209600

struct soacl;
struct sockaddr;
struct socket;
struct unpcb;
struct protosw;
struct sockaddr_ns;

extern int kmem;
extern ns_off_t klseek(int, ns_off_t, int);
extern struct mbufconst mbufconst;

extern void unixdomainpr(register struct socket *, caddr_t, struct unpcb *);
extern void streampr(ns_off_t);
extern struct protox *knownname(char *);

extern mac_label plabel;
extern short havemac;   	/* kernel supports Mandatory Access Control */
extern mac_label *fetchlabel(mac_label *, ns_off_t);
extern void labelpr(mac_label *);
extern void soaclpr(struct soacl *);

extern int	lflag;
extern int	qflag;
#endif

extern int	Aflag;		/* show addresses of protocol control block */
extern int	Nflag;		/* show link# as addresses */
extern int	aflag;		/* show all sockets (including servers) */
#ifndef sgi
extern int	dflag;		/* show i/f dropped packets */
extern int	gflag;		/* show group (multicast) routing or stats */
#endif
extern int	iflag;		/* show interfaces */
extern int	mflag;		/* show memory stats */
extern int	nflag;		/* show addresses numerically */
extern int	pflag;		/* show given protocol */
extern int	rflag;		/* show routing tables (or routing stats) */
extern int	sflag;		/* show protocol statistics */
extern int	tflag;		/* show i/f watchdog timers */

extern int	interval;	/* repeat interval for i/f stats */

extern char	*interface;	/* desired i/f for stats, or NULL for all i/fs */
extern int	unit;		/* unit number for above */

extern int	af;		/* address family */

extern char	*prog;		/* program name */


extern void	kread(int fd, void *buf, unsigned len);
extern char	*plural __P((int));
extern char	*plurales __P((int));

extern void	protopr(ns_off_t, char *);
extern void	tcp_stats(char *);
extern void	udp_stats(char *);
extern void	ip_stats(char *);
extern void	icmp_stats(char *);
extern void	igmp_stats(char *);

extern void	mbpr(ns_off_t);

extern void	hostpr __P((u_long, u_long));
extern void	impstats __P((u_long, u_long));

extern void	intpr(int, ns_off_t, ns_off_t);

extern void	pr_rthdr __P(());
extern void	pr_family __P((int));
extern void	rt_stats(ns_off_t);
extern char	*ns_phost __P((struct sockaddr_ns *));
extern void	upHex __P((char *));

extern char	*routename(__uint32_t, int);
extern char	*netname(__uint32_t, __uint32_t, int);
extern char	*ns_print __P((struct sockaddr *));
extern void	routepr __P((u_long));

extern void	nsprotopr __P((u_long, char *));
extern void	spp_stats __P((u_long, char *));
extern void	idp_stats __P((u_long, char *));
extern void	nserr_stats __P((u_long, char *));

extern void	unixpr(ns_off_t, ns_off_t, struct protosw *);

#ifndef sgi
extern void	esis_stats __P((u_long, char *));
extern void	clnp_stats __P((u_long, char *));
extern void	cltp_stats __P((u_long, char *));
extern void	iso_protopr __P((u_long, char *));
extern void	iso_protopr1 __P((u_long, int));
extern void	tp_protopr __P((u_long, char *));
extern void	tp_inproto __P((u_long));
extern void	tp_stats __P((caddr_t, caddr_t));
#endif

extern void	mroutepr(ns_off_t, ns_off_t, ns_off_t);
extern void	mrt_stats(ns_off_t);
