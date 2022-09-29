/*
 * Copyright 1998, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */

#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/types.h>
#include <net/raw.h>
#include <net/if.h>
#include <sys/syssgi.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <sys/tigon.h>
#include <sys/if_eg.h>

/* exit number that indicates different errors */
#define NO_ERROR           0
#define GENERIC_ERROR      1
#define SETNTRACE_ERROR    2
#define GETNTRACE_ERROR    3


unsigned int traceBuf[ALT_TRACE_SIZE*ALT_TRACE_ELEM_SIZE];
unsigned int dumpBuf[WINDOW_LEN];
int pflag=0;
int sflag=0;

static char *handler_names[32] = {
    "idle_time (SW0)",
    "h_mac_10_check (SW1)",
    "scream_handler (Mac Tx Comp)",
    "h_send_data_ready (SW2)",
    "h_recv_bd_ready (SW3)",
    "scream_handler (Mac Rx Start)",
    "h_mac_rx_comp (Mac Rx Comp)",
    "scream_handler (SW4)",
    "h_send_bd_ready (SW5)",
    "h_recv_bd_ready (SW6)",
    "scream_handler (SW7)",
    "scream_handler (SW8)",
    "h_dma_wr_assist_lo (DMA WR Asst Lo Comp)",
    "h_dma_rd_assist_lo (DMA RD Asst Lo Comp)",
    "h_dma_wr_assist_hi (DMA WR Asst Hi Comp)",
    "h_dma_rd_assist_hi (DMA RD Asst Hi Comp)",
    "scream_handler (SW9)",
    "scream_handler (DMA WR Comp)",
    "scream_handler (DMA RD Comp)",
    "h_dma_wr_attn (DMA WR Attn)",
    "h_dma_rd_attn (DMA RD Attn)",
    "scream_handler (SW10)",
    "mh_command (Mailbox)",
    "h_send_replicate (SW11)",
    "h_mac_10_negotiate (SW12)",
    "h_mac_rx_attn (Mac RX Attn)",
    "h_mac_tx_attn (Mac TX Attn)",
    "scream_handler (SW13)",
    "h_timer (TIMER)",
    "scream_handler (SW14)",
    "scream_handler (External Serial Data)",
    "scream_handler (SW15)"
};

void
Perror(char *string)
{
	fprintf(stderr, "egcmd: ERROR: %s: %s[%d].\n",
		string, strerror(errno), errno);
	return;
} 

