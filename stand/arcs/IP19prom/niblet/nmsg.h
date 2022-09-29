/* nmsgs.h - 
 *	Indices of messages that nibblet can print with the MSG nsyscall.
 *
 */

#define MSG_BITS	6
#define MIND2OFF(x)	(x << MSG_BITS)
#define MSG_ALIGN	.align 	MSG_BITS

#define	M_PROCNUM	0	/* Process number: */
#define M_ICOUNT	1	/* Instruction count: */
#define M_FAILEDREGS	2	/* Failed process registers: (1-7,30,31,PC) */
#define M_PASSED	3	/* Test passed. */
#define M_NUMPROCS	4	/* Number of processes active: */
#define M_MEMERR	5	/* Memory error (virtual addr, physcial addr, wrote, read): */
#define M_REGDUMP	6	/* Register dump: (1-7,30,31,PC) */
#define M_FAILED	7	/* Process failed: */
#define M_BADVADDR	8	/* Bad virtual address in general handler: */
#define M_PROCINIT	9	/* Processor initializing */
#define M_PROCWAIT	10	/* Processor waiting */
#define M_UNSUPPORT	11	/* Unsupported exception */
#define M_LOCKFAIL	12	/* Locking has failed */
#define M_DOINGPROCS	13	/* Preparing processes 2... */
#define M_PASSDONE	14	/* Finished with pass... */
#define M_MASTER	15	/* Master CPU: */
#define M_SLAVE		16	/* CPU waiting: */
#define M_RUNNING	17	/* CPU running. */
#define M_EXCCOUNT	18	/* Exception count... */
#define M_INTCOUNT	19	/* Interrupt count... */
#define M_NOPROCS	20	/* No processes left to run - twiddling. */
#define M_STARTINIT	21	/* Starting initialization */
#define M_ENDINIT	22	/* Finished with initialization */
#define M_STAT		23	/* Status: */
#define M_LOAD		24	/* Loading: */
#define M_CHECK		25	/* Checking: */
#define M_CONTEXT	26	/* Context switching */
#define M_PASSSTART	27	/* Starting pass */
#define M_BADINT	28	/* Unexpected interrupt: */
#define M_BADMEM	29	/* CURPROC != U_MYADR(CURPROC) */
#define M_CURPROC	30	/* Curproc is at */
#define M_TIMEOUT	31	/* Niblet timed out. */
#define M_PASSINT	32	/* Supertest passed on interrupt */
#define M_FAILINT	33	/* Supertest failed on interrupt */
#define M_STESTPASSED	34	/* Last process passed. */
#define M_ERTOIP	35	/* ERTOIP nonzero... */
