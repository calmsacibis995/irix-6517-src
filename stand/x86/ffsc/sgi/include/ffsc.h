/*
 * Standard FFSC-specific declarations & constants for VxWorks
 */

#ifndef _FFSC_H_
#define _FFSC_H_

#include <types/vxtypes.h>
#include <netinet/in.h>
#include <semlib.h>

struct console;
struct user;


#define DRAM_SIZE     4194304

/* System-wide constants */
#define MAX_RACKS       23   /* This number should actually be N+1 where
				N is the actual number of racks you're 
				trying to support.  This is because the 
				arrays are 0-based and rackid's are
				1-based */
#define MAX_BAYS	2


/* Maximum command/response lengths */
#define MAX_DISP_CMD_LEN	80		/* Display task commands */
#define MAX_ELSC_CMD_LEN	80		/* ELSC commands */
#define MAX_ELSC_MSG_LEN	160	        /* ELSC Message from the hub */
#define MAX_FFSC_CMD_LEN	240		/* FFSC commands */
#define MAX_CONS_CMD_LEN	240		/* Console task commands */

#define MAX_LISTEN_MSG_LEN	sizeof(int)	/* Listener messages */

#define MAX_DISP_RESP_LEN	        128	/* Display task responses */
#define MAX_ELSC_RESP_LEN	        128	/* ELSC responses */
#define MAX_FFSC_RESP_LEN_SINGLE	128	/* Single FFSC responses */
#define MAX_FFSC_RESP_LEN     (MAX_RACKS*MAX_BAYS*(MAX_FFSC_RESP_LEN_SINGLE+1))
#define MAX_LOCAL_RESP_LEN	        128	/* "Local" responses */

#define MAX_OOBMSG_DATA		65535		/* Max size of OOB msg data */
#define MAX_OOBMSG_OVERHEAD	5		/* Bytes non-data in OOB msg */
#define MAX_OOBMSG_LEN		(MAX_OOBMSG_DATA+MAX_OOBMSG_OVERHEAD)


/* Tuneable values */
#define TUNE_BUF_READ		0	/* Timeout for reads into a buf_t */
#define TUNE_BUF_WRITE		1	/* Timeout for writes from a buf_t */
#define TUNE_CI_ROUTER_MSG	2	/* CI_X_Y = timeout for console read */
#define TUNE_CI_ROUTER_RESP	3	/*          from X for purpose Y     */
#define TUNE_CI_ROUTER_WAIT	4
#define TUNE_CO_PROGRESS	5	/* CO_X = timeout for console writes */
#define TUNE_CO_ROUTER		6	/*        to X			     */
#define TUNE_CW_WAKEUP		7	/* Console task select wake up */
#define TUNE_LO_ROUTER		8	/* Write timeout for listener task */
#define TUNE_RI_CONNECT		9	/* RI_X = timeout for router reads */
#define TUNE_RI_ELSC_MSG	10	/*        for purpose X		   */
#define TUNE_RI_ELSC_RESP	11
#define TUNE_RI_LISTENER	12
#define TUNE_RI_REQ		13
#define TUNE_RI_RESP		14
#define TUNE_RO_CONS		15	/* RO_X = timeout for router writes */
#define TUNE_RO_ELSC		16	/*	  for purpose X		    */
#define TUNE_RO_REMOTE		17
#define TUNE_RO_RESP		18
#define TUNE_RW_RESP_LOCAL	19	/* RW_RESP_X = select timeout on */
#define TUNE_RW_RESP_REMOTE	20	/*             response		 */
#define TUNE_CONNECT_TIMEOUT	21
#define TUNE_LISTEN_PORT	22	/* Listen port for remote requests */
#define TUNE_LISTEN_MAX_CONN	23	/* Size of listen queue */
#define TUNE_PROGRESS_INTERVAL	24	/* Bytes per progress dot */
#define TUNE_XFER_MAX_ERROR	25	/* Max # of xmodem errors */
#define TUNE_XFER_MAX_TMOUT	26	/* Max # of xmodem timeouts */
#define TUNE_NUM_SWITCH_SAMPLES 27	/* # samples when reading switches */
#define TUNE_DEBOUNCE_DELAY	28	/* Debounce delay in usecs */
#define TUNE_CONS_MODE_CHANGE   29	/* Delay for console mode change */
#define TUNE_OOBMSG_WRITE	30	/* Out Of Band message write */
#define TUNE_RI_DISPLAY		31	/* Timeout router writes to display */
#define TUNE_RO_DISPLAY		32	/* Timeout router writes to display */
#define TUNE_LOG_DFLT_NUMLINES	33	/* Default # of lines in a log */
#define TUNE_LOG_DFLT_LINELEN	34	/* Default avg line width in a log */
#define TUNE_PAGE_DFLT_LINES	35	/* Default lines per page */
#define TUNE_PWR_DELAY		36	/* Pause between pwr u's & c's */
#define TUNE_CLEAR_FLASH_LOOPS	37	/* Flash RAM clear delay */
#define TUNE_CLEAR_FLASH_RETRY	38	/* # retries for clearing a block */