void
usage(void)
{
	fprintf(stderr, "Usage: egcmd interface-name cmd-string\n");
	fprintf(stderr, "where \"cmd-string\" is:\n");

	fprintf(stderr, "\t DEBUG_SET  debug_mask_hex  debug_level\n");
	fprintf(stderr, "\t\t where \"debug_mask\" is a combination of:\n");
	fprintf(stderr, "\t\t\t 0x1  for  LINK debugging\n");
	fprintf(stderr, "\t\t\t 0x2  for  RECV debugging\n");
	fprintf(stderr, "\t\t\t 0x4  for  TRANSMIT debugging\n");
	fprintf(stderr, "\t\t\t 0x8  for  INTR debugging\n");
	fprintf(stderr, "\t\t\t 0x10 for  CMD debugging\n");
	fprintf(stderr, "\t\t \"debug_level\" is a value from 0-10\n");
	fprintf(stderr, "\t\t\t 0 is for no debug\n");
	fprintf(stderr, "\t\t\t higher values imply more debug prints\n\n");

	fprintf(stderr, "\t READ_MEM start-addr end-addr\n");
	fprintf(stderr, "\t\t where \"addresses are relative to NIC\n\n");

	fprintf(stderr, "\t TRACE_SET hex_val\n");
	fprintf(stderr, "\t\t where \"hex_val\" is a combination of:\n");
	fprintf(stderr, "\t\t\t 0x00000001  for  TRACE_TYPE_SEND\n");
	fprintf(stderr, "\t\t\t 0x00000002  for  TRACE_TYPE_RECV\n");
	fprintf(stderr, "\t\t\t 0x00000004  for  TRACE_TYPE_DMA\n");
	fprintf(stderr, "\t\t\t 0x00000008  for  TRACE_TYPE_EVENT\n");
	fprintf(stderr, "\t\t\t 0x00000010  for  TRACE_TYPE_COMMAND\n");
	fprintf(stderr, "\t\t\t 0x00000020  for  TRACE_TYPE_MAC\n");
	fprintf(stderr, "\t\t\t 0x00000040  for  TRACE_TYPE_STATS\n");
	fprintf(stderr, "\t\t\t 0x00000080  for  TRACE_TYPE_TIMER\n");
	fprintf(stderr, "\t\t\t 0x00000100  for  TRACE_TYPE_DISP\n");
	fprintf(stderr, "\t\t\t 0x00000200  for  TRACE_TYPE_MAILBOX\n");
	fprintf(stderr, "\t\t\t 0x00000400  for  TRACE_TYPE_RECV_BD\n");
	fprintf(stderr, "\t\t\t 0x00000800  for  TRACE_TYPE_LNK_PHY\n");
	fprintf(stderr, "\t\t\t 0x00001000  for  TRACE_TYPE_LNK_NEG\n\n");

	fprintf(stderr, "\t TRACE_READ \n");
	fprintf(stderr, "\t\t to read the current setting of trace value\n\n");

	fprintf(stderr, "\t TRACE_GET \n");
	fprintf(stderr, "\t\t to dump a NIC trace \n\n");

	fprintf(stderr, "\t PROFILE_GET \n");
	fprintf(stderr, "\t\t to dump a NIC profile \n\n");

	fprintf(stderr, "\t PROFILE_CLEAR \n");
	fprintf(stderr, "\t\t to clear the NIC profile\n\n");

	fprintf(stderr, "\t STAT_GET \n");
	fprintf(stderr, "\t\t to dump the NIC stats\n\n");

	fprintf(stderr, "\t STAT_CLEAR \n");
	fprintf(stderr, "\t\t to clear the NIC stats\n\n");

	fprintf(stderr, "\t PROMISC_SET \n");
	fprintf(stderr, "\t\t to set the NIC in promiscuous mode\n\n");

	fprintf(stderr, "\t PROMISC_UNSET \n");
	fprintf(stderr, "\t\t to get NIC out of promiscuous mode\n\n");

	exit(1);
}

static void
PrintMemBanner (void)
{
	char hostname[256];
	time_t timenow;
   
	(void) gethostname(hostname, sizeof(hostname));
	(void) time(&timenow);
	(void) printf("Alteon NIC  memory dump on `%s' at %s", hostname, ctime(&timenow));
}

static void
PrintMem(u_int *tbp, u_int saddr, int len)
{
	int 	i;
	int	swap=1;
	u_int 	ltmp;

	for (i=0; i < len; ) {

		(void) printf("%8.8x   ", saddr);

		if (swap) {
			ltmp = *tbp++;
			(void) printf("%8.8x ", ntohl(ltmp));
			ltmp = *tbp++;
			(void) printf("%8.8x ", ntohl(ltmp));
			ltmp = *tbp++;
			(void) printf("%8.8x ", ntohl(ltmp));
			ltmp = *tbp++;
			(void) printf("%8.8x ", ntohl(ltmp));
		} else {
			ltmp = *tbp++;
			(void) printf("%8.8x ", ltmp);
			ltmp = *tbp++;
			(void) printf("%8.8x ", ltmp);
			ltmp = *tbp++;
			(void) printf("%8.8x ", ltmp);
			ltmp = *tbp++;
			(void) printf("%8.8x ", ltmp);
		}
		(void) printf("\n");
		saddr += 16;
		i+= 16;
	}
}

