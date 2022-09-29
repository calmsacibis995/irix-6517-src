/* vxmCmdLib.h - vxmon command library header file */

/*
modification history
---------------------
01h,12jan94,tpr added Am29k cpu family support.
01h,26aug93,hdn added macros for I80X86.
01g,01aug93,jcf added EVENT_BREAK_QUERY.
01f,08jun93,rfs added support for SPARC.
01e,25mar93,del added EVENT_EXIT_ERROR to event types.
01d,16nov92,del added flt. pt. stuff for I960KB.
01c,17oct92,del minor tweaks.
01b,25jun92,del added floating point stuff.
01a,28oct91,del written.
*/

#ifndef INCvxmCmdLibh
#define INCvxmCmdLibh

#ifndef	_ASMLANGUAGE

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

#define REM_CMD_SIZE		0x04	/* size of command w/o data */

typedef struct 
    {
    unsigned char rcCmd;		/* remote command */
    unsigned char rcHostId;		/* id of host process */
    unsigned char rcTgtId;		/* id of target process */
    unsigned char rcDummy;		/* for alignment */
    unsigned char rcData[0x08];		/* data */
    } REM_CMD;


enum REMOTE_COMMANDS 
    {
    REM_OK =		0x00,	/* remote OK */
    REM_ERROR =		0xff,	/* remote ERROR  */
    REM_CMD_UNK =	0x01,	/* command unknown */
    REM_CONNECT_EXTERNAL,	/* connected to external agent */
    REM_PING,			/* generic debug use */
    REM_CONNECT,		/* remote connect */
    REM_SET_DBG_MODE,		/* remote set debug mode (unused) */

    REM_KILL,			/* kill a remote process */
    REM_CALL_FUNC,		/* remote call function */
    REM_SETUP_FUNC,		/* setup args and stack for func call */
    REM_STEP,			/* remote step */
    REM_CONT,			/* remote continue */

    REM_RESUME,			/* resume code that "paused" */
    REM_EXC_INFO_GET,		/* get exception info */
    REM_BP_SET,			/* remote set breakpoint */
    REM_BP_CLEAR,		/* remote clear breakpoint */
    REM_MEM_READ,		/* remote read memory */

    REM_MEM_WRITE,		/* remote write memory */
    REM_MEM_CLR,		/* remote clear memory */
    REM_GREGS_GET,		/* get general register(s) */
    REM_GREGS_SET_ALL,		/* get general register(s) */
    REM_GREGS_SET,		/* set general register(s) */

    REM_FPREGS_GET,		/* get flt. pt. register(s) */
    REM_FPREGS_SET,		/* set flt. pt. register(s) */
    REM_IO_READ,		/* read input from host */
    REM_IO_WRITE,		/* write output to host */
    REM_CALLBACK,		/* perform remote callback */

    REM_STATUS			/* get vxm status for host */
    };


enum VXM_STATES 		/* monitor states */
    {
    VXM_PROG_STEP = 0x01,	/* the UAC is stopped at a single step */
    VXM_PROG_BREAK,		/* the UAC is stopped at a breakpoint */
    VXM_PROG_EXITED,		/* the UAC exited */
    VXM_PROG_RUNNING,		/* the UAC is running */
    VXM_PROG_PAUSED		/* the UAC is paused */
    };

typedef struct 			/* remote connection structure */
    {
    int		rcPID;		/* dummy pid for gdb */
    int		rcMaxPktData;	/* user defined max packet size */
    BOOL	rcTgtDbg;	/* target debug switch */
    BOOL	rcTgtHasFP;	/* floating point regs */
    }REM_CONN;

enum argType 
    {
    T_UNKNOWN 	= 0,
    T_BYTE 	= 1,
    T_WORD 	= 2,
    T_INT 	= 3,
    T_FLOAT 	= 4,
    T_DOUBLE 	= 5
    };
typedef enum argType argType;


typedef struct 
    {
    argType type;
    union 
	{
	char 	avByte;
	short 	avWord;
	int 	avInt;
	/* float 	avFloat;
	double 	avDouble; */
	} argValU;
    } RC_ARG;

#define	REM_MAX_ARGS	0x07	/* maximum function arguments */

typedef struct 			/* remote function call */
    {
    FUNCPTR	rcFuncAddr;	/* address of function */
    int		rcNArgs;	/* number of arguments */
    RC_ARG	rcFuncArgs[REM_MAX_ARGS]; /* function arguments */
    } RC_FUNC_CALL;

typedef struct			/* remote callback structure */
    {
    UINT32	rcToken;	/* token for service */
    UINT32	rcNArgs;	/* number of arguments to pass */
    RC_ARG	rcArgs[REM_MAX_ARGS]; /* function arguments */
    } RC_CALLBACK;

typedef struct 			/* remote step/continue */
    {
    int 	rcTid;		/* task id to step  or continue */
    addr_t	rcStartAddr;	/* address to start from */
    addr_t	rcRangeStart;	/* low address of step range */
    addr_t	rcRangeEnd;	/* High address of step range */
    } RC_SC;

