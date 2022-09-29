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
#define M_ENTER_TLB_REFILL 36
#define M_ENTER_TLBL 37
#define M_ENTER_BADADDR 38
#define M_VADDR_IS 39
#define M_ENHI_IS 40
#define M_ENLO_IS 41
#define M_TLBSET_IS 42
#define M_ENTER_PROBE_BADADDR 43
#define M_BADADDR_FOUND_INVALID_IN_TLB 44
#define M_BADADDR_NOT_FOUND_IN_TLB 45
#define M_BADADDR_NOT_FOUND_IN_PAGETABLE 46
#define M_TLBL_PAGETABLE_ADDRESS_IS_OK 47
#define M_ERET_TO_TASK 48
#define M_TLBL_PAGETABLE_IS_AT 49
#define M_LOAD_FROM_PT_AT 50
#define M_WROTE_TO_TLB 51
#define M_INTERRUPT_NOT_SCHED 52
#define M_CAUSE_IS 53
#define M_ENTER_EVEREST_ZERO 54
#define M_LEAVE_EVEREST_ZERO 55
#define M_INTERRUPT_FOUND_ONE 56
#define M_BAD_INTERRUPT 57
#define M_FAIL_TEST 58
#define M_CAUSE_IS_ZERO 59
#define M_EPC_IS 60
#define M_CAUSE_MASKED_BY_SR_IS 61
#define M_PID_IS 62
#define M_UNEXPECTED_INTERRUPT 63
#define M_SR_IS 64
#define M_DUMP_TLB_ENTRY 65
#define M_BAD_SYSCALL_VALUE 66
#define M_BRANCHED_ON_REG_VALUE 67
#define M_ENTER_GENEXCEPT_NOT_ON_SCHED 68
#define M_RSYS_IS 69
#define M_ENTER_DUMP_TLB 70
#define M_LOAD_FROM_PT_SUCCESSFUL 71
#define M_SCHED 72
#define M_TASK_SWITCH_TO_PID 73
#define M_RETURN_TO_PID 74
#define M_CURPROC_IS 75
#define M_SAVE_T_REG 76
#define M_RESTORE_T_REG 77
#define M_SAVE_REST_REG 78
#define M_RESTORE_REST_REG 79
#define M_COUNTS_IS 80
#define M_SENDING_PASS_INT 81
#define M_NMI 82
#define M_HANDLER_VECTOR_IS 83
#define M_RETURN_TO_POD 84
#define M_REG_IS 85
#define M_RECEIVED_PASS_INT 86
#define M_RECEIVED_FAIL_INT 87
#define M_SENDING_FAIL_INT 88
#define M_TWIDDLING 89
#define M_ERTOIP_IS 90
#define M_PT_INFO 91
#define M_IMPOSSIBLE 92