static void
PrintTable(u_int *tbp)
{
	int i;
	u_int ltmp;

	/* XXX :  ted */	
	/*   int size = (nicTrace != -1) ? ALT_NTRACE_SIZE : ALT_TRACE_SIZE; */
	int size = ALT_NTRACE_SIZE;
	char hostname[256];
	time_t timenow;
   
	/* XXX ted */
	int swap = 1;

	(void) gethostname(hostname, sizeof(hostname));
	(void) time(&timenow);
	(void) printf("Alteon NIC  trace buffer on `%s' at %s",
                      hostname, ctime(&timenow));

	for (i=0; i < size; i++) {
		/* early break when we get done with buffer */
		if (*tbp == 0)
			break;
/*  XXX ted:  cpi = (char *)tbp; */
/*  XXX ted:	  len = 6 * sizeof(int); */

		(void) printf("%8.8s ", (char *)tbp);

		tbp += 2;
		ltmp = *tbp++;
		if (swap)
			(void) printf("%8.8x ", ntohl(ltmp));
		else
			(void) printf("%8.8x ", ltmp);
		ltmp = *tbp++;

		if (swap) {
			(void) printf("%8.8x ", ntohl(ltmp));
			ltmp = *tbp++;
			(void) printf("%8.8x ", ntohl(ltmp));
			ltmp = *tbp++;
			(void) printf("%8.8x ", ntohl(ltmp));
			ltmp = *tbp++;
			(void) printf("%8.8x ", ntohl(ltmp));
			ltmp = *tbp++;
			(void) printf("%8.8x ", ntohl(ltmp));
		} else {
			(void) printf("%8.8x ", ltmp);
			ltmp = *tbp++;
			(void) printf("%8.8x ", ltmp);
			ltmp = *tbp++;
			(void) printf("%8.8x ", ltmp);
			ltmp = *tbp++;
			(void) printf("%8.8x ", ltmp);
			ltmp = *tbp++;
			(void) printf("%8.8x ", ltmp);
		}
		(void) printf("\n");
	}
}

static void
PrintNicProfile(struct tg_stats *statp)
{
	U32 ticks = 0;
	int i;
	
	/* add up the total counts for ease of use */
	for (i=0; i<32; i++)
		ticks += statp->nicProfile[i];
	
	(void) printf("\nEvent Profiling:\n");
	for (i=31; i >= 0; i--)
		(void) printf("   %s: %u (%2.2f%%)\n",
			      handler_names[i],
			      statp->nicProfile[i],
			      (statp->nicProfile[i] * 100.)/ticks);
	
	(void) printf("\nTotal Ticks: %u\n", ticks);
}


