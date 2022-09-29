/*
 * Copyright 1989, 1992, Silicon Graphics, Inc.
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

#ident	"$Revision: 1.105 $"

/* Definitions for external major numbers. */

#define HWGRAPH_MAJOR	0	/* Reserved for hwgraph dev_t's */
#define	MM_MAJOR	1	/* /dev/mem & /dev/kmem */
#define	SY_MAJOR	2
#define	RD_MAJOR	3
#define	SCSIBUS_MAJOR	5	/* SCSI bus and FC loop operations */
#define	PRF_MAJOR	7	/* Kernel profiler */
#define	CDSIO_MAJOR	8	/* serial I/O board */
#define	VIDEO1_MAJOR	9	/* First Video Driver */
#define	SVIDEO_MAJOR	VIDEO1_MAJOR
#define	CLN_MAJOR	10	/* streams clone device */
#define	HUBSPC_MAJOR	11	/* SN0 hubspc device */
#define	TCO_MAJOR	12	/* TLI connection-oriented loopback device */
#define	TCL_MAJOR	13	/* TLI connectionless loopback device */
#define	PTC_MAJOR	14	/* PTY controller--see also PTC*_MAJOR */
#define	PTS_MAJOR	15
#define	IKC_MAJOR	16	/* Ikon color printer driver */
#define TPITCP_MAJOR	18	/* TPI TCP    clonable STREAMS device */
#define TPIUDP_MAJOR	19	/* TPI UDP    clonable STREAMS device */
#define TPIIP_MAJOR	20	/* TPI raw IP clonable STREAMS device */
#define TPIICMP_MAJOR	21	/* TPI ICMP   clonable STREAMS device */
#define	TCOO_MAJOR	24	/* TLI connection oriented orderly release dev*/
#define EPCEI_MAJOR	25	/* EPC external interrupts driver */
#define CSIO_MAJOR	26	/* character serial i/o */
#define	LV_MAJOR	30	/* logical volume disk driver. */

#define	SLIP_MAJOR	31	/* SLIP */
#define	GPIB_MAJOR	32

#define SAD_MAJOR	33	/* STREAMS Administrative Driver */

#define	WSTY_MAJOR	34	/* WAN Sync pseudo TTY driver */
#define	GSE_MAJOR	35	/* ICA driver */
#define	PRIXP_MAJOR	36	/* PRI-48XP ISDN */

#define	ZERO_MAJOR	37	/* /dev/zero - mapped shared mem driver */
#define	MCIOPLP_MAJOR	38	/* PI printer */

/*	39 was PI audio (MCIOAUD_MAJOR) and is no longer used */

#define	OLDSMFD_MAJOR	40	/* SCSI floppy driver (Scientific Microsystems
				 * 5 1/4, Konica ST-510 10.7Mb, and NCR 5 1/4"
				 * and 3 1/2" drives on SCSI ctlr 0 */
#define	KLOG_MAJOR	41	/* Kernel error logging driver */
#define	IMON_MAJOR	42	/* inode monitor driver */
#define	FSCTL_MAJOR	45	/* EFS pseudo-device */
#define TPORT_MAJOR	46	/* Textport Emulation on graphics */
#define USEMA_MAJOR	47	/* pollable semaphore device */

#define	PPP_MAJOR	52	/* PPP */
#define	HDSP_MAJOR	53	/* Audio (hdsp & kdsp drivers) */
#define SHMIQ_MAJOR	54	/* Shared mem input queue (for window system) */
#define QCNTL_MAJOR	55	/* Shared mem input queue (character device) */
#define GFXS_MAJOR	56	/* graphics driver streams interface */
#define GFX_MAJOR	57	/* graphics driver */
#define CONSOLE_MAJOR	58	/* console driver */
#define IPFILT_MAJOR	59	/* IP packet filtering */


/*		60-79	Are reserved for Customer Use   */

#define STRLOG_MAJOR	80	/* STREAMS log device */
#define RTE_MAJOR	87


#define PTC1_MAJOR	104	/* additional PTYs */
#define PTS1_MAJOR	105
#define PTC2_MAJOR	106
#define PTS2_MAJOR	107
#define PTC3_MAJOR	108
#define PTS3_MAJOR	109
#define PTC4_MAJOR	110
#define PTS4_MAJOR	111

#define MFS_MAJOR	112
#define MFS1_MAJOR	113
#define MFS2_MAJOR	114
#define MFS3_MAJOR	115

#define SNATR_MAJOR	116	/* IBM SNA Token Ring stream pseudo-driver */

#define	SNIF_MAJOR	117	/* SNIF STREAMS Network Interface */
#define TMR_MAJOR	118	/* OSI STREAMS timer */
#define COX_MAJOR	119	/* OSI TP0/2/4 WAN */

#define	PLEX_CMAJOR	120	/* IRIS Volume Manager plex char intf */
#define	VOL_BMAJOR	121	/* IRIS Volume Manager volume block intf */
#define	VOL_CMAJOR	121	/* IRIS Volume Manager volume char intf */
#define	VOL_SPECMAJOR	122	/* IRIS Volume Manager control intf (char) */

