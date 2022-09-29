/* TCP over FDDI SMT/CMT definitions
 *
 *
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.1 $"
#pragma once


#define MSEC_PER_HZ	    (1000/HZ)
#define USEC_PER_HZ	    (1000000/HZ)
#define SMT_USEC2HZ(val)    (((val)+USEC_PER_HZ-1)/USEC_PER_HZ)
#define SMT_MSEC2HZ(val)    (((val)+MSEC_PER_HZ-1)/MSEC_PER_HZ)

#define TPC_RESET(smt)	((smt)->smt_ops->reset_tpc((smt), (smt)->smt_pdata))

#define SMT_USEC_TIMER(smt,val) smt_timeout(smt,SMT_USEC2HZ(val))
#define SMT_MSEC_TIMER(smt,val) smt_timeout(smt,SMT_MSEC2HZ(val))

#define PCM_BIT(n)		(1<<(n))
#define PCM_R_BIT(smt,n)	(0 != ((smt)->smt_st.r_val & PCM_BIT(n)))



/* driver facilities used by CMT
 */
struct smt_ops {
	void	(*reset_tpc)();		/* reset TPC timer */
	time_t	(*get_tpc)();		/* get value of TPC timer */
	void	(*off)();		/* everything off */
	void	(*setls)();		/* set PHY state */
	void	(*lct_on)();		/* start LCT */
	int	(*lct_off)();		/* stop LCT and return result */
	void	(*join)();		/* join the ring */
	struct smt*(*trace)();		/* propagate trace */
	int	(*st_get)();		/* get current counter values */
	int	(*conf_set)();		/* set new configuration */
	void	(*alarm)();		/* awaken deamon */
	void	(*d_bec)();		/* start directed beacon */
};


enum rt_ls {
	R_PCM_QLS=0, R_PCM_ILS=1, R_PCM_MLS=2, R_PCM_HLS=3,
	R_PCM_ALS=4, R_PCM_ULS,   R_PCM_NLS,   R_PCM_LSU,

	T_PCM_QLS=16, T_PCM_ILS,  T_PCM_MLS,   T_PCM_HLS,
	T_PCM_ALS,    T_PCM_ULS,  T_PCM_NLS,   T_PCM_LSU,

	T_PCM_REP,			/* repeat as a cheap bypass */
	T_PCM_SIG,			/* CMT signalling */
	T_PCM_THRU,			/* THRU_{A,B} */
	T_PCM_WRAP,			/* WRAP_ {A,B} */
	T_PCM_WRAP_AB,			/* WRAP_AB */
	T_PCM_LCT,			/* doing LCT */
	T_PCM_LCTOFF,			/* turn off LCT */
	T_PCM_BREAK			/* seem to have entered BREAK */
};
#define SMT_LOG_LEN	128		/* should be a power of 2 */
#define SMT_LSLOG(smt,tr,newls) { \
	int lptr = (smt)->smt_st.log_ptr; \
	(smt)->smt_st.log[lptr].ls = ((newls)|(tr)*16); \
	(smt)->smt_st.log[lptr].lbolt = lbolt; \
	(smt)->smt_st.log_ptr = (lptr + 1) % SMT_LOG_LEN; \
}


/* configuration values for the kernel and the deamon parts of SMT
 */
struct smt_conf {
	SMT_MANUF_DATA	mnfdat;

	fdditimer2c	t_max;	/* FDDI timers */
	fdditimer2c	t_min;
	fdditimer2c	t_req;
	fdditimer2c	tvx;

	uint		ler_cut;	/* cut off link if LER exceeds this */

	enum fddi_st_type type;		/* our type */
	enum fddi_pc_type pc_type;

	u_char		debug;		/* debugging level */

	u_char		ip_pri;		/* LLC "priority" for IP frames */

	enum pcm_tgt	pcm_tgt;
};


/* Counters should have enough bits so that they will not "wrap" in less than
 *	one month or about 10**7 seconds.  This means that at least some
 *	counters, such as the count of total frames seen, should have
 *	more than 32 significant bits.  The FDDI SMT standard does not
 *	require counters to have more than 32 bits.  However, by additional
 *	significant bits here, we can make it possible to allow more useful
 *	counters in the future.
 */
struct smt_lcntr {			/* the shape of kernel SMT counters */
	unsigned long	hi;
	unsigned long	lo;
};



/* State, statistics and history */
struct smt_st {
	LFDDI_ADDR	addr;		/* station address--FDDI order  */

	fdditimer2c	t_neg;		/* negoticated token rotation time */
	enum mac_states mac_state;	/* recent MAC state */

	enum fddi_pc_type npc_type;	/* neighbor type */

