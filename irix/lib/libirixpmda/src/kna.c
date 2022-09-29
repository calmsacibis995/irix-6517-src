#ident "$Id: kna.c,v 1.22 1999/10/22 00:54:51 ivanr Exp $"

/*
 * Handle metrics for cluster kna (17)
 *
 * Code built by newcluster on Mon Jun  6 15:40:17 EST 1994
 */

#include <sys/types.h>
#include <sys/sysmp.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/tcpipstats.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

static struct kna	kna;

#if BEFORE_IRIX6_5
#define PM_KNA_TYPE	PM_TYPE_32
#define PM_KNA_UTYPE	PM_TYPE_U32
#else /* IRIX6.5 and later */
#define PM_KNA_TYPE	PM_TYPE_U64
#define PM_KNA_UTYPE	PM_TYPE_U64
#endif

static pmMeta		meta[] = {
/* irix.network.tcp.connattempt */
  { (char *)&kna.tcpstat.tcps_connattempt, { PMID(1,17,1), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.accepts */
  { (char *)&kna.tcpstat.tcps_accepts, { PMID(1,17,2), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.connects */
  { (char *)&kna.tcpstat.tcps_connects, { PMID(1,17,3), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.drops */
  { (char *)&kna.tcpstat.tcps_drops, { PMID(1,17,4), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.conndrops */
  { (char *)&kna.tcpstat.tcps_conndrops, { PMID(1,17,5), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.closed */
  { (char *)&kna.tcpstat.tcps_closed, { PMID(1,17,6), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.segstimed */
  { (char *)&kna.tcpstat.tcps_segstimed, { PMID(1,17,7), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rttupdated */
  { (char *)&kna.tcpstat.tcps_rttupdated, { PMID(1,17,8), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.delack */
  { (char *)&kna.tcpstat.tcps_delack, { PMID(1,17,9), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.timeoutdrop */
  { (char *)&kna.tcpstat.tcps_timeoutdrop, { PMID(1,17,10), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rexmttimeo */
  { (char *)&kna.tcpstat.tcps_rexmttimeo, { PMID(1,17,11), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.persisttimeo */
  { (char *)&kna.tcpstat.tcps_persisttimeo, { PMID(1,17,12), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.keeptimeo */
  { (char *)&kna.tcpstat.tcps_keeptimeo, { PMID(1,17,13), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.keepprobe */
  { (char *)&kna.tcpstat.tcps_keepprobe, { PMID(1,17,14), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.keepdrops */
  { (char *)&kna.tcpstat.tcps_keepdrops, { PMID(1,17,15), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.sndtotal */
  { (char *)&kna.tcpstat.tcps_sndtotal, { PMID(1,17,16), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.sndpack */
  { (char *)&kna.tcpstat.tcps_sndpack, { PMID(1,17,17), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.sndbyte */
  { (char *)&kna.tcpstat.tcps_sndbyte, { PMID(1,17,18), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.network.tcp.sndrexmitpack */
  { (char *)&kna.tcpstat.tcps_sndrexmitpack, { PMID(1,17,19), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.sndrexmitbyte */
  { (char *)&kna.tcpstat.tcps_sndrexmitbyte, { PMID(1,17,20), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.network.tcp.sndacks */
  { (char *)&kna.tcpstat.tcps_sndacks, { PMID(1,17,21), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.sndprobe */
  { (char *)&kna.tcpstat.tcps_sndprobe, { PMID(1,17,22), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.sndurg */
  { (char *)&kna.tcpstat.tcps_sndurg, { PMID(1,17,23), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.sndwinup */
  { (char *)&kna.tcpstat.tcps_sndwinup, { PMID(1,17,24), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.sndctrl */
  { (char *)&kna.tcpstat.tcps_sndctrl, { PMID(1,17,25), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.sndrst */
  { (char *)&kna.tcpstat.tcps_sndrst, { PMID(1,17,26), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvtotal */
  { (char *)&kna.tcpstat.tcps_rcvtotal, { PMID(1,17,27), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvpack */
  { (char *)&kna.tcpstat.tcps_rcvpack, { PMID(1,17,28), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvbyte */
  { (char *)&kna.tcpstat.tcps_rcvbyte, { PMID(1,17,29), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.network.tcp.rcvbadsum */
  { (char *)&kna.tcpstat.tcps_rcvbadsum, { PMID(1,17,30), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvbadoff */
  { (char *)&kna.tcpstat.tcps_rcvbadoff, { PMID(1,17,31), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvshort */
  { (char *)&kna.tcpstat.tcps_rcvshort, { PMID(1,17,32), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvduppack */
  { (char *)&kna.tcpstat.tcps_rcvduppack, { PMID(1,17,33), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvdupbyte */
  { (char *)&kna.tcpstat.tcps_rcvdupbyte, { PMID(1,17,34), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.network.tcp.rcvpartduppack */
  { (char *)&kna.tcpstat.tcps_rcvpartduppack, { PMID(1,17,35), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvpartdupbyte */
  { (char *)&kna.tcpstat.tcps_rcvpartdupbyte, { PMID(1,17,36), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.network.tcp.rcvoopack */
  { (char *)&kna.tcpstat.tcps_rcvoopack, { PMID(1,17,37), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvoobyte */
  { (char *)&kna.tcpstat.tcps_rcvoobyte, { PMID(1,17,38), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.network.tcp.rcvpackafterwin */
  { (char *)&kna.tcpstat.tcps_rcvpackafterwin, { PMID(1,17,39), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvbyteafterwin */
  { (char *)&kna.tcpstat.tcps_rcvbyteafterwin, { PMID(1,17,40), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.network.tcp.rcvafterclose */
  { (char *)&kna.tcpstat.tcps_rcvafterclose, { PMID(1,17,41), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvwinprobe */
  { (char *)&kna.tcpstat.tcps_rcvwinprobe, { PMID(1,17,42), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvdupack */
  { (char *)&kna.tcpstat.tcps_rcvdupack, { PMID(1,17,43), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvacktoomuch */
  { (char *)&kna.tcpstat.tcps_rcvacktoomuch, { PMID(1,17,44), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvackpack */
  { (char *)&kna.tcpstat.tcps_rcvackpack, { PMID(1,17,45), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.rcvackbyte */
  { (char *)&kna.tcpstat.tcps_rcvackbyte, { PMID(1,17,46), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.network.tcp.rcvwinupd */
  { (char *)&kna.tcpstat.tcps_rcvwinupd, { PMID(1,17,47), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

#if BEFORE_IRIX6_5

/* irix.network.tcp.pcbcachemiss */
  { (char *)&kna.tcpstat.tcpps_pcbcachemiss, { PMID(1,17,48), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

#else /* IRIX 6.5 and later */

/* irix.network.tcp.pcbcachemiss */
  { 0, { PMID(1,17,48), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },

#endif

/* irix.network.tcp.predack */
  { (char *)&kna.tcpstat.tcps_predack, { PMID(1,17,49), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.preddat */
  { (char *)&kna.tcpstat.tcps_preddat, { PMID(1,17,50), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.pawsdrop */
  { (char *)&kna.tcpstat.tcps_pawsdrop, { PMID(1,17,51), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.error */
  { (char *)&kna.icmpstat.icps_error, { PMID(1,17,52), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.oldshort */
  { (char *)&kna.icmpstat.icps_oldshort, { PMID(1,17,53), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.oldicmp */
  { (char *)&kna.icmpstat.icps_oldicmp, { PMID(1,17,54), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.badcode */
  { (char *)&kna.icmpstat.icps_badcode, { PMID(1,17,55), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.tooshort */
  { (char *)&kna.icmpstat.icps_tooshort, { PMID(1,17,56), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.checksum */
  { (char *)&kna.icmpstat.icps_checksum, { PMID(1,17,57), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.badlen */
  { (char *)&kna.icmpstat.icps_badlen, { PMID(1,17,58), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.reflect */
  { (char *)&kna.icmpstat.icps_reflect, { PMID(1,17,59), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.echoreply */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_ECHOREPLY], { PMID(1,17,60), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.unreach */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_UNREACH], { PMID(1,17,61), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.sourcequench */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_SOURCEQUENCH], { PMID(1,17,62), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.redirect */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_REDIRECT], { PMID(1,17,63), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.echo */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_ECHO], { PMID(1,17,64), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.routeradvert */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_ROUTERADVERT], { PMID(1,17,65), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.routersolicit */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_ROUTERSOLICIT], { PMID(1,17,66), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.timxceed */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_TIMXCEED], { PMID(1,17,67), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.paramprob */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_PARAMPROB], { PMID(1,17,68), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.tstamp */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_TSTAMP], { PMID(1,17,69), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.tstampreply */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_TSTAMPREPLY], { PMID(1,17,70), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.ireq */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_IREQ], { PMID(1,17,71), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.ireqreply */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_IREQREPLY], { PMID(1,17,72), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.maskreq */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_MASKREQ], { PMID(1,17,73), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.inhist.maskreply */
  { (char *)&kna.icmpstat.icps_inhist[ICMP_MASKREPLY], { PMID(1,17,74), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.echoreply */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_ECHOREPLY], { PMID(1,17,75), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.unreach */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_UNREACH], { PMID(1,17,76), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.sourcequench */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_SOURCEQUENCH], { PMID(1,17,77), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.redirect */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_REDIRECT], { PMID(1,17,78), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.echo */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_ECHO], { PMID(1,17,79), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.routeradvert */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_ROUTERADVERT], { PMID(1,17,80), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.routersolicit */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_ROUTERSOLICIT], { PMID(1,17,81), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.timxceed */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_TIMXCEED], { PMID(1,17,82), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.paramprob */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_PARAMPROB], { PMID(1,17,83), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.tstamp */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_TSTAMP], { PMID(1,17,84), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.tstampreply */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_TSTAMPREPLY], { PMID(1,17,85), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.ireq */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_IREQ], { PMID(1,17,86), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.ireqreply */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_IREQREPLY], { PMID(1,17,87), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.maskreq */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_MASKREQ], { PMID(1,17,88), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.icmp.outhist.maskreply */
  { (char *)&kna.icmpstat.icps_outhist[ICMP_MASKREPLY], { PMID(1,17,89), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.igmp.rcv_total */
  { (char *)&kna.igmpstat.igps_rcv_total, { PMID(1,17,90), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.igmp.rcv_tooshort */
  { (char *)&kna.igmpstat.igps_rcv_tooshort, { PMID(1,17,91), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.igmp.rcv_badsum */
  { (char *)&kna.igmpstat.igps_rcv_badsum, { PMID(1,17,92), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.igmp.rcv_queries */
  { (char *)&kna.igmpstat.igps_rcv_queries, { PMID(1,17,93), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.igmp.rcv_badqueries */
  { (char *)&kna.igmpstat.igps_rcv_badqueries, { PMID(1,17,94), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.igmp.rcv_reports */
  { (char *)&kna.igmpstat.igps_rcv_reports, { PMID(1,17,95), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.igmp.rcv_badreports */
  { (char *)&kna.igmpstat.igps_rcv_badreports, { PMID(1,17,96), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.igmp.rcv_ourreports */
  { (char *)&kna.igmpstat.igps_rcv_ourreports, { PMID(1,17,97), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.igmp.snd_reports */
  { (char *)&kna.igmpstat.igps_snd_reports, { PMID(1,17,98), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.badhlen */
  { (char *)&kna.ipstat.ips_badhlen, { PMID(1,17,99), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.badlen */
  { (char *)&kna.ipstat.ips_badlen, { PMID(1,17,100), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.badoptions */
  { (char *)&kna.ipstat.ips_badoptions, { PMID(1,17,101), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.badsum */
  { (char *)&kna.ipstat.ips_badsum, { PMID(1,17,102), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.cantforward */
  { (char *)&kna.ipstat.ips_cantforward, { PMID(1,17,103), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.cantfrag */
  { (char *)&kna.ipstat.ips_cantfrag, { PMID(1,17,104), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.delivered */
  { (char *)&kna.ipstat.ips_delivered, { PMID(1,17,105), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.forward */
  { (char *)&kna.ipstat.ips_forward, { PMID(1,17,106), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.fragdropped */
  { (char *)&kna.ipstat.ips_fragdropped, { PMID(1,17,107), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.fragmented */
  { (char *)&kna.ipstat.ips_fragmented, { PMID(1,17,108), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.fragments */
  { (char *)&kna.ipstat.ips_fragments, { PMID(1,17,109), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.fragtimeout */
  { (char *)&kna.ipstat.ips_fragtimeout, { PMID(1,17,110), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.localout */
  { (char *)&kna.ipstat.ips_localout, { PMID(1,17,111), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.noproto */
  { (char *)&kna.ipstat.ips_noproto, { PMID(1,17,112), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.noroute */
  { (char *)&kna.ipstat.ips_noroute, { PMID(1,17,113), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.odropped */
  { (char *)&kna.ipstat.ips_odropped, { PMID(1,17,114), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.ofragments */
  { (char *)&kna.ipstat.ips_ofragments, { PMID(1,17,115), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.reassembled */
  { (char *)&kna.ipstat.ips_reassembled, { PMID(1,17,116), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.redirect */
  { (char *)&kna.ipstat.ips_redirectsent, { PMID(1,17,117), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.tooshort */
  { (char *)&kna.ipstat.ips_tooshort, { PMID(1,17,118), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.toosmall */
  { (char *)&kna.ipstat.ips_toosmall, { PMID(1,17,119), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.total */
  { (char *)&kna.ipstat.ips_total, { PMID(1,17,120), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.udp.ipackets */
  { (char *)&kna.udpstat.udps_ipackets, { PMID(1,17,121), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.udp.hdrops */
  { (char *)&kna.udpstat.udps_hdrops, { PMID(1,17,122), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.udp.badsum */
  { (char *)&kna.udpstat.udps_badsum, { PMID(1,17,123), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.udp.badlen */
  { (char *)&kna.udpstat.udps_badlen, { PMID(1,17,124), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.udp.noport */
  { (char *)&kna.udpstat.udps_noport, { PMID(1,17,125), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.udp.noportbcast */
  { (char *)&kna.udpstat.udps_noportbcast, { PMID(1,17,126), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.udp.fullsock */
  { (char *)&kna.udpstat.udps_fullsock, { PMID(1,17,127), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.udp.opackets */
  { (char *)&kna.udpstat.udps_opackets, { PMID(1,17,128), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

#if BEFORE_IRIX6_5

/* irix.network.udp.pcbcachemiss */
  { (char *)&kna.udpstat.udpps_pcbcachemiss, { PMID(1,17,129), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

#else /* IRIX 6.5 and later */

/* irix.network.udp.pcbcachemiss */
  { 0, { PMID(1,17,129), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },

#endif

/* irix.network.mbuf.alloc */
  { (char *)&kna.mbstat.m_mbufs, { PMID(1,17,130), PM_KNA_TYPE, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mbuf.clustalloc */
  { (char *)&kna.mbstat.m_clusters, { PMID(1,17,131), PM_KNA_TYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mbuf.clustfree */
  { (char *)&kna.mbstat.m_clfree, { PMID(1,17,132), PM_KNA_TYPE, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mbuf.failed */
  { (char *)&kna.mbstat.m_drops, { PMID(1,17,133), PM_KNA_TYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mbuf.waited */
  { (char *)&kna.mbstat.m_wait, { PMID(1,17,134), PM_KNA_TYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mbuf.drained */
  { (char *)&kna.mbstat.m_drain, { PMID(1,17,135), PM_KNA_TYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mbuf.typealloc */
  { (char *)&kna.mbstat.m_mtypes[0], { PMID(1,17,136), PM_KNA_TYPE, PM_INDOM_MBUF, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.badvers */
  { (char *)&kna.ipstat.ips_badvers, { PMID(1,17,137), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.ip.rawout */
  { (char *)&kna.ipstat.ips_rawout, { PMID(1,17,138), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

#if BEFORE_IRIX6_5

/* irix.network.mcr.mfc_lookups */
  { 0, { PMID(1,17,139), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mcr.mfc_misses */
  { 0, { PMID(1,17,140), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mcr.upcalls */
  { 0, { PMID(1,17,141), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mcr.no_route */
  { 0, { PMID(1,17,142), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mcr.bad_tunnel */
  { 0, { PMID(1,17,143), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mcr.cant_tunnel */
  { 0, { PMID(1,17,144), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mcr.wrong_if */
  { 0, { PMID(1,17,145), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mcr.upq_ovflw */
  { 0, { PMID(1,17,146), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mcr.cache_cleanups */
  { 0, { PMID(1,17,147), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mcr.drop_sel */
  { 0, { PMID(1,17,148), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mcr.q_overflow */
  { 0, { PMID(1,17,149), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mcr.pkt2large */
  { 0, { PMID(1,17,150), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mcr.upq_sockfull */
  { 0, { PMID(1,17,151), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },

#else /* IRIX 6.5 and later */

/* irix.network.mcr.mfc_lookups */
  { (char *)&kna.mrtstat.mrts_mfc_lookups, { PMID(1,17,139), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mcr.mfc_misses */
  { (char *)&kna.mrtstat.mrts_mfc_misses, { PMID(1,17,140), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mcr.upcalls */
  { (char *)&kna.mrtstat.mrts_upcalls, { PMID(1,17,141), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mcr.no_route */
  { (char *)&kna.mrtstat.mrts_no_route, { PMID(1,17,142), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mcr.bad_tunnel */
  { (char *)&kna.mrtstat.mrts_bad_tunnel, { PMID(1,17,143), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mcr.cant_tunnel */
  { (char *)&kna.mrtstat.mrts_cant_tunnel, { PMID(1,17,144), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mcr.wrong_if */
  { (char *)&kna.mrtstat.mrts_wrong_if, { PMID(1,17,145), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mcr.upq_ovflw */
  { (char *)&kna.mrtstat.mrts_upq_ovflw, { PMID(1,17,146), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mcr.cache_cleanups */
  { (char *)&kna.mrtstat.mrts_cache_cleanups, { PMID(1,17,147), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mcr.drop_sel */
  { (char *)&kna.mrtstat.mrts_drop_sel, { PMID(1,17,148), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mcr.q_overflow */
  { (char *)&kna.mrtstat.mrts_q_overflow, { PMID(1,17,149), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mcr.pkt2large */
  { (char *)&kna.mrtstat.mrts_pkt2large, { PMID(1,17,150), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mcr.upq_sockfull */
  { (char *)&kna.mrtstat.mrts_upq_sockfull, { PMID(1,17,151), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

#endif

#if BEFORE_IRIX6_4

/* irix.network.tcp.badsyn */
  { 0, { PMID(1,17,152), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.tcp.listendrop */
  { 0, { PMID(1,17,153), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.tcp.persistdrop */
  { 0, { PMID(1,17,154), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.tcp.synpurge */
  { 0, { PMID(1,17,155), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },

#else /* 6.4 or later */

/* irix.network.tcp.badsyn */
  { (char *)&kna.tcpstat.tcps_badsyn, { PMID(1,17,152), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.listendrop */
  { (char *)&kna.tcpstat.tcps_listendrop, { PMID(1,17,153), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.persistdrop */
  { (char *)&kna.tcpstat.tcps_persistdrop, { PMID(1,17,154), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.tcp.synpurge */
  { (char *)&kna.tcpstat.tcps_synpurge, { PMID(1,17,155), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

#endif

#if BEFORE_IRIX6_5
/* irix.network.mbuf.pcb.total */
  { 0, { PMID(1,17,156), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mbuf.pcb.bytes */
  { 0, { PMID(1,17,157), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mbuf.mcb.total */
  { 0, { PMID(1,17,158), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mbuf.mcb.bytes */
  { 0, { PMID(1,17,159), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
/* irix.network.mbuf.mcb.fail */
  { 0, { PMID(1,17,160), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
#else
/* irix.network.mbuf.pcb.total */
  { (char *)&kna.mbstat.m_pcbtot, { PMID(1,17,156), PM_KNA_TYPE, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mbuf.pcb.bytes */
  { (char *)&kna.mbstat.m_pcbbytes, { PMID(1,17,157), PM_KNA_TYPE, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mbuf.mcb.total */
  { (char *)&kna.mbstat.m_mcbtot, { PMID(1,17,158), PM_KNA_TYPE, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mbuf.mcb.bytes */
  { (char *)&kna.mbstat.m_mcbbytes, { PMID(1,17,159), PM_KNA_TYPE, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.mbuf.mcb.fail */
  { (char *)&kna.mbstat.m_mcbfail, { PMID(1,17,160), PM_KNA_TYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
#endif

#if BEFORE_IRIX6_5

/* irix.network.st.* */
  { 0, { PMID(1,17,161), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,162), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,163), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,164), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,165), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,166), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,167), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,168), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,169), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,170), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,171), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,172), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,173), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,174), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,175), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },
  { 0, { PMID(1,17,176), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, {0,0,0,0,0,0} } },

#else

/* irix.network.st.connattempt */
  { (char *)&kna.ststat.stps_connattempt, { PMID(1,17,161), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.accepts */
  { (char *)&kna.ststat.stps_accepts, { PMID(1,17,162), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.connects */
  { (char *)&kna.ststat.stps_connects, { PMID(1,17,163), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.drops */
  { (char *)&kna.ststat.stps_drops, { PMID(1,17,164), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.connfails */
  { (char *)&kna.ststat.stps_connfails, { PMID(1,17,165), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.closed */
  { (char *)&kna.ststat.stps_closed, { PMID(1,17,166), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.txtotal */
  { (char *)&kna.ststat.stps_txtotal, { PMID(1,17,167), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.datatxtotal */
  { (char *)&kna.ststat.stps_datatxtotal, { PMID(1,17,168), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.rxtotal */
  { (char *)&kna.ststat.stps_rxtotal, { PMID(1,17,169), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.datarxtotal */
  { (char *)&kna.ststat.stps_datarxtotal, { PMID(1,17,170), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.cksumbad */
  { (char *)&kna.ststat.stps_cksumbad, { PMID(1,17,171), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.oototal */
  { (char *)&kna.ststat.stps_oototal, { PMID(1,17,172), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.keyrejects */
  { (char *)&kna.ststat.stps_keyrejects, { PMID(1,17,173), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.txrejects */
  { (char *)&kna.ststat.stps_txrejects, { PMID(1,17,174), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.rxrejects */
  { (char *)&kna.ststat.stps_rxrejects, { PMID(1,17,175), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.network.st.slotdrops */
  { (char *)&kna.ststat.stps_slotdrops, { PMID(1,17,176), PM_KNA_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} } },

#endif

};

static int	nmeta = (sizeof(meta)/sizeof(meta[0]));
static int	fetched;
static int 	direct_map = 1;

void kna_init(int reset)
{
    int		i;
    int		indomtag;

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
        indomtag = meta[i].m_desc.indom;
        if (indomtag == PM_INDOM_NULL)
            continue;
        if (indomtag < 0 || indomtag >= PM_INDOM_NEXT) {
            __pmNotifyErr(LOG_ERR, "kna_init: bad instance domain (%d) for metric %s\n",
                         indomtag, pmIDStr(meta[i].m_desc.pmid));
            continue;
        }
        /* Replace tag with it's indom */
        meta[i].m_desc.indom = indomtab[indomtag].i_indom;
    }

    for (i = 0; i < nmeta; i++) {
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&kna));
	if (direct_map && meta[i].m_desc.pmid != PMID(1,17,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, "kna_init: direct map disabled @ meta[%d]", i);
	}
    }
}

void kna_fetch_setup(void)
{
    fetched = 0;
}

int kna_desc(pmID pmid, pmDesc *desc)
{
    int		i;

    if (direct_map) {
	__pmID_int	*pmidp = (__pmID_int *)&pmid;
	i = pmidp->item - 1;
	if (i < nmeta)
	    goto doit;
    }
    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
doit:
	    *desc = meta[i].m_desc;	/* struct assignment */
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

int kna_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int		i;
    int		k;
    int		sts;
    pmAtomValue	*avp;
    int		nval;
    static int	interesting[MT_MAX];

    if (direct_map) {
	__pmID_int	*pmidp = (__pmID_int *)&pmid;
	i = pmidp->item - 1;
	if (i < nmeta)
	    goto doit;
    }
    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
doit:
	    if (meta[i].m_desc.type == PM_TYPE_NOSUPPORT) {
	    	vpcb->p_nval = PM_ERR_APPVERSION;
	    	return 0;
	    }

	    if (fetched == 0) {
#ifdef MPSA_TCPIPSTATS
		if (sysmp(MP_SAGET, MPSA_TCPIPSTATS, &kna, sizeof(struct kna)) < 0) {
		    __pmNotifyErr(LOG_WARNING, "kna_fetch: %s", pmErrStr(-errno));
		    return -errno;
		}
		fetched = 1;
#else
		return PM_ERR_PMID;
#endif
	    }

	    switch (pmid) {
            /*
	     * This metric is special-cased because it is the only one with an indom.
	     */
	    case PMID(1,17,136):	/* irix.network.mbuf.typealloc */
              {
		int numinst = indomtab[PM_INDOM_MBUF].i_numinst;
		vpcb->p_nval = 0;
		for (k=0; k < numinst; k++) {
		    int inst = mbuftyp[k].i_id; 
		    if (interesting[inst] = __pmInProfile(meta[i].m_desc.indom, profp, inst))
			vpcb->p_nval++;
		}
		sizepool(vpcb);
		nval = 0;
		for (k=0; k < numinst; k++) {
		    int inst = mbuftyp[k].i_id; 
		    if (interesting[inst] == 0)
			continue;

		    avp = (pmAtomValue *)&((char *)&kna)[(ptrdiff_t)meta[i].m_offset+ inst*sizeof(kna.mbstat.m_mtypes[0])];

		    if ((sts = __pmStuffValue(avp, 0, &vpcb->p_vp[nval], meta[i].m_desc.type)) < 0)
			break;
		    vpcb->p_vp[nval++].inst = inst;
		}
		vpcb->p_valfmt = sts;
		return sts;
	      }
	    default: /* irix.network.anything_other_than_reqs */
		vpcb->p_nval = 1;
		vpcb->p_vp[0].inst = PM_IN_NULL;
		avp = (pmAtomValue *)&((char *)&kna)[(ptrdiff_t)meta[i].m_offset];
		if ((sts = __pmStuffValue(avp, 0, vpcb->p_vp, meta[i].m_desc.type)) < 0)
		    return sts;
		vpcb->p_valfmt = sts;
		/* success */
		return 0;
	    }
	}
    }
    return PM_ERR_PMID;
}
