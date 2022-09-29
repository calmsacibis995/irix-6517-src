/*
 * PCI vendor and chip revision ID
 */
#define FCADP_VENDID 0x9004
#define FCADP_DEVID 0x1160

/*
 * Maximum sizes and amounts of things
 */
#define FC_LILP_SIZE 132	/* size of lip position map */
#define FCADP_MAPSIZE 128	/* number of default map entries - 128 min */
#define FCADP_MAXSENSE 256	/* amount of sense data allocated */

#define FCADP_MAXADAP 96
#define FCADP_MAXLUN 2048
#define FCADP_MAXTARG 110
#define FCADP_MAXLPORT 127
#define FCADP_MAX_TACHYON_TARG 8

#define LIPTIMEOUT (3 * HZ)
#define LOSLIPTIMEOUT (15 * HZ)
#define FCADP_PLOGI_RETRY 2

/*
 * We need to reserve room on the controller for a primitive TCB,
 * a LIP TCB, an ADISC or PLOGI TCB, and an ABORT TCB, in case of
 * trouble, so we reduce the pool of available TCBs for normal commands to
 * account for these with the following define.
 * For now, we do not issue ABORT TCBs.
 * If ABORTs or ADISCs are desired, we may need to reserve 1 of each per target.
 */
#define FCADP_LNKSVC_SETASIDE 1
#define FCADP_LIP_SETASIDE 1
#define FCADP_PRIMITIVE_SETASIDE 1
#define FCADP_TCB_SETASIDE \
	(FCADP_LNKSVC_SETASIDE + FCADP_LIP_SETASIDE + FCADP_PRIMITIVE_SETASIDE)
#define FCADP_MAX_UNSOL_TCB 3	/* max # of available unsol TCBs */
#define FCADP_MAXCMD 512	/* max # of outstanding commands */

struct fc_tcbhdr
{
	ushort			 th_number;
	uint			 th_bigmap:1,	/* bigmap used for this tcb */
				 th_allocmap:1,	/* allocated map for tcb */
				 th_type:4;
	uint8_t			 th_target;	/* target id */
	ushort			 th_ticker;
	ushort			 th_ticklimit;
	struct _tcb		*th_tcb;
	SH_SG_DESCRIPTOR	*th_map;
	u_char			*th_sense;
	struct scsi_request	*th_req;
	struct fc_tcbhdr	*th_next;
};

/*
 * Constants for fc_tcbhdr.th_type.  The primitive TCB types are disjoint from
 * the rest, since the primitives are delivered through a reserve TCB.
 */
#define TCBTYPE_IDLE 0
#define TCBTYPE_LIP 1
#define TCBTYPE_ADISC 2
#define TCBTYPE_PLOGI 3
#define TCBTYPE_PRLI 4
#define TCBTYPE_SCSI 5
#define TCBTYPE_UNSOL 6
#define TCBTYPE_ABTS 7
#define TCBPRIM_LIPF7 1
#define TCBPRIM_LIPRST 2
#define TCBPRIM_LPB 3
#define TCBPRIM_LPE 4
#define TCBPRIM_LPEALL 5


struct fcadpluninfo
{
	u_char		number;
	u_char		present,	/* lun is present */
			exclusive;	/* lun is alloc'd exclusive */
	ushort			 refcount;	/* number of allocs active */
	void			(*sense_callback)();
	struct scsi_target_info	 tinfo;
};

struct fcadptargetinfo
{
	uint8_t		number;
	u_char		needplogi,
			needprli,
			needadisc,
			plogi,		/* plogi done */
			prli,		/* prli done */
			abts_all,	/* abort all active commands */
			login_failed,
			present,	/* 1=in hwgraph 0=not in graph */
			check_pending,	/* 1=target gone after lip, waiting
					   for it to reappear */
			disappeared,	/* 1=didn't reappear when check_pending */
			q_state;	/* See FCTI_STATE_* defines below */
	ushort			 timeout_progress;
	toid_t			 check_pending_id;
	struct scsi_request	*waithead;	/* first waiting scsirequest */
	struct scsi_request	*waittail;	/* last waiting scsirequest */
	struct fcadptargetinfo	*scsi_next;	/* next target with SCSI cmd to do */
	struct fcadptargetinfo	*lnksvc_next;	/* next target with link svc cmd to do */
	struct fcadptargetinfo	*abts_next;	/* next target with ABTS cmd to do */
	struct fc_tcbhdr	*active;	/* queue of TCBs that are active */
	struct fc_tcbhdr	*abort;		/* TCB that needs aborting */
	struct fc_plogi_payload	*plogi_payload;
	struct fc_prli_payload	*prli_payload;
	uint8_t		 	 lunmask[FCADP_MAXLUN / 8];
	mutex_t			 opensema;
};

/*
 * The FCTI_* #defines are for the fcadptargetinfo.q_state field, which
 * refers to the state of the target with respect to issuing new commands.
 * It does not indicate anything with respect to active commands.
 *
 * The idle state should indicate that no commands are waiting for issue.
 *   When one is queued, it can be immediately issued pending available tcbs.
 *   The target is not in the controller wait queue (fcadpctlrinfo.waithead
 *   and fcadpctlrinfo.waittail).
 * The queued state indicates that commands are queued and will be issued
 *   when tcbs are available.  The target is in the controller wait queue.
 * The quiesce state indicates that no commands are to be issued in the
 *   target.  The target is not in the controller wait queue.  There may
 *   be commands queued for the target.  If so, then the target will be
 *   placed into the controller wait queue when the quiesce condition is
 *   cleared.
 */