#define NUM_TUNE		39	/* # of tuneable vars */
#define TUNE_MAX		48

extern int ffscTune[TUNE_MAX];		/* Array of tuneable values */

typedef struct tuneinfo {
	const char *Name;		/* Name of variable */
	int	   Index;		/* Index into ffscTune, etc. */
	int	   Default;		/* Default value */
	int	   Flags;		/* Other flags */
} tuneinfo_t;

#define TIF_SECRET	0x00000001	/* Don't display with PRINTENV ALL */

extern tuneinfo_t ffscTuneInfo[NUM_TUNE];	/* Info about tuneable vars */



/* FFSC control characters */
#define FFSC_CMD_END		"\r"	/* CR */
#define FFSC_CMD_END_CHAR	'\r'

#define FFSC_ACK_SEPARATOR	","
#define FFSC_ACK_SEPARATOR_CHAR ','
#define FFSC_ACK_END		"\r"	/* CR */
#define FFSC_ACK_END_CHAR	'\r'


/* IRIX sysctlrd characters */
#define OBP_CHAR	'\xa0'		/* Out of Band Prefix character */
#define OBP_STR		"\xa0"		/* OBP_CHAR in string form */


/* I/O Ports (logical mappings to hardware serial devices) */
#define PORT_UNASSIGNED -1	/* No port assignment */
#define PORT_TERMINAL	0	/* User terminal */
#define PORT_ELSC_U	1	/* Upper ELSC (bay 0) */
#define PORT_ELSC_L	2	/* Lower ELSC (bay 1) */
#define PORT_SYSTEM	3	/* Base I/O TTY */
#define PORT_ALTCONS	4	/* "Alternate console" (modem) */
#define PORT_DEBUG	5	/* VxWorks target shell */
#define PORT_DAEMON	6	/* IRIX system controller daemon */
#define NUM_PORTS	7	/* Number of defined ports */

extern int ffscPortFDs[NUM_PORTS];	/* Port->FD mapping */

#define portTerminal	(ffscPortFDs[PORT_TERMINAL])
#define portELSC_U	(ffscPortFDs[PORT_ELSC_U])
#define portELSC_L	(ffscPortFDs[PORT_ELSC_L])
#define portSystem	(ffscPortFDs[PORT_SYSTEM])
#define portAltCons	(ffscPortFDs[PORT_ALTCONS])
#define portDebug	(ffscPortFDs[PORT_DEBUG])
#define portDaemon	(ffscPortFDs[PORT_DAEMON])

/***
 * NOTE: portDebug will be used for our second console device
 * if debug is zero.
 * In the event that debug is non-zero, we will simply use the 
 * alternate-console for debugging messages.
 */

/* Global console descriptors */
#define CONS_TERMINAL	0		/* Primary Terminal (COM1)*/
#define CONS_DAEMON	1		/* IRIX system controller daemon */
#define CONS_ALTERNATE	2		/* Alternate console (COM5) */
#define CONS_REMOTE	3		/* Remote FFSC */
#define CONS_DAEMON2    4               /* IRIX system controller daemon */
#define NUM_CONSOLES	5		/* # of consoles */

extern struct console *consoles[NUM_CONSOLES]; /* All known consoles */
extern struct console *getSystemConsole();     /* Returns syscon -console.c*/
extern int getSystemConsoleIndex();            /* Returns idx of same */
extern int getSystemPortFD();
#define consTerminal	(consoles[CONS_TERMINAL])
#define consDaemon	(consoles[CONS_DAEMON])
#define consAlternate	(consoles[CONS_ALTERNATE])
#define consRemote	(consoles[CONS_REMOTE])
#define consDaemon2     (consoles[CONS_DAEMON2])