	enum pcm_state	pcm_state;
	enum rt_ls	tls;		/* currently transmitted line state */
	enum pcm_ls	rls;		/* recently received line state */
	unsigned long	t_next;		/* signalling delay */
	unsigned long	flags;
	unsigned long	alarm;
	uint		n;		/* CMT bit number, "n" */
	unsigned long	r_val;
	unsigned long	t_val;

	uint		ler;		/* long term LER */
	uint		ler2;		/* recent, short term LER */

	unsigned long	lct_fail;	/* current LCT failures */
	unsigned long	lct_tot_fail;	/* total LCT failures */
	unsigned long	connects;	/* times thru BREAK without joining */

	int		log_ptr;
	struct smt_log {		/* log of line states */
	    unsigned	ls:5;
	    unsigned	lbolt:27;
	} log[SMT_LOG_LEN];


	/* many of these substantially under-count their events
	 */
	struct smt_lcntr rngop;		/* ring went non-RingOP to RingOP */
	struct smt_lcntr rngbroke;	/* ring went RingOP to non-RingOP */

	struct smt_lcntr multda;	/* frames with my DA & A set */
	struct smt_lcntr misfrm;	/* missed frame */

	struct smt_lcntr xmtabt;	/* line state change during xmit */

	struct smt_lcntr tkerr;		/* token received & MAC in T2 or T3 */
	struct smt_lcntr clm;		/* entered MAC T4 */
	struct smt_lcntr bec;		/* entered MAC T5 */
	struct smt_lcntr tvxexp;	/* TVX expired */
	struct smt_lcntr trtexp;	/* TRT expired and late count != 0 */
 	struct smt_lcntr tkiss;		/* tokens issued */

	struct smt_lcntr myclm;		/* my CLAIM frames received */
	struct smt_lcntr loclm;		/* lower CLAIM frames received */
	struct smt_lcntr hiclm;		/* higher CLAIM frames received */
	struct smt_lcntr mybec;		/* my beacon frames received */
	struct smt_lcntr otrbec;	/* other beacon frames */

	struct smt_lcntr dup_mac;	/* forged MAC frames with our DA */

	struct smt_lcntr frame_ct;	/* total frames seen */
	struct smt_lcntr tok_ct;	/* token count */
	struct smt_lcntr err_ct;	/* bad CRC or len; E-bit set here */
	struct smt_lcntr lost_ct;	/* FORMAC lost pulses */

				/* Errors in frames for us */
	struct smt_lcntr flsh;		/* flushed frames */
	struct smt_lcntr abort;		/* aborted frames */
	struct smt_lcntr miss;		/* gap too small */
	struct smt_lcntr error;		/* bad CRC or length */
	struct smt_lcntr e;		/* received E bit */
	struct smt_lcntr a;		/* received A bit */
	struct smt_lcntr c;		/* received C bit */
 	struct smt_lcntr tot_junk;	/* discarded frames */
 	struct smt_lcntr junk_void;	/* discarded void frames */
 	struct smt_lcntr junk_bec;	/* discarded beacon frames */
	struct smt_lcntr shorterr;	/* packet unreasonably short */
	struct smt_lcntr longerr;	/* packet unreasonably long */
	struct smt_lcntr rx_ovf;	/* receive FIFO overrun */
	struct smt_lcntr buf_ovf;	/* out of receive buffers */

	struct smt_lcntr tx_under;	/* transmit FIFO underrun */

	struct smt_lcntr eovf;		/* elasticity buffer overflow */
	struct smt_lcntr noise;		/* bad line state for a while */

 	struct smt_lcntr spare_cntr[7];	/* for future expansion */

	unsigned long	ring_latency;	/* ring latency in byte clocks */

	unsigned long	lem_count;	/* total LEM errors seen */
};


/* official flag bits found in smt_st.flags */
#define PCM_BS_FLAG	0x00000001	/* BS_Flag */
#define PCM_LS_FLAG	0x00000002	/* LS_Flag */
#define PCM_TC_FLAG	0x00000004	/* TC_Flag */
#define PCM_TD_FLAG	0x00000008	/* TD_Flag */
#define PCM_RC_FLAG	0x00000010	/* RC_Flag */
#define PCM_CF_JOIN	0x00000020	/* CF_Join */

#define PCM_WITHHOLD	0x00000040	/* do not attach */
#define PCM_THRU_FLAG	0x00000080	/* THRU */
#define PCM_PC_DISABLE	0x00000100	/* stay in PC_MAINT */
#define PCM_WA_FLAG	0x00000200	/* withhold A port connection */
#define PCM_WAT_FLAG	0x00000400	/* withhold A port connection to M */

#define PCM_LEM_FAIL	0x80000000	/* recently suffered LER_Cutoff */