static void
PrintStats(struct tg_stats *statp)
{
    U32 i;
    (void) printf("\nMAC statistics:\n");
    (void) printf("   dot3StatsAlignmentErrors: %u\n", 
		  statp->dot3StatsAlignmentErrors);
    (void) printf("   dot3StatsFCSErrors: %u\n", statp->dot3StatsFCSErrors);
    (void) printf("   dot3StatsSingleCollisionFrames: %u\n",
		  statp->dot3StatsSingleCollisionFrames);
    (void) printf("   dot3StatsMultipleCollisionFrames: %u\n",
		  statp->dot3StatsMultipleCollisionFrames);
    (void) printf("   dot3StatsSQETestErrors: %u\n",
		  statp->dot3StatsSQETestErrors);
    (void) printf("   dot3StatsDeferredTransmissions: %u\n",
		  statp->dot3StatsDeferredTransmissions);
    (void) printf("   dot3StatsLateCollisions: %u\n",
		  statp->dot3StatsLateCollisions);
    (void) printf("   dot3StatsExcessiveCollisions: %u\n",
		  statp->dot3StatsExcessiveCollisions);
    (void) printf("   dot3StatsInternalMacTransmitErrors: %u\n",
		  statp->dot3StatsInternalMacTransmitErrors);
    (void) printf("   dot3StatsCarrierSenseErrors: %u\n",
		  statp->dot3StatsCarrierSenseErrors);
    (void) printf("   dot3StatsFrameTooLongs: %u\n",
		  statp->dot3StatsFrameTooLongs);
    (void) printf("   dot3StatsInternalMacReceiveErrors: %u\n",
		  statp->dot3StatsInternalMacReceiveErrors);

    (void) printf("\nInterface statistics:\n");
    (void) printf("   ifIndex: %u\n", statp->ifIndex);
    (void) printf("   ifDescr: %s\n", statp->ifDescr);
    (void) printf("   ifType: %u\n", statp->ifType);
    (void) printf("   ifMtu: %u\n", statp->ifMtu);
    (void) printf("   ifSpeed: %u\n", statp->ifSpeed);
    (void) printf("   ifPhysAddress: ");

    for (i=0; i < 5; i++) {
	(void) printf("%x:", statp->ifPhysAddress.octet[i]);
    }
    (void) printf("%x\n", statp->ifPhysAddress.octet[i]);

    (void) printf("   ifAdminStatus: %u\n", statp->ifAdminStatus);
    (void) printf("   ifOperStatus: %u\n", statp->ifOperStatus);
    (void) printf("   ifLastChange: %u\n", statp->ifLastChange);
#if 0				/* deprecated */
    (void) printf("   ifInOctets: %u\n", statp->ifInOctets);
    (void) printf("   ifInUcastPkts: %u\n", statp->ifInUcastPkts);
    (void) printf("   ifInNUcastPkts: %u\n", statp->ifInNUcastPkts);
#endif /* 0 */
    (void) printf("   ifInDiscards: %u\n", statp->ifInDiscards);
    (void) printf("   ifInErrors: %u\n", statp->ifInErrors);
    (void) printf("   ifInUnknownProtos: %u\n", statp->ifInUnknownProtos);
#if 0				/* deprecated */
    (void) printf("   ifOutOctets: %u\n", statp->ifOutOctets);
    (void) printf("   ifOutUcastPkts: %u\n", statp->ifOutUcastPkts);
    (void) printf("   ifOutNUcastPkts: %u\n", statp->ifOutNUcastPkts);
#endif /* 0 */
    (void) printf("   ifOutDiscards: %u\n", statp->ifOutDiscards);
    (void) printf("   ifOutErrors: %u\n", statp->ifOutErrors);
    (void) printf("   ifOutQLen: %u\n", statp->ifOutQLen);
    (void) printf("   ifHCInOctets: %llu\n", statp->ifHCInOctets);
    (void) printf("   ifHCInUcastPkts: %llu\n", statp->ifHCInUcastPkts);
    (void) printf("   ifHCInMulticastPkts: %llu\n", 
		  statp->ifHCInMulticastPkts);
    (void) printf("   ifHCInBroadcastPkts: %llu\n", 
		  statp->ifHCInBroadcastPkts);
    (void) printf("   ifHCOutOctets: %llu\n", statp->ifHCOutOctets);
    (void) printf("   ifHCOutUcastPkts: %llu\n", statp->ifHCOutUcastPkts);
    (void) printf("   ifHCOutMulticastPkts: %llu\n", 
		  statp->ifHCOutMulticastPkts);
    (void) printf("   ifHCOutBroadcastPkts: %llu\n", 
		  statp->ifHCOutBroadcastPkts);
    (void) printf("   ifLinkUpDownTrapEnable: %u\n", 
		  statp->ifLinkUpDownTrapEnable);
    (void) printf("   ifHighSpeed: %u\n", statp->ifHighSpeed);
    (void) printf("   ifPromiscuousMode: %u\n", statp-> ifPromiscuousMode); 
    (void) printf("   ifConnectorPresent: %u\n", statp->ifConnectorPresent);

    (void) printf("\nInternal MAC RX statistics:\n");
    (void) printf("   nicMacRxLateColls: %u\n", statp->nicMacRxLateColls);
    (void) printf("   nicMacRxLinkLostDuringPkt: %u\n", 
		  statp->nicMacRxLinkLostDuringPkt);
    (void) printf("   nicMacRxPhyDecodeErr: %u\n", 
		  statp->nicMacRxPhyDecodeErr);
    (void) printf("   nicMacRxMacAbort: %u\n", statp->nicMacRxMacAbort);
    (void) printf("   nicMacRxTruncNoResources: %u\n", 
		  statp->nicMacRxTruncNoResources);
    (void) printf("   nicMacRxDropUla: %u\n", statp->nicMacRxDropUla);
    (void) printf("   nicMacRxDropMcast: %u\n", statp->nicMacRxDropMcast);
    (void) printf("   nicMacRxFlowControl: %u\n", statp->nicMacRxFlowControl);
    (void) printf("   nicMacRxDropSpace: %u\n", statp->nicMacRxDropSpace);
    (void) printf("   nicMacRxColls: %u\n", statp->nicMacRxColls);
    (void) printf("   nicMacRxTotalAttentions: %u\n", 
		  statp->nicMacRxTotalAttns);
    (void) printf("   nicMacRxLinkAttentions: %u\n", 
		  statp->nicMacRxLinkAttns);
    (void) printf("   nicMacRxSyncAttentions: %u\n", 
		  statp->nicMacRxSyncAttns);
    (void) printf("   nicMacRxConfigAttentions: %u\n", 
		  statp->nicMacRxConfigAttns);
    (void) printf("   nicMacReset: %u\n", statp->nicMacReset);
    (void) printf("   nicMacRxBufferAttentions: %u\n", 
		  statp->nicMacRxBufAttns);
    (void) printf("   nicMacRxZeroFrameCleanup: %u\n", 
		  statp->nicMacRxZeroFrameCleanup);
    (void) printf("   nicMacRxOneFrameCleanup: %u\n", 
		  statp->nicMacRxOneFrameCleanup);
    (void) printf("   nicMacRxMultipleFrameCleanup: %u\n", 
		  statp->nicMacRxMultipleFrameCleanup);
    (void) printf("   nicMacRxTimerCleanup: %u\n", 
		  statp->nicMacRxTimerCleanup);

    (void) printf("\nInternal MAC TX statistics:\n");
    (void) printf("   nicMacTxTotalAttentions: %u\n", 
		  statp->nicMacTxTotalAttns);
    (void) printf("\n   MAC TX Collision Histogram :\n");
    for (i= 0; i <15; i++) {
	(void) printf("   nicMacTxCollision[%d]: %u\n",i+1,  
		      statp->nicMacTxCollisionHistogram[i]);
    }

    (void) printf("\nLocal statistics:\n");
    (void) printf("\nHost Commands:\n");
    (void) printf("   nicCmdsHostState: %u\n", statp->nicCmdsHostState);
    (void) printf("   nicCmdsFDRFiltering: %u\n", statp->nicCmdsFDRFiltering);
    (void) printf("   nicCmdsSetRecvProdIndex: %u\n", 
		  statp->nicCmdsSetRecvProdIndex);
    (void) printf("   nicCmdsUpdateGencommStats: %u\n", 
		  statp->nicCmdsUpdateGencommStats);
    (void) printf("   nicCmdsAddMCastAddr: %u\n", statp->nicCmdsAddMCastAddr);
    (void) printf("   nicCmdsDelMCastAddr: %u\n", statp->nicCmdsDelMCastAddr);
    (void) printf("   nicCmdsSetPromiscMode: %u\n", 
		  statp->nicCmdsSetPromiscMode);
    (void) printf("   nicCmdsLinkNegotiate: %u\n", 
		  statp->nicCmdsLinkNegotiate);
    (void) printf("   nicCmdsSetMACAddr: %u\n", statp->nicCmdsSetMACAddr);
    (void) printf("   nicCmdsClearProfile: %u\n", statp->nicCmdsClearProfile);
    (void) printf("   nicCmdsSetMulticastMode: %u\n", 
		  statp->nicCmdsSetMulticastMode);
    (void) printf("   nicCmdsClearStats: %u\n", statp->nicCmdsClearStats);
    (void) printf("   nicCmdsSetRecvJumboProdIndex: %u\n", 
		  statp->nicCmdsSetRecvJumboProdIndex);
    (void) printf("   nicCmdsRefreshStats: %u\n", statp->nicCmdsRefreshStats);
    (void) printf("   nicCmdsUnknown: %u\n", statp->nicCmdsUnknown);

    (void) printf("\nNIC Events:\n");
    (void) printf("   nicEventsNICFirmwareOperational: %u\n",
		  statp->nicEventsNICFirmwareOperational);
    (void) printf("   nicEventsStatsUpdated: %u\n", 
		  statp->nicEventsStatsUpdated);
    (void) printf("   nicEventsLinkStateChanged: %u\n", 
		  statp->nicEventsLinkStateChanged);
    (void) printf("   nicEventsError: %u\n", statp->nicEventsError);
    (void) printf("   nicEventsMCastListUpdated: %u\n", 
		  statp->nicEventsMCastListUpdated);
    (void) printf("   nicRingSetSendProdIndex: %u\n", 
		  statp->nicRingSetSendProdIndex);
    (void) printf("   nicRingSetSendConsIndex: %u\n", 
		  statp->nicRingSetSendConsIndex);
    (void) printf("   nicRingSetRecvReturnProdIndex: %u\n", 
		  statp->nicRingSetRecvReturnProdIndex);
    (void) printf("   nicEventThresholdHit: %u\n", 
		  statp->nicEventThresholdHit);
    (void) printf("   nicSendThresholdHit: %u\n", statp->nicSendThresholdHit);
    (void) printf("   nicRecvThresholdHit: %u\n", statp->nicRecvThresholdHit);

    (void) printf("\nDMA Attentions:\n");
    (void) printf("   nicDmaRdOverrun: %u\n", statp->nicDmaRdOverrun);
    (void) printf("   nicDmaRdUnderrun: %u\n", statp->nicDmaRdUnderrun);
    (void) printf("   nicDmaWrOverrun: %u\n", statp->nicDmaWrOverrun);
    (void) printf("   nicDmaWrUnderrun: %u\n", statp->nicDmaWrUnderrun);
    (void) printf("   nicDmaRdMasterAborts: %u\n", 
		  statp->nicDmaRdMasterAborts);
    (void) printf("   nicDmaWrMasterAborts: %u\n", 
		  statp->nicDmaWrMasterAborts);
    
    (void) printf("\nRing/Misc. Resources:\n");
    (void) printf("   nicDmaWriteRingFull: %u\n", statp->nicDmaWriteRingFull);
    (void) printf("   nicDmaReadRingFull: %u\n", statp->nicDmaReadRingFull);
    (void) printf("   nicEventRingFull: %u\n", statp->nicEventRingFull);
    (void) printf("   nicEventProducerRingFull: %u\n", 
		  statp->nicEventProducerRingFull);
    (void) printf("   nicTxMacDescrRingFull: %u\n", 
		  statp->nicTxMacDescrRingFull);
    (void) printf("   nicTotalRecvBDs: %u\n", statp->nicTotalRecvBDs);
    (void) printf("   nicJumboRecvBDs: %u\n", statp->nicJumboRecvBDs);
    (void) printf("   nicStdRecvBDs: %u\n", statp->nicRecvBDs);
    (void) printf("   nicNoMoreRxBDs: %u\n", statp->nicNoMoreRxBDs);
    (void) printf("   nicNoMoreWrDMADescriptors: %u\n", 
		  statp->nicNoMoreWrDMADescriptors);
    (void) printf("   nicOutOfTxBufSpaceFrameRetry: %u\n", 
		  statp->nicOutOfTxBufSpaceFrameRetry);
    (void) printf("   nicNoSpaceInReturnRing: %u\n", 
		  statp->nicNoSpaceInReturnRing);

    (void) printf("\nHost Interrupts:\n");
    (void) printf("   nicInterrupts: %u\n", statp->nicInterrupts);
    (void) printf("   nicAvoidedInterrupts: %u\n", 
		  statp->nicAvoidedInterrupts);

}