typedef struct			/* remote breakpoint */
    {
    addr_t	rcAddr;		/* address to set */
    int		rcPassCnt;	/* pass count */
    int		rcTid;		/* id of task to set breakpoint for */
    int	        rcBpType;	/* breakpoint type */
    } RC_BPT;

typedef struct			/* remote memory read/write */
    {
    addr_t	rcAddr;		/* address to read */
    int		rcLen;		/* number of bytes to read */
    } RC_MRW;

#define REG_REQ_ALL		-1	/* request all registers */

#if CPU_FAMILY==MC680X0
#define NUM_GREGS		0x12	/* a0-a7, d0-d7, sr, pc */	
#define GP_REG_BYTES		(NUM_GREGS * sizeof (unsigned long))
#define	FP_NUM_DREGS		8	/* from fpp68kLib.h */
#endif /* CPU_FAMILY==MC680X0 */

#if CPU_FAMILY==I960
#define NUM_GREGS		0x23	/* r0-r15, g0-g15, acw, tcw, pcw */
#define GP_REG_BYTES		(NUM_GREGS * sizeof (unsigned long))
#define	FP_NUM_DREGS		4	/* from fpp960Lib.h */
#endif /* CPU_FAMILY==I960 */

#if CPU_FAMILY==SPARC
#define NUM_GREGS	38	/* r0-r31, y, psr, wim, tbr, pc, npc */
#define GP_REG_BYTES (NUM_GREGS * sizeof (unsigned long))
#define	FP_NUM_DREGS 16                /* from fppSparcLib.h */
#endif /* CPU_FAMILY==SPARC */

#if CPU_FAMILY==AM29XXX
#define NUM_GREGS	205	/* pc[012], cps, ch[adc], alu, rsp, rab, rfb  */
				/* lr1, ip[abc], q, gr96-gr111, gr116-gr124 */
#define GP_REG_BYTES	(NUM_GREGS * sizeof (unsigned long))
#define	FP_NUM_DREGS	2
#endif /* CPU_FAMILY==AM29XXX */

#if CPU_FAMILY==I80X86
#define NUM_GREGS               0x0a    /* eax-esp, eflags, pc */
#define GP_REG_BYTES            (NUM_GREGS * sizeof (unsigned long))
#define FP_NUM_DREGS            8       /* from fppI86Lib.h */
#endif /* CPU_FAMILY==I80X86 */


typedef struct				/* remote register request */
    {
    int	rcRegNo;			/* register number */
    } RC_REG_REQ;

/* the following two structures are used when sending a variable number
 * of registers */
typedef struct
    {
    int		rNum;			/* register number */
    UINT32	rValue;			/* register value */
    } ONE_REG;

typedef struct
    {
    int		vrRegCnt;		/* number of regs */
    BOOL 	vrHaveFP;		/* TRUE if Fp regs have changed */
    ONE_REG	vrReg[NUM_GREGS];	/* register number and value */
    } RC_VAR_REGS;

#define VAR_REG_BASE_SIZE	(2 * sizeof (int))

typedef struct
    {
    char	rcGRegs[GP_REG_BYTES];	/* entire register set */
    } RC_GREGS;

/* this typedef taken from fpp[arch]Lib.h so that file would not need to 
 * be included here. This is because this file is included by remote-vxm*
 * in the vxgdb tree
 */

#if CPU_FAMILY==MC680X0
typedef struct
    {
    u_char	f[12];
    } VXM_DOUBLEX;
#endif /* CPU_FAMILY==MC680X0 */

#if CPU_FAMILY==I960
typedef struct
    {
    unsigned long	f[4];
    } VXM_DOUBLEX;
#endif /* CPU_FAMILY==I960 */

#if CPU_FAMILY==SPARC
typedef struct
    {
    unsigned long	f[2];
    } VXM_DOUBLEX;
#endif /* CPU_FAMILY==SPARC */

#if CPU_FAMILY==AM29XXX
typedef struct
    {
    unsigned long	f[2];
    } VXM_DOUBLEX;
#endif /* CPU_FAMILY==AM29XXX */

#if CPU_FAMILY==I80X86
typedef struct
    {
    u_char      f[10];
    } VXM_DOUBLEX;
#endif /* CPU_FAMILY==I80X86 */


/* the following two structures are used for sending a variable number of
 * floating point registers */

typedef struct
    {
    int		rFpRNo;			/* Flt. Pt. register number */
    VXM_DOUBLEX	rFpVal;			/* Flt. Pt. register value */
    } ONE_FP_REG;

#if CPU_FAMILY==MC680X0
typedef struct
    {
    int		vrRegCnt;		/* number of vrFpRegs */
    int		vrFpcr;
    int		vrFpsr;
    int		vrFpiar;
    ONE_FP_REG	vrFpReg[FP_NUM_DREGS];
    } RC_VAR_FPREGS;

