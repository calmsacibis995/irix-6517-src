/* TCP over FDDI SMT/CMT definitions
 *
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 */

#ifndef  __SYS_SMT_H
#define  __SYS_SMT_H
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.52 $"


#define USEC_PER_HZ	    (1000000/HZ)
#define SMT_USEC2HZ(val)    (((val)+USEC_PER_HZ-1)/USEC_PER_HZ)

#define TPC_RESET(smt)	((smt)->smt_ops->reset_tpc((smt), (smt)->smt_pdata))

#define SMT_USEC_TIMER(smt,val) smt_timeout(smt,SMT_USEC2HZ(val))

#define PCM_BIT(n)		(1<<(n))
#define PCM_R_BIT(smt,n)	(0 != ((smt)->smt_st.r_val & PCM_BIT(n)))



enum rt_ls {
	/* the R_* line states must match the official ones
	 */
	R_PCM_QLS=0, R_PCM_ILS=1, R_PCM_MLS=2, R_PCM_HLS=3,
	R_PCM_ALS=4, R_PCM_ULS,   R_PCM_NLS,   R_PCM_LSU,

	T_PCM_QLS,   T_PCM_ILS,   T_PCM_MLS,   T_PCM_HLS,
	T_PCM_ALS,   T_PCM_ULS,   T_PCM_NLS,   T_PCM_LSU,

	T_PCM_REP,			/* 16 repeat as a cheap bypass */
	T_PCM_SIG,			/* 17 CMT signalling */
	T_PCM_THRU,			/* 18 THRU_{A,B} */
	T_PCM_WRAP,			/* 19 WRAP_ {A,B} */
	T_PCM_WRAP_AB,			/* 20 WRAP_AB */
	T_PCM_LCT,			/* 21 doing LCT */
	T_PCM_LCTOFF,			/* 22 turn off LCT */

	T_PCM_OFF,			/* 23 */
	T_PCM_BREAK,			/* 24 */
	T_PCM_TRACE,			/* 25 */
	T_PCM_CONNECT,			/* 26 */
	T_PCM_NEXT,			/* 27 */
	T_PCM_SIGNAL,			/* 28 */
	T_PCM_JOIN,			/* 29 */
	T_PCM_VERIFY,			/* 30 */
	T_PCM_ACTIVE,			/* 31 */
	T_PCM_MAINT,			/* 32 */
	T_PCM_BYPASS,			/* 33 */

	SMT_LOG_X			/* 34 whatever the driver wants */
};


/* configuration values for the kernel and the deamon parts of SMT
 */
struct smt_conf {
	SMT_MANUF_DATA	mnfdat;

	fdditimer2c	t_max;		/* FDDI timers */
	fdditimer2c	t_min;
	fdditimer2c	t_req;
	fdditimer2c	tvx;

	__uint32_t	ler_cut;	/* cut off link if LER exceeds this */

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
 * XXX on 64-bit systems, make this __uint64
 */
struct smt_lcntr {			/* the shape of kernel SMT counters */
	__uint32_t  hi;
	__uint32_t  lo;
};

#define SMT_LOG_LEN	128		/* should be a power of 2 */
#define SMT_LSLOG(smt,newls) {				\
	int lptr = (smt)->smt_st.log_ptr;		\
	(smt)->smt_st.log[lptr].ls = (newls);		\
	(smt)->smt_st.log[lptr].lbolt = lbolt;		\
	(smt)->smt_st.log_ptr = (lptr + 1) % SMT_LOG_LEN; \
}


/* State, statistics and history */
struct smt_st {
	LFDDI_ADDR	addr;		/* station address--FDDI order  */

	fdditimer2c	t_neg;		/* negoticated token rotation time */
	enum mac_states mac_state;	/* recent MAC state */

	enum fddi_pc_type npc_type;	/* neighbor type */

	enum pcm_state	pcm_state;
	enum rt_ls	tls;		/* currently transmitted line state */
	enum pcm_ls	rls;		/* recently received line state */
	__uint32_t	t_next;		/* signalling delay */
	__uint32_t	flags;
	__uint32_t	alarm;
	__uint32_t	n;		/* CMT bit number, "n" */
	__uint32_t	r_val;
	__uint32_t	t_val;

	__uint32_t	ler;		/* long term LER */
	__uint32_t	ler2;		/* recent, short term LER */

	__uint32_t	lct_fail;	/* current LCT failures */
	__uint32_t	lct_tot_fail;	/* total LCT failures */
	__uint32_t	connects;	/* times thru BREAK without joining */

	__int32_t	log_ptr;
	struct smt_log {		/* log of line states */
	    unsigned    ls:6;
	    unsigned    lbolt:26;
	} log[SMT_LOG_LEN];


	/* many of these substantially under-count their events
	 * XXX  The first (and so all) of these smt_lcntr counters
	 * must be 8-byte aligned so that 64-bit programs can deal with
	 * them properly.
	 */
	struct smt_lcntr rngop;		/* ring went non-RingOP to RingOP */
	struct smt_lcntr rngbroke;	/* ring went RingOP to non-RingOP */

	struct smt_lcntr pos_dup;	/* weak evidence of duplicate addr */
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

	/*			Errors in frames for us */
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

	struct smt_lcntr viol;		/* violation symbols */

	struct smt_lcntr spare_cntr[6];	/* for future expansion */

	__uint32_t	ring_latency;	/* ring latency in byte clocks */

