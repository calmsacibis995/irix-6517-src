/* TCP over FDDI
 *
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 */

#ifndef __SYS_FDDI_H
#define __SYS_FDDI_H

#ident "$Revision: 1.1 $"


#include <sys/llc.h>


/* what the MIB calls a FddiTimerTwosComplement */
typedef long int fdditimer2c;

/* convert microseconds to 80-nanosecond byte-clocks */
#define USEC2FDDICLK(t) (((t)*100)/8)

/* limits on the fddi timers, in byte times translated from usec */
#define FDDI_MIN_TVX	USEC2FDDICLK(-2500)
#define FDDI_DEF_TVX 	USEC2FDDICLK(-4000)
#define FDDI_MAX_TVX	USEC2FDDICLK(-25000)

#define FDDI_MIN_T_MAX	USEC2FDDICLK(-165000)
#define FDDI_MIN_T_MIN	USEC2FDDICLK(-100)


#define FDDI_MINOUT sizeof(struct smac_frame)  /* smallest output frame */
#define FDDI_MAXINFO 4478		/* maximum Info bytes in a frame */
#define FDDI_MAXOUT (FDDI_MAXINFO \
		     +sizeof(struct lmac_hdr))

#define FDDI_MININ (FDDI_MINOUT+sizeof(FDDI_FCS))   /* smallest input frame */
#define FDDI_MAXIN (FDDI_MAXOUT+sizeof(FDDI_FCS))   /* largest frame */

#define FDDI_MIN_LLC (sizeof(struct fddi))	/* smallest we can do */

#define FDDIMTU (4096+256)		/* official FDDI MTU */


/* inidicators received with each frame in mac_bits */
#define MAC_E_BIT	0x10		/* E indicator */
#define MAC_A_BIT	0x08		/* A indicator */
#define MAC_C_BIT	0x04		/* C indicator */
#define MAC_ERROR_BIT	0x02		/* bad FCS or length */
#define MAC_DA_BIT	0x01		/* frame DA matched our DA */

/* Frame Control values */
#define MAC_FC_CLASS	0x80		/* C bit: 0=async, 1=sync */
#define MAC_FC_ALEN	0x40		/* L bit: 0=short, 1=long */
#define MAC_FC_MAC_FF	0x80
#define MAC_FC_SMT_FF	0x00
#define MAC_FC_LLC_FF	0x10
#define MAC_FC_IMP_FF	0x20
#define MAC_FC_VOID	0x00		/* void frame, without length bit */

#define MAC_FC_BEACON (MAC_FC_ALEN|MAC_FC_MAC_FF|2) /* long address beacon */
#define MAC_FC_CLAIM  (MAC_FC_ALEN|MAC_FC_MAC_FF|3) /* long address claim */
#define	MAC_FC_SMT    (MAC_FC_ALEN|MAC_FC_SMT_FF|1) /* simple SMT frame */
#define MAC_FC_LLC(p) (MAC_FC_ALEN|MAC_FC_LLC_FF|((p)&7))   /* async LLC */

#define MAC_FC_SMT_NSA	(MAC_FC_ALEN | MAC_FC_SMT_FF | 0xf)

#define	MAC_FC_IS_MAC(fc) (((fc) & 0xb0) == MAC_FC_MAC_FF)
#define MAC_FC_IS_VOID(fc) (((fc) & ~MAC_FC_ALEN) == MAC_FC_VOID)
#define	MAC_FC_IS_SMT(fc) (((fc)&0xb0) == MAC_FC_SMT_FF && !MAC_FC_IS_VOID(fc))
#define	MAC_FC_IS_LLC(fc) (((fc) & 0x78) == (MAC_FC_LLC_FF|MAC_FC_ALEN))
#define MAC_FC_IS_IMP(fc) (((fc) & 0x38) == MAC_FC_IMP_FF)