int
do_command(char *interface, int cmd, int val, int val2)
{
	int                  error = NO_ERROR;
	int		     len;
	struct ifreq         ifr;
	struct eg_treq       *tfr;
	struct eg_dreq       *dfr;
	struct eg_mreq       *mfr;
	int                  socket_fd;
	struct sockaddr_raw  sr;
	struct tg_stats *tstats;

	if (interface == NULL) 
	  /* should never happen */
		return GENERIC_ERROR;

	if( (socket_fd = socket(AF_RAW, SOCK_RAW, RAWPROTO_SNOOP)) == -1){
		Perror("socket(AF_RAW)");
		error = GENERIC_ERROR;
		goto done;
	}

       	bzero((char *)&sr, sizeof(sr));
	sr.sr_family = AF_RAW;
	strcpy(sr.sr_ifname, interface);
	sr.sr_port = 0;
	if( bind( socket_fd, &sr, sizeof(sr) ) == -1 ){
		if (errno == EADDRNOTAVAIL) {
			fprintf(stderr, "egcmd: no such interface: %s\n",
				interface);
		} else {
			Perror("bind(AF_RAW)");
		}
		error = GENERIC_ERROR;
		goto done;
	}

	bzero((char*)&ifr, sizeof(ifr));
	strcpy(ifr.ifr_name, interface);

	switch (cmd) {

	case ALT_READMEM:
		mfr = (struct eg_mreq *)&ifr;
		mfr->egAddr = val;
		mfr->userAddr = (U64)dumpBuf;
		len=val2 - val; 
		
		if (len < 0)
			printf ("start address greater than end address\n");

		PrintMemBanner ();

		while (len > 0) {
			if (len > WINDOW_LEN) {
				mfr->len = (U32)(WINDOW_LEN);
				len -= WINDOW_LEN;
			} else {
				mfr->len = (U32)(len);
				len = 0;
			}

			bzero(dumpBuf, WINDOW_LEN);
			if( ioctl(socket_fd, ALT_READMEM, (caddr_t)&ifr) < 0){
			Perror("ioctl(ALT_READMEM)");
			error = GENERIC_ERROR;
			return error;
			}

			PrintMem(dumpBuf, mfr->egAddr, mfr->len);
			mfr->egAddr += WINDOW_LEN;
		}
		break;

	case ALT_GETNTRACE:
		tfr = (struct eg_treq *)&ifr;
		tfr->tbufp = (U64)traceBuf; 

		if( ioctl( socket_fd, ALT_GETNTRACE, (caddr_t)&ifr) < 0){
			Perror("ioctl(ALT_GETNTRACE)");
			error = GENERIC_ERROR;
			return error;
		}
		PrintTable(traceBuf);
		break;

	case ALT_SETTRACE:
		tfr = (struct eg_treq *)&ifr;
		tfr->cmd = val;

		if( ioctl( socket_fd, ALT_SETTRACE, (caddr_t)&ifr) < 0){
			Perror("ioctl(ALT_SETTRACE)");
			error = GENERIC_ERROR;
			return error;
		}
		break;

	case ALT_SETDEBUG:
		dfr = (struct eg_dreq *)&ifr;
		dfr->debug_mask = val;
		dfr->debug_level = val2;

		if( ioctl( socket_fd, ALT_SETDEBUG, (caddr_t)&ifr) < 0){
			Perror("ioctl(ALT_SETDEBUG)");
			error = GENERIC_ERROR;
			return error;
		}
		break;

	case ALT_GET_NIC_STATS:
		tfr = (struct eg_treq *)&ifr;
		tstats = (struct tg_stats *)malloc(sizeof(struct tg_stats));
		tfr->tbufp = (U64)tstats; 

		if( ioctl( socket_fd, ALT_GET_NIC_STATS, (caddr_t)&ifr) < 0){
			Perror("ioctl(ALT_GET_NIC_STATS)");
			error = GENERIC_ERROR;
			return error;
		}
		if (pflag)
		  PrintNicProfile(tstats);
		if (sflag)
		  PrintStats(tstats);

		free(tstats);
		break;

	case ALT_CLEARPROFILE:
		if( ioctl( socket_fd, ALT_CLEARPROFILE, (caddr_t)&ifr) < 0){
			Perror("ioctl(ALT_CLEARPROFILE)");
			error = GENERIC_ERROR;
			return error;
		}
		break;

	case ALT_CLEARSTATS:
		if( ioctl( socket_fd, ALT_CLEARSTATS, (caddr_t)&ifr) < 0){
			Perror("ioctl(ALT_CLEARSTATS)");
			error = GENERIC_ERROR;
			return error;
		}
		break;

	case ALT_SETPROMISC:
		if( ioctl( socket_fd, ALT_SETPROMISC, (caddr_t)&ifr) < 0){
			Perror("ioctl(ALT_SETPROMISC)");
			error = GENERIC_ERROR;
			return error;
		}
		break;

	case ALT_UNSETPROMISC:
		if( ioctl( socket_fd, ALT_UNSETPROMISC, (caddr_t)&ifr) < 0){
			Perror("ioctl(ALT_UNSETPROMISC)");
			error = GENERIC_ERROR;
			return error;
		}
		break;
		
        default :
		break;
	}



      done: 
	if (socket_fd != -1) close(socket_fd);

	return error;

}