#define SYSIN_UPPER     4/* System console should talk to upper module COM4 */
#define SYSIN_LOWER     6/* System console should talk to lower module COM6 */
extern int comPortState ;   /* Whether we are talking to COM4 or COM6     */

/* Global user descriptors */
#define USER_DAEMON	0	/* IRIX system controller daemon */
#define USER_TERMINAL	1	/* Terminal */
#define USER_ALTERNATE	2	/* Alternate console (modem) */
#define USER_DISPLAY	3	/* Display/switches device */
#define USER_REMOTE	4	/* Current owner of remote console task */
#define USER_ADMIN	5	/* Dummy user for administrative commands */
#define USER_LOCAL_FFSC	6	/* Dummy user for the local FFSC */
#define NUM_LOCAL_USERS 7	/* Number of local users */

#define USER_FFSC	NUM_LOCAL_USERS	/* Base for FFSC entries (1/FFSC) */

#define NUM_USERS	(NUM_LOCAL_USERS + MAX_RACKS)

extern struct user *users[NUM_USERS];	/* Users of this FFSC */

#define userDaemon	(users[USER_DAEMON])
#define userTerminal	(users[USER_TERMINAL])
#define userAlternate	(users[USER_ALTERNATE])
#define userDisplay	(users[USER_DISPLAY])
#define userAdmin	(users[USER_ADMIN])
#define userRemote	(users[USER_REMOTE])


/* Global pipe descriptors */
extern int C2RReqFDs[NUM_CONSOLES];	/* Console->Router requests */
extern int C2RAckFDs[NUM_CONSOLES];	/* Console->Router responses */
extern int R2CReqFDs[NUM_CONSOLES];	/* Router->Console requests */
extern int D2RReqFD;			/* Display->Router messages */
extern int DispReqFD;			/* ALL->Display requests */
extern int L2RMsgFD;			/* Listen->Router messages */

/* Debugging flags & messages */
typedef struct debuginfo {
	uint32_t	Flags;		/* Flags */
	uint16_t	Console;	/* Debugging console ID */
	uint16_t	MsgMon;		/* Monitor all input on this FD */
	uint32_t	reserved2[6];	/* reserved for future expansion */
} debuginfo_t;


#define FDBF_ADDRD		0x00000001	/* Address daemon messages */
#define FDBF_INIT		0x00000002	/* Initialization progress */
#define FDBF_CONSOLE		0x00000004	/* Console actions */
#define FDBF_BUFFER		0x00000008	/* Buffer actions */
#define FDBF_ELSC		0x00000010	/* ELSC communication */
#define FDBF_ROUTER		0x00000020	/* Router communication */
#define FDBF_PARSEDEST          0x00000040	/* Destination parsing */
#define FDBF_REMOTE		0x00000080	/* Remote communication */
#define FDBF_IDENT		0x00000100	/* Identity services */
#define FDBF_SWITCH		0x00000200	/* Switch handling */
#define FDBF_XMODEM		0x00000400	/* XMODEM transfer stuff */
#define FDBF_BASEIO		0x00000800	/* Base I/O communication */
#define FDBF_TMPDEBUG	        0x00001000	/* Temp. Debug Statements */
#define FDBF_PTASKID		0x00002000	/* Prefix ffscMsg output  with task name */
#define FDBF_PARTINFO		0x00004000	/* Partition information */

#define FDBF_ADCLIENTS	0x80000000	/* Allow address daemon clients */
#define FDBF_ITMANIP	0x40000000	/* Allow identity table manipulation */
#define FDBF_NOOOBMSG	0x20000000	/* Disable OOB message handling */
#define FDBF_NOOBPDBL	0x10000000	/* Don't double user OBP chars */
#define FDBF_SHOWBADCR	0x08000000	/* Don't discard extra CRs & LFs */
#define FDBF_ENABLEALL	0xff000000	/* Enable all dangerous things */

#define FDBC_NONE	0		/* No debugging messages */
#define FDBC_COM1	1		/* Send debug msgs to COM1-COM6 */
#define FDBC_COM2	2
#define FDBC_COM3	3
#define FDBC_COM4	4
#define FDBC_COM5	5
#define FDBC_COM6	6
#define FDBC_DISPLAY	10		/* Send debug msgs to display */
#define FDBC_STDOUT	11		/* Send debug msgs to stdout */
#define FDBC_STDERR	12		/* Send debug msgs to stderr */