/* bits in 48-bit addresses--in "canonical bit order", not MAC hdr order */
#define MAC_DA_IG 0x01			/* 0=Individual or 1=Group */
#define MAC_DA_UL 0x02			/* 0=Universal or 1=Local */
/* same bits in FDDI or SWapped bit order */
#define MAC_DA_IG_SW 0x80
#define MAC_DA_UL_SW 0x40


/* is this FDDI-bit order string a group (either group or multi) address,
 *	a broadcast address, or a multicast address?
 */
#define FDDI_ISGROUP_SW(addr)	((addr)[0] & MAC_DA_IG_SW)
#define FDDI_ISBROAD_SW(addr)	FDDI_ISBROAD(addr)
#define FDDI_ISMULTI_SW(addr)	(FDDI_ISGROUP_SW(addr) \
	 && bcmp(addr, etherbroadcastaddr, sizeof etherbroadcastaddr))

#ifdef IRIX33 /* ugh */
#define	FDDI_ISGROUP(addr)	((addr)[0] & 0x01)
#else
#define FDDI_ISGROUP(addr)	ETHER_ISGROUP(addr)
#endif
#define FDDI_ISBROAD(addr)	ETHER_ISBROAD(addr)
#define FDDI_ISMULTI(addr) 	ETHER_ISMULTI(addr)


/* long and short addresses */
typedef struct {unsigned char b[6];} LFDDI_ADDR;
typedef struct {unsigned char b[2];} SFDDI_ADDR;

/* filler to align tcp packets */
#define FDDI_FILLER 3

/* minimum Info field */
typedef struct {unsigned long info[1];} FDDI_INFO;

/* the Frame Check Sequence */
typedef unsigned long FDDI_FCS;


/* the start of any FDDI frame (using long addresses) */
struct lmac_hdr {
	unsigned char	filler[FDDI_FILLER-1];	/* align things reasonably */
	unsigned char	mac_bits;	/* status bits on input */
	unsigned char	mac_fc;		/* Frame Control */
	LFDDI_ADDR	mac_da;		/* destination address */
	LFDDI_ADDR	mac_sa;		/* source address */
};

/* the start of any FDDI frame (using short addresses) */
struct smac_hdr {
	unsigned char	filler[FDDI_FILLER-1];
	unsigned char	mac_bits;
	unsigned char	mac_fc;
	SFDDI_ADDR	mac_da;
	SFDDI_ADDR	mac_sa;
};

/* a long MAC frame */
struct lmac_frame {
	struct lmac_hdr	macf_hdr;
	FDDI_INFO	macf_info;
};

/* a short MAC frame */
struct smac_frame {
	struct smac_hdr	macf_hdr;
	FDDI_INFO	macf_info;
};


/* the start of an FDDI frame using LLC, before any data */
struct fddi {
	struct lmac_hdr	fddi_mac;
	struct llc	fddi_llc;
};



/* "Drain port" numbers to select FDDI frames
 */
#define FDDI_PORT_SMT	1
#define FDDI_PORT_MAC	2
#define FDDI_PORT_IMP	3
#define FDDI_PORT_LLC	4		/* this will change with full DSAPs */


/* initial SGI IEEE ID, 8:0:69, in ethernet order */
#define SGI_MAC_SA0 0x08
#define SGI_MAC_SA1 0x00
#define SGI_MAC_SA2 0x69


/* FDDI SMT Multicast addresses, in FDDI bit order */
#define FDDI_MCAST_SRF	 {0x01, 0x80, 0xc2, 0x00, 0x01, 0x10}
#define FDDI_MCAST_D_BEC {0x01, 0x80, 0xc2, 0x00, 0x01, 0x00}
#define FDDI_MCAST_D_BEC03 ((0x01 << 24) + (0x80 <<16) + (0xc2 << 8) + 0x00)
#define FDDI_MCAST_D_BEC45 ((0x01 << 8) + 0x00)


/* Manufacturer Data
 */
typedef struct smt_manfdata {
	u_char	manf_oui[3];		/* Unique Id from IEEE */
	u_char	manf_data[29];		/* manufacturer defined data */
} SMT_MANUF_DATA;