/* size of constant portion of RC_VAR_FPREGS (RegCnt, fpcr, fpsr, fpiar) */
#define FPVR_CONST_SIZE 	(0x04 * sizeof (int))
#endif /* CPU_FAMILY==MC680X0 */

#if CPU_FAMILY==I960
typedef struct
    {
    int		vrRegCnt;		/* number of vrFpRegs */
    ONE_FP_REG	vrFpReg[FP_NUM_DREGS];
    } RC_VAR_FPREGS;

/* size of constant portion of RC_VAR_FPREGS (vrRegCnt) */
#define FPVR_CONST_SIZE 	(sizeof (int))
#endif /* CPU_FAMILY==I960 */

#if CPU_FAMILY==SPARC
typedef struct rc_var_fpregs
    {
    int          vrRegCnt;                    /* number of vrFpRegs */
    unsigned int fsr;                         /* status register */
    ONE_FP_REG   vrFpReg[FP_NUM_DREGS];
    } RC_VAR_FPREGS;

/* size of constant portion of RC_VAR_FPREGS (everything except vrFpReg) */
#define FPVR_CONST_SIZE 	(2 * sizeof (int))
#endif /* CPU_FAMILY==SPARC */

#if CPU_FAMILY==AM29XXX
typedef struct rc_var_fpregs
    {
    int          vrRegCnt;                    /* number of vrFpRegs */
    ONE_FP_REG   vrFpReg[FP_NUM_DREGS];
    } RC_VAR_FPREGS;

/* size of constant portion of RC_VAR_FPREGS (everything except vrFpReg) */
#define FPVR_CONST_SIZE 	(sizeof (int))
#endif /* CPU_FAMILY==AM29XXX */

#if CPU_FAMILY==I80X86
typedef struct
    {
    int         vrRegCnt;               /* number of vrFpRegs */
    int         vrFpcr;
    int         vrFpsr;
    int         vrFptag;
    ONE_FP_REG  vrFpReg[FP_NUM_DREGS];
    } RC_VAR_FPREGS;

/* size of constant portion of RC_VAR_FPREGS (RegCnt, fpcr, fpsr, fptag) */
#define FPVR_CONST_SIZE         (0x04 * sizeof (int))
#endif /* CPU_FAMILY==I80X86 */


/* XXX string. check this user defineable ? */
/* maximum number of characters to send/receive per transfer */
#define MAX_IO_LEN		80
typedef struct
    {
    int		rcLen;			/* length of I/O data */
    char	rcIOData[MAX_IO_LEN];	/* I/O data */
    } RC_IO;


typedef struct			/* return state of monitor to host */
    {
    UINT32	vxmState;
    } RC_STATE; 

/* Target to host events */
enum EVENT_TYPE 
    {
    EVENT_BREAK,		/* breakpoint */
    EVENT_STEP, 		/* single step */
    EVENT_EXIT,			/* UAC exited */
    EVENT_SIGNAL,		/* UAC received signal */
    EVENT_OUTPUT,		/* UAC has output */
    EVENT_INPUT,		/* UAC requesting input */
    EVENT_USR_INT,		/* UAC is paused due to user interrupt */
    EVENT_BREAK_ERROR,		/* XXX string. testing */
    EVENT_EXIT_ERROR,		/* UAC exited abnormally*/
    EVENT_BREAK_QUERY		/* UAC requesting ctrl-c support */
    };

typedef enum EVENT_TYPE EVENT_TYPE;

typedef struct			/* remote event structure */
    {
    EVENT_TYPE 	veType;		/* type of event */
    int		veTaskId;	/* XX string. dummy */
    RC_VAR_REGS veRegs;		/* arch specific registers */
    } VXM_EVENT;

#define  EVENT_BASE_SIZE 	(2 * sizeof(int))

/* error codes */
#define RC_BP_SET_ERROR		0x5000
#define RC_REG_GET_ERROR	0x5001
#define RC_REG_SET_ERROR	0x5002
#define RC_UNK_ARG_TYPE		0x5003

/* debugging macros */
extern volatile u_long * pDbg;
extern	BOOL		dbgPutOK;
#define DBG_PUT_INIT	bzero ((char *)0x800000, 0x1000);
#define DBG_PUT(x) 	if (dbgPutOK) {*pDbg = x; ++pDbg;}

#ifndef __STDC__
IMPORT	void 	vxmCmdLoop ();
IMPORT	void	vxmCmdPut ();
IMPORT	void 	vxmHostNotify ();
IMPORT	void	vxmInit ();
IMPORT	int	vxmPause ();
#else /* __STDC__ */
IMPORT	void 	vxmCmdLoop ();
IMPORT	void 	vxmCmdPut (REM_CMD * pCmd, int dataLen);
IMPORT	void 	vxmHostNotify ( int eventType);
IMPORT	void	vxmInit ();
IMPORT	int	vxmPause ();
#endif /* __STDC__ */

#endif /* _ASMLANGUAGE */

#define	ASM_EVENT_USR_INT	-1	/* XXX strig. hack */		

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

#endif /* INCvxmCmdLibh */