#define FCTI_STATE_IDLE 0	/* target not in controller queue - no cmds waiting */
#define FCTI_STATE_QUEUED 1	/* target is in controller wait queue */
#define FCTI_STATE_QUIESCE 2	/* target not in controller queue even if cmds waiting */
#define FCTI_STATE_AGE 4	/* target command aging timeout */

struct target_queue
{
	struct fcadptargetinfo	*head;
	struct fcadptargetinfo	*tail;
};

struct fcadpctlrinfo
{
	ushort		number;		/* controller number */

	u_char		quiesce,	/* Do not issue new commands */
			lip_wait,	/* prim sent; waiting to send lip tcb */
			lip_issued,	/* Lip TCB sent; awaiting completion */
			lipmap_valid,

			primbusy,	/* primitive TCB in use */
			userprim,	/* user primitive active (LPB/LPE) */
			bigmap_busy,	/* bigmap (below) in use */
			off_line,	/* interrupts disabled */

			dead,		/* ctlr unusable */
			intr_state,	/* set in ulm_event, for use in intr */
			LoS,		/* Loss of Signal/Sync */
			scsi_cmds,	/* scsi commands have been done */

			intr,		/* in interrupt routine */
			error_id;	/* ID of target that had timeout */

	uint8_t			 host_id;	/* FC-AL addr of host adapter */
	uint8_t			 lip_attempt;	/* number of attempts to lip */
	uint8_t			 mia_type;
	ushort			 lipcount;	/* # proc waiting for lip */

	ushort			 cmdcount;	/* # active commands */
	ushort			 hiwater;	/* # active commands */
	ushort			 maxqueue;	/* max # active commands */
	ushort			 tcbindex;	/* next regular TCB */

	toid_t			 lip_timeout;
	toid_t			 quiesce_timeout; /* timeout ID */
	uint32_t		 quiesce_time;	/* length of time to quiesce */
	uint			 timeout_interval;
	toid_t			 timeout_id;
	int			 userprim_status;

	struct target_queue	 scsi_queue;
	struct target_queue	 lnksvc_queue;
	struct target_queue	 abts_queue;

	struct fc_tcbhdr	*freetcb;	/* list of available tcb's */
	struct scsi_request	*req_complete_chain;

	/*
	 * Persistant values of the data structure that do not change
	 * across board/chip/bus/controller resets.
	 * These fields that are allocated/initialized at attach time.
	 */
	uint64_t		 portname;	/* WorldWide Address */
	u_char			*ibar0_addr;	/* base address of PCI chip */
	u_char			*ibar1_addr;	/* base address of PCI chip */
	u_char			*config_addr;	/* address of chip config space */
	u_char			*status_addr;	/* address of int status block */

	struct _tcb		*tcbqueue;	/* circular queue of TCB's for
						 * sh_deliver_tcb */
	struct fc_tcbhdr	**tcbptrs;	/* pointer to pointers to
						 * tcbhdrs, indexed by tcbnum */
	struct fc_tcbhdr	*prim_tcb;	/* TCB reserved for primitives */
	struct fc_tcbhdr	*lip_tcb;	/* TCB reserved for LIP */

	struct shim_config	*config;	/* slim him config structure */
	void			*control_blk;	/* HIM control block */
	void			*doneq;		/* done Q for sequencer */
	SH_SG_DESCRIPTOR	*bigmap;	/* maxdmasz s/g list */
	uint8_t			*lipmap;	/* FC-AL loop position map */

	struct fcadptargetinfo	*target[FCADP_MAXTARG];
	vertex_hdl_t		 pcivhdl;
	vertex_hdl_t		 ctlrvhdl;
#if FCADP_SPINLOCKS
	lock_t			 himlock;
	int			 himlockspl;
#else
	mutex_t			 himlock;
#endif
	mutex_t			 opensema;	/* mutual exclusion for alloc */
	mutex_t			 probesema;
	sema_t			 lipsema;	/* loop init wait sema */
	sema_t			 userprim_sema;

	char			 hwgname[LABEL_LENGTH_MAX];
	char			 lip_buffer[420];
};

typedef struct fcadpctlrinfo fcadpctlrinfo_t;
#pragma set field attribute fcadpctlrinfo_t himlock align=128

/* values for fcadpctlrinfo.quiesce */
#define LIP_QUIESCE	1
#define USR_QUIESCE	2

/* values for fcadpctlrinfo.intr_state */
#define INTR_FATAL 4
#define INTR_RESET_RETRY 3
#define INTR_LOS 2
#define INTR_CLEAR 0

#define INTRCHECK_INTERVAL (HZ/50)
#define TIMEOUTCHECK_INTERVAL (HZ*2)

/*
 * In the tcbptrs array above, we set the low bit of the pointer when a
 * TCB is inactive.
 */
#define TCB_INACTIVE(ci, tcbnum) ci->tcbptrs[tcbnum] = (struct fc_tcbhdr *) \
				 ((uintptr_t) ((ci)->tcbptrs[tcbnum]) | 1)
#define TCB_ACTIVE(ci, tcbnum) ci->tcbptrs[tcbnum] = (struct fc_tcbhdr *) \
				 ((uintptr_t) ((ci)->tcbptrs[tcbnum]) & ~1)
#define TCB_IS_ACTIVE(ci, tcbnum) (!((uintptr_t) (ci)->tcbptrs[tcbnum] & 1))
#define TCB_IS_INACTIVE(ci, tcbnum) ((uintptr_t) (ci)->tcbptrs[tcbnum] & 1)
#define FIND_TCB(ci, tcbnum) (struct fc_tcbhdr *) \
	((uintptr_t) ci->tcbptrs[(tcbnum)] & ~1)