#define SGI_FDDI_MNFDATA(ifname) {SGI_MAC_SA0,SGI_MAC_SA1,SGI_MAC_SA2, \
	"Silicon Graphics ifname"}



/* station type */
enum fddi_st_type {
	SAS,				/* Single Attachment Station */
	SAC,				/* Single Attachment Concentrator */
	SM_DAS,				/* single-MAC, Dual Attachment */
	DM_DAS				/* dual-MAC, Dual Attach */
};

enum fddi_pc_type {
	PC_A=0, PC_B=1, PC_S=2, PC_M=3, PC_UNKNOWN=4
};


/* target for the PCM machine */
enum pcm_tgt {
	PCM_DEFAULT=0,
	PCM_NONE,			/* either repeat or MAC-insert */
	PCM_CMT				/* run the state machine */
};


/* PHY line states */
enum pcm_ls {
	PCM_QLS=0, PCM_ILS=1, PCM_MLS=2, PCM_HLS=3,
	PCM_ALS=4, PCM_ULS,   PCM_NLS,   PCM_LSU
};
#define NUM_PCM_LS 8
/* one character labels for the enumeration for debugging printfs */
#define PCM_LS_LABELS {"Q", "I", "M", "H", "A", "U", "N", "L"}

enum mac_states {
	MAC_OFF,			/* hardware off */
	MAC_IDLE,			/* "MAC reset" */
	MAC_CLAIMING,
	MAC_BEACONING,
	MAC_ACTIVE			/* normal state */
};
#define NUM_MAC_STATES 5



/* PCM states similar to the official ones */
enum pcm_state {
	PCM_ST_OFF	=0,		/* PC0 */
	PCM_ST_BREAK	=1,		/* PC1 */
	PCM_ST_TRACE	=2,		/* PC2 */
	PCM_ST_CONNECT	=3,		/* PC3 */
	PCM_ST_NEXT	=4,		/* PC4 */
	PCM_ST_SIGNAL	=5,		/* PC5 */
	PCM_ST_JOIN	=6,		/* PC6 */
	PCM_ST_VERIFY	=7,		/* PC7 */
	PCM_ST_ACTIVE	=8,		/* PC8 */
	PCM_ST_MAINT	=9,		/* PC9 */

	PCM_ST_BYPASS	=20		/* waiting for bypass--EC5 or EC7 */
};


/* official SMT times, in microseconds */
#define SMT_A_MAX	200		/* max signal acquisition */
#define SMT_TL_MIN	30		/* transmit for this long */
#define SMT_C_MIN	(2*1000)	/* minimum time in CONNECT */
#define SMT_I_MAX	(25*1000)	/* optical bypass delays */
#define SMT_TB_MIN	(5*1000)	/* minimum BREAK time */
#define SMT_TB_MAX	(50*1000)	/* BREAK time until BS_Flag set */
#define	SMT_T_OUT	(100*1000)	/* signalling timeout */
#define SMT_T_NEXT9	(200*1000)	/* MAC Local Loop */
#define SMT_TRACE_MAX	(7*1000*1000)	/* wait for TRACE this long */
#define SMT_NS_MAX	1311		/* 1.31072 msec */

#define LC_SHORT	(50*1000)	/* LCT durations */
#define LC_MEDIUM	(500*1000)
#define LC_LONG		(5*1000*1000)
#define LC_EXTENDED	(50*1000*1000)


/* not official */
#define SMT_DRAG	(30*1000*1000)	/* stop signaling this long if
					 *	the other side is crazy
					 */


/* LEM values */
#define SMT_LER_CUT_DEF	7		/* default, min, & max LER_Cutoff */
#define SMT_LER_CUT_MIN 4		/* exponents */
#define SMT_LER_CUT_MAX 15
#define SMT_LER_ALM_DEF 8		/* LER_Alarm values */
#define SMT_LER_ALM_MIN SMT_LER_CUT_MIN
#define SMT_LER_ALM_MAX SMT_LER_CUT_MAX

#endif /* __SYS_FDDI_H */
