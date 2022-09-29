/* LOCK assumes that CPUNUM is set and that registers t0 and t1
   can be clobbered. */

#define LOCK(x) \
254:	LL	t1, 0( x );				\
	nop;						\
	bne	t1, zero, 254b;    			\
	ADDI	t1, t1, 1;   				\
	SC	t1, 0( x );    				\
	nop;						\
	beq	t1, zero, 254b;   			\
	nop

#define UNLOCK(x) \
	SW	r0, 0(x)


#define ATOMIC_ADDI(a, bb)               	\
254:    LL	t0, 0(a);          		\
	nop;                            	\
	ADDI	t0, t0, bb;    			\
	SC	t0, 0(a);          		\
	nop;                            	\
	beq	t0, r0, 254b;      		\
	nop


/*      x is a register continaing the barrier address.
 *      y is a register containing the number of processors to syhnchronize.
 */
#define BARRIER(x, y)                           \
	ATOMIC_ADDI(x, 1);                      \
253:    LW	t0, 0(x);                  	\
	nop;                                    \
	SUB	t0, t0, y; 		        \
	bne	t0, zero, 253b;            	\
	nop

/*      x is a register continaing the barrier address.
 *      y is a register containing the number of processors to syhnchronize.
 *	t is the number of clock ticks (COUNT reg) to wait.
 *	Register t1 is nonzero only after a timeout
 */	
#define TIMED_BARRIER(x, y, t)			\
	ATOMIC_ADDI(x, 1);                      \
	MTC0	(zero, C0_COUNT);		\
	nop;					\
253:	MFC0	(t1, C0_COUNT);			\
	LI	t0, t;				\
	slt	t0, t0, t1;			\
	bnez	t0, 254f;			\
	nop;					\
	LW	t0, 0(x);                  	\
	nop;                                    \
	SUB	t0, t0, y; 		        \
	bne	t0, zero, 253b;            	\
	nop;					\
	move	t1, zero;			\
254:	nop;

/* Pseudocode for the softlocks (note that they only work for two processes):
	i is our CPUNUM,
	j is the other processor's CPUNUM (in this case, 1 - i)

LOCK:
	flag[i] := true;
	turn := j;
	while (flag[j] and turn= j) do skip;

UNLOCK:
	flag[i] := false;
*/

#ifdef SOFTLOCKS /* Use nasty software synchronization */
/* This code requires t0 - t3 */
#define RUNLOCK \
	LA	t0, flag;			/* flag array address */  \
	LW	t2, P_CPUNUM(PRIVATE)		/* Get CPUNUM */	  \
	ADD	t0, t0, t1;			/* flag[i] */		  \
	LI	t1, 4;							  \
	SW	t1, 0(t0);			/* flag[i] := 4 */	  \
	LA	t3, turn;						  \
	SUB	t1, t1, t2;			/* SCRATCH2 := j */	  \
	LA	t0, flag;			/* flag array address */  \
	ADD	t0, t0, t1;			/* flag[j] */		  \
	SW	t1, 0(t3);			/* turn := j */		  \
	nop;								  \
254:	LW	t1, 0(t3);			/* SCRATCH2 := turn */	  \
	nop;								  \
	beq	t1, t2, 255f;			/* if turn <> i, skip */  \
	LW	t1, 0(t0);			/* SCRATCH2 := flag[j] */ \
	nop;								  \
	beq	t1, r0, 255f;			/* if flag[j]<>0, skip */ \
	nop;								  \
	j	254b;				/* end of while */	  \
255:	nop					/* Acquired lock! */

#define UNRUNLOCK \
	LA	t0, flag;			/* flag array address */  \
	LW	t1, P_CPUNUM(PRIVATE)		/* get CPUNUM */	  \
	ADD	t0, t0, t1;			/* flag[i] */		  \
	SW	r0, 0(t0)			/* flag[i] := 0 */	  \
	nop

#else /* Use load-linked, store-conditional */
#define RUNLOCK \
	LA	t0, runqlock; \
	LOCK(t0)
#define UNRUNLOCK \
	LA	t0, runqlock; \
	UNLOCK(t0)				# Release runqlock	\

#endif /* SOFTLOCKS */