#define COTS_MAJOR	123	/* OSI TP4 LAN */
#define DLR_MAJOR	124	/* OSI DLPI LAN */
#define SNDC_MAJOR	125	/* OSI SNDCF */
#define IP_X25_MAJOR	126	/* IP to SX.25 */
#define INACTIVE_MAJOR	127	/* OSI Inactive Subset */
#define SMFD_MAJOR	160
/* 160 to 175 reserved for future SMFD use */
#define USRAID_MAJOR	176	/* UltraStor RAID disk driver */
/* 176 to 191 reserved for future USRAID use */
#define XLV_MAJOR	192	/* XLV logical volume manager */
#define XLV_LOWER_MAJOR	193	/* XLV lower driver */
#define INDYCOMP_MAJOR	194	/* IndyComp */

#define	PCKM_MAJOR	197
#define USRVME_MAJOR	198	/* user level VME */
#define SYSCTLR_MAJOR	199	/* EVEREST System Controller serial driver */
#define SYNC_MAJOR	200	/* DMEDIA ism services sync driver */

#define MIDI_MAJOR	201	/* MIDI streams multiplexor */
#define IMIDI_MAJOR	202	/* software MIDI pseudo device */
#define	VIDEO2_MAJOR	203	/* Second Video Driver */

#define FLASH_MAJOR	204	/* Flash PROM driver (primarily for Everest) */
#define EXTINT_MAJOR	205	/* External interrupt driver (Everest) */
#define PW_MAJOR	206	/* Oracle Postwait driver */

#define	SDLC_MAJOR	208	/* SDLC driver */
#define	X25_VC_MAJOR	209	/* SX.25 to VC conversion driver */
#define	XTY_MAJOR	210	/* Pseudo tty for uucp/cu over PAD */
#define X25_MAJOR	211	/* X25 PLP */
#define LAPB_MAJOR	212	/* Link Access Protocol Balanced */
#define LLC2_MAJOR	213	/* Logical Link Control */
#define IXE_MAJOR	214	/* IP over X.25		*/
#define X25_VME_MAJOR	215	/* X25 VME driver */
#define X25_GIO_MAJOR	216	/* X25 GIO driver */
#define X25_LOOP_MAJOR	217	/* X25 loop back driver */

#define	LATS_MAJOR	218	/* Local Area Transport */
#define	DN_LL_MAJOR	219	/* 4DDN logical link driver */
#define	DN_NETMAN_MAJOR 220	/* 4DDN network management */
#define DN_CUSTK_MAJOR	221	/* 4DDN Stream Stack	*/
#define DN_TPI_MAJOR	222	/* 4DDN TPI		*/
#define	DN_LOOP_MAJOR	223	/* 4DDN loop back driver */
#define DN_DUMP_MAJOR	224	/* 4DDN Dump module	*/

#define ISDNSME_MAJOR	225	/* isdn system managment driver */
#define NDLPI_MAJOR	226	/* isdn null dlpi driver (for test) */
#define LDLPI_MAJOR	227	/* isdn loop-back dlpi driver (for test) */
#define LAPD_MAJOR	228	/* isdn lapd dlpi driver */
#define SPLI_MAJOR	229	/* isdn streams-pli driver */
#define Q931MUX_MAJOR	230	/* isdn q931 appl multiplexor */

#define	RAM_MAJOR	231	/* ram block device */
#define USRDMA_MAJOR	232	/* user level DMA */

#define VIGRA110_MAJOR	233	/* Vigra110 device dependent AL driver     */
#define VIGRA210_MAJOR	234	/* Vigra210 device dependent AL driver     */
#define INDIGO_MAJOR	235	/* Indigo Audio device dependent AL driver */
#define A2_MAJOR	236	/* A2 Audio device dependent AL driver     */

#define ATM_MAJOR	237	/* ATM interface */

#define ASOSER_MAJOR	238	/* ASO serial STREAMS driver */
#define ASOSERNS_MAJOR	239	/* ASO serial non-STREAMS driver */

#define XLV_GEN_MAJOR	246	/* XLV generic disk device */

#define RIO_MAJOR	247	/* Remote IO */

#define DMS_MAJOR	248	/* Digital Media Streams driver */
#define DMRB_MAJOR	249	/* Digital Media Ring Buffer driver */

#define HIPPI_MAJOR	251	/* HIPPI interface */
#define	CCSYNC_MAJOR	252	/* EVEREST CC sync driver */
#define	TSD_MAJOR	253	/* Kernel telnetd streams devices */
#define COSMO_MAJOR	254	/* Cosmo Compression GIO option board */
#define SWIPE_MAJOR	255	/* Gauntlet swIPe encryption */
#define	VIDEO3_MAJOR	256	/* Third Video Driver */
#define	COSMO2_MAJOR	257	/* IMPACT Compression option board (ISD) */
#define	NDSM_MAJOR	258	/* network dualhead pseudo-mouse driver.  */
#define	NDSK_MAJOR	259	/* network dualhead pseudo-keyboard driver.  */
#define	DU_MAJOR	260	/* DUART driver */

#define DPIPE_MAJOR     261     /* Peer-to-Peer Pipe driver */
#define FSPE_MAJOR      262     /* File system pipe end driver */
#define LNSHM_MAJOR     263     /* SN0net shared-memory driver */
#define	DVS_MAJOR	264	/* DVS Video driver (ASD digital video I/O) */
#define	XCONN_MAJOR	265	/* Kernel support for X local transport */
#define	KRPCH_MAJOR	266	/* DFS kernel RPC driver */
#define	NCS_MAJOR	267	/* DFS kernel debug driver */



/* GIO bus IDs have moved to sys/giobus.h */