void
main(int argc, char *argv[])
{
	char interface[10];
	int cmd;
	int val;
	int val2;
	int goodcmd = 1;

	if (argc < 3)
		usage();
	strcpy(interface, argv[1]);

	if (!strcmp(argv[2], "TRACE_SET")) {
		cmd = ALT_SETTRACE;
		if (argc != 4)
			usage();
		val = strtol(argv[3], NULL, 16);
		if ( val < TRACE_TYPE_SEND || val > TRACE_LEVEL_2)
			usage();
	} else if (!strcmp(argv[2], "DEBUG_SET")) {
		cmd = ALT_SETDEBUG;
		if (argc != 5)
			usage();
		val = strtol(argv[3], NULL, 16);
		val2 = strtol(argv[4], NULL, 10);
	} else if (!strcmp(argv[2], "TRACE_READ")) {
		cmd = ALT_READTRACE;
	} else if (!strcmp(argv[2], "READ_MEM")) {
		cmd = ALT_READMEM;
		if (argc != 5)
			usage();
		val = strtol(argv[3], NULL, 0);
		val2 = strtol(argv[4], NULL, 0);
	} else if (!strcmp(argv[2], "TRACE_GET")) {
		cmd = ALT_GETNTRACE;
	} else if (!strcmp(argv[2], "PROFILE_GET")) {
		cmd = ALT_GET_NIC_STATS;
		pflag=1;
	} else if (!strcmp(argv[2], "PROFILE_CLEAR")) {
		cmd = ALT_CLEARPROFILE;
	} else if (!strcmp(argv[2], "STAT_CLEAR")) {
		cmd = ALT_CLEARSTATS;
	} else if (!strcmp(argv[2], "PROMISC_SET")) {
		cmd = ALT_SETPROMISC;
	} else if (!strcmp(argv[2], "PROMISC_UNSET")) {
		cmd = ALT_UNSETPROMISC;
	} else if (!strcmp(argv[2], "STAT_GET")) {
		cmd = ALT_GET_NIC_STATS;
		sflag=1;
	} else {
		goodcmd = 0;
	}

	if (goodcmd) {
		do_command(interface, cmd, val, val2);
	} else {
		usage();
	}
	exit(0);
}