/* less official */
#define PCM_NE		0x40000000	/* TNE>NS_Max happened */
#define PCM_TIMER	0x20000000	/* have a callout waiting */
#define PCM_RNGOP	0x10000000	/* ring is operational */

#define PCM_HAVE_BYPASS	0x08000000	/* have a bypass switch */

#define PCM_CON_U	0x04000000	/* undesirable connection */
#define PCM_CON_X	0x02000000	/* M to M connection */
/* #define unused	0x01000000	*/

#define PCM_DEBUG	0x00800000	/* turn on printf()s */

#define PCM_TRACE_REQ	0x00400000	/* have received a TRACE request */
#define PCM_TRACE_OTR	0x00200000	/* propagating TRACE from other PCM */
#define PCM_DRAG	0x00100000	/* dragging our heels in BREAK */


/* do not reset these */
#define PCM_SAVE_FLAGS	(PCM_WA_FLAG | PCM_WAT_FLAG | PCM_LEM_FAIL \
			 | PCM_DEBUG | PCM_HAVE_BYPASS)


#define PCM_SET(smt,b)	(0 != ((smt)->smt_st.flags & (b)))

/* synthetic PC_Mode bit, since we never have M ports ourself */
#define PC_MODE_P(smt)	((smt)->smt_st.npc_type != PC_M)
#define PC_MODE_T(smt)	((smt)->smt_st.npc_type == PC_M)


/* alarm bits to awaken demon */
#define	SMT_ALARM_RNGOP	0x00000001	/* ring died or recovered */
#define SMT_ALARM_LEM	0x00000002	/* many link errors seen */
#define SMT_ALARM_BS	0x00000004	/* stuck in BREAK */
#define SMT_ALARM_CON_X	0x00000008	/* illegal connection topology */
#define SMT_ALARM_CON_U	0x00000010	/* undesirable connection topology */
#define SMT_ALARM_CON_W 0x00000020	/* connection withheld */
#define SMT_ALARM_TFIN	0x00000040	/* PC-Trace finished */
#define SMT_ALARM_TBEG	0x00000080	/* PC-Trace started by neighbor */
#define SMT_ALARM_SICK	0x00000100	/* the hardware is sick */

/* lenght of mbuf used to awaken smt deamon */
#define SMT_LEN_ALARM	(sizeof(FDDIBUFLLC)+4)




/* pointer to user buffers for counters or configuration
 */
typedef struct smt_sioc {
	int	phy;			/* for configuration and state */
	union	{
		struct smt_conf *conf;
		struct smt_st	*st;
		unsigned long	alarm;

		enum pcm_ls	ls;	/* MAINT line state or ALS */
		u_char		*d_bec;	/* directed beacon INFO field */
	} smt_ptr_u;
	uint	len;
} SMT_SIOC;

#define SIOC_SMTGET_CONF    _IOW('S', 1, SMT_SIOC)
#define SIOC_SMTSET_CONF    _IOW('S', 2, SMT_SIOC)
#define SIOC_SMTGET_ST	    _IOW('S', 3, SMT_SIOC)
#define SIOC_SMT_TRACE	    _IOW('S', 4, SMT_SIOC)    /* force PC-Trace */
#define SIOC_SMT_ARM	    _IOWR('S',5, SMT_SIOC)    /* arm SMT notification */
#define	SIOC_SMT_MAINT	    _IOW('S', 6, SMT_SIOC)    /* send a line state */
#define SIOC_SMT_LEM_FAIL   _IOW('S', 7, SMT_SIOC)    /* reject line for LEM */
#define SIOC_SMT_DIRECT_BEC _IOW('S', 8, SMT_SIOC)    /* set special beacon */




/* everything needed by CMT and the kernel portion of SMT
 */
struct smt {
	caddr_t		smt_pdata;	/* board private data */
	struct smt_ops	*smt_ops;

	struct smt_conf smt_conf;	/* general configuration values */

	struct mbuf	*smt_alarm_mbuf;    /* drained to awaken deamon */

	int		smt_ktid;	/* current timer-ID for killing */
	int		smt_tid;	/* timer-ID for validating */

	struct smt_st	smt_st;		/* current state */
};


#ifdef _KERNEL
/* SMT/PCM functions called by drivers
 */
extern void smt_off(struct smt *smt);
extern void smt_clear(struct smt *smt);
extern uint smt_ler(struct timeval* lem_time, unsigned long ct);
extern int smt_ioctl(struct ifnet *ifp,
		     struct smt *smt1, struct smt *smt2,
		     int, struct smt_sioc* sioc);
extern void smt_pcm(struct smt *smt);


#define SMTDEBUGPRINT(smt,lvl,a) {if ((smt)->smt_conf.debug > (lvl) \
				      && PCM_SET(smt,PCM_DEBUG)) printf a;}

#endif /* _KERNEL */