#define FDBC_ISCOM(X)	((X) >= FDBC_COM1  &&  (X) <= FDBC_COM6)

extern debuginfo_t	ffscDebug;	/* Debugging information */

int  ffscDebugWrite(char *, int);	/* Write to debug port directly */
void ffscMsg(const char *, ...);	/* Print an FFSC message */
void ffscMsgN(const char *, ...);	/* Print an FFSC message (no \n) */
void ffscMsgS(const char *, ...);	/* Print an FFSC message + strerror */
int  ffscSetDebugConsole(uint16_t);	/* Set the current debugging console */


/* Identity information */
typedef unsigned char uniqueid_t[8];	/* A world-unique identifier */
#define UNIQUEID_STRLEN   24		/* Length of human-readable uniqueid */


typedef uint32_t bayid_t;		/* A bay ID value */
#define BAYID_UNASSIGNED  0xffffffff
#define BAYID_MAX	  1
#define BAYID_BITMAP_ALL  0x00000003
#define BAYID_STRLEN	  11

extern char ffscBayName[MAX_BAYS];	/* Single-char name for each bay */

typedef uint32_t rackid_t;		/* A rack ID value */
#define RACKID_UNASSIGNED 0xffffffff
#define RACKID_BITMAP_ALL 0x000000ff
#define RACKID_MAX	  MAX_RACKS-1     /* This is so because racks are
					     numbered 1-X  (no 0)       */
#define RACKID_STRLEN	  11

typedef uint32_t modulenum_t;		/* A module number */
#define MODULENUM_UNASSIGNED 0xffffffff
#define MODULENUM_MAX	     0x000000ff
#define MODULENUM_STRLEN     11



/* Identifying information about an individual FFSC */
typedef struct identity {
        uniqueid_t      uniqueid;		/* FFSC identifier */
        struct in_addr  ipaddr;			/* IP address */
        rackid_t        rackid;			/* Rack ID */
	unsigned int	capabilities;		/* Capability flags */
	modulenum_t	modulenums[MAX_BAYS];	/* Module #'s for each bay */
} identity_t;

#define CAP_USERINFO	0x00000001	/* Able to propagate user info */


extern identity_t ffscLocal;		/* Identifying info for this FFSC */
extern identity_t *ffscMaster;		/* Identity of the master FFSC */
extern int	  ffscIsMaster;		/* non-zero: this is the master FFSC*/


/* Timestamps */
typedef struct timespec timestamp_t;	/* A timestamp */
#define TIMESTAMP_STRLEN  24

extern timestamp_t ffscStartTime;	/* Timestamp at FFSC initialization */


/*
 * Partition information structure, contains information on all known partitions
 * on the MMSC network.
 */
typedef struct part_info_t{
  int partId;
  int modules[255]; /* Up to 255 modules/partition */
  int moduleCount;
  int consoleModule;
  char *name;
  struct part_info_t* next;
} part_info_t;


/*
 * Partition information structure, contains information on
 * a single module's partition mapping.
 */
typedef struct mod_part_info_t{
  int partid;
  int modid;
  int cons;
} mod_part_info_t;
  

extern SEM_ID part_mutex;		
extern part_info_t* partInfo;

/*
 * Routines for maintaining partitioning information: partition.c 
 */
void part_init();
void print_part(part_info_t** info);
int update_part_info(part_info_t** info, int partid, int modid, int console);
int alloc_part_info(part_info_t** info);
int update_partition_info(part_info_t** info, char* str);
void print_all_part(part_info_t** info);
void dealloc_part_info(part_info_t** info);
char* get_part_info(part_info_t** info);
char* get_part_info_on_partid(part_info_t** info, int partid);
int modnum_to_partnum(part_info_t** info, int modNum);
int console_module(part_info_t** info, int partid);
void lock_part_info();
void unlock_part_info(); 
int contains_module(part_info_t** info, int partid, int modid);
int num_parts(part_info_t* info);
void remove_partition_id(part_info_t** info, int partid);
/*
 * Defines and constants for OOB message processing and state-information.
 */
#define    GFX_OOB_LOWER      0x01000000
#define    GFX_OOB_UPPER      0x10000000
#define    GFX_OOB_BOTH       0x11000000 /* Both upper and lower */

extern unsigned int oob_handler_status;
int inOOBMessageProcessing();




#define ROUTER_PRI 100
#endif  /* _FFSC_H_ */