	__uint32_t	lem_count;	/* total LEM errors seen */
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
#define PCM_INS_BYPASS	0x04000000	/* bypass (being) inserted */

#define PCM_CON_U	0x02000000	/* undesirable connection */
#define PCM_CON_X	0x01000000	/* M to M connection */

#define PCM_DEBUG	0x00800000	/* turn on printf()s */

#define PCM_SELF_TEST	0x00400000	/* (when) finished tracing, test */
#define PCM_TRACE_OTR	0x00200000	/* propagating TRACE to other PHY */
#define PCM_DRAG	0x00100000	/* dragging our heels in BREAK */

#define	PCM_PHY_BUSY	0x00080000	/* hardware PCM is busy */
#define PCM_PC_START	0x00040000	/* force PCM_ST_OFF to PCM_ST_BREAK */

/* do not reset these */
#define PCM_SAVE_FLAGS	(PCM_WA_FLAG | PCM_WAT_FLAG | PCM_LEM_FAIL \
			 | PCM_HAVE_BYPASS | PCM_INS_BYPASS	\
			 | PCM_DEBUG | PCM_SELF_TEST)


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

/* length of mbuf used to awaken smt deamon */
#define SMT_LEN_ALARM	(sizeof(FDDI_IBUFLLC)+4)



/* pointer to user buffers for counters or configuration
 */
typedef struct smt_sioc {
	__uint32_t  phy;		/* for configuration and state */
	union	{
#ifdef _KERNEL
		__uint32_t	u32ptr;	/* 32-bit user-code pointer */
#else
#if _MIPS_SIM == _ABI64
		/* this interface is only for 32-bit mode programs */
#else
		struct smt_conf *conf;
		struct smt_st	*st;
		void		*d_bec;	/* directed beacon INFO field */
#endif
#endif
		__uint32_t	alarm;

		enum pcm_ls	ls;	/* MAINT line state or ALS */
	} smt_ptr_u;
	__uint32_t  len;
} SMT_SIOC;

#define SIOC_SMTGET_CONF    _IOW('S', 1, SMT_SIOC)
#define SIOC_SMTSET_CONF    _IOW('S', 2, SMT_SIOC)
#define SIOC_SMTGET_ST	    _IOW('S', 3, SMT_SIOC)
#define SIOC_SMT_TRACE	    _IOW('S', 4, SMT_SIOC)  /* force PC-Trace */
#define SIOC_SMT_ARM	    _IOWR('S',5, SMT_SIOC)  /* arm SMT notification */
#define	SIOC_SMT_MAINT	    _IOW('S', 6, SMT_SIOC)  /* send a line state */
#define SIOC_SMT_LEM_FAIL   _IOW('S', 7, SMT_SIOC)  /* reject line for LEM */
#define SIOC_SMT_DIRECT_BEC _IOW('S', 8, SMT_SIOC)  /* set special beacon */



#ifdef _KERNEL
/* driver facilities used by CMT
 */
struct smt;				/* make ANSI C happy */
struct d_bec_frame;
struct smt_ops {
#define SOP struct smt*, void*
	void	(*reset_tpc)(SOP);	/* reset TPC timer */
	time_t	(*get_tpc)(SOP);	/* get value of TPC timer */
	void	(*off)(SOP);		/* everything off */
	void	(*setls)(SOP,enum rt_ls);   /* set PHY state */
	void	(*lct_on)(SOP);		/* start LCT */
	int	(*lct_off)(SOP);	/* stop LCT and return result */
	void	(*cfm)(SOP);		/* join the ring */
	int	(*st_get)(SOP);		/* get current counter values */
	int	(*conf_set)(SOP,struct smt_conf*);  /* set configuration */
	void	(*alarm)(SOP,u_long);	/* awaken deamon */
	void	(*d_bec)(SOP, struct d_bec_frame*, int);    /* beacon */
	struct smt*(*osmt)(SOP);	/* help propagate trace */
	void	(*trace_req)(SOP);

	void	(*setvec)(SOP, int);	/* send bit vector */
	void	(*pc_join)(SOP);	/* go from PC_NEXT to PC_JOIN */
	void	(*pcm)(struct smt*);	/* run PCM */
#undef SOP
};
#endif



/* everything needed by CMT and the kernel portion of SMT
 */
struct smt {
	caddr_t		smt_pdata;	/* board private data */
#ifdef _KERNEL
	struct smt_ops	*smt_ops;
#else
	void		*smt_ops;
#endif

	struct smt_conf smt_conf;	/* general configuration values */

	struct mbuf	*smt_alarm_mbuf;    /* drained to awaken deamon */

	toid_t		smt_ktid;	/* current timer-ID for killing */
	__int32_t	smt_tid;	/* timer-ID for validating */

	struct smt_st	smt_st;		/* current state */
};


#ifdef _KERNEL
extern int smt_debug_lvl;

extern int smt_max_connects;		/* give up if cannot join by this */


/* SMT/PCM/CMT functions called by drivers
 */
extern void smt_off(struct smt*);
extern void smt_clear(struct smt*);
extern void smt_fin_trace(struct smt*, struct smt*);
extern void smt_new_st(struct smt*, enum pcm_state);
extern uint smt_ler(struct timeval*, __uint32_t);
extern int smt_ioctl(struct ifnet*, struct smt*, struct smt*,
		     int, struct smt_sioc*);
extern void smt_timeout(struct smt*, int);
extern void smt_ck_timeout(struct smt*, int);
extern void host_pcm(struct smt*);
extern void moto_pcm(struct smt*);


extern char* smt_pc_label[];
#define PC_LAB(smt)	smt_pc_label[(int)(smt)->smt_conf.pc_type]

#define SMTDEBUGPRINT(smt,lvl,a) {if ((smt)->smt_conf.debug > (lvl) \
				      && PCM_SET(smt,PCM_DEBUG)) printf a;}

#endif /* _KERNEL */
#ifdef __cplusplus
}
#endif
#endif /* __SYS_SMT_H */
