#define SYS_GPID()			li	RSYS, GPID; syscall
#define SYS_VTOP(reg)		li	RSYS, VTOP; daddu	RPARM, reg, r0; syscall
#define SYS_REST()			li	RSYS, REST; syscall
#define SYS_INVALIDATE()	li	RSYS, INVALID; syscall
#define SYS_GNPROC()		li	RSYS, GNPROC; syscall
#define SYS_GCOUNT()		li	RSYS, GCOUNT; syscall
#define SYS_GSHARED()		li	RSYS, GSHARED; syscall
#define PRINT(value) 		li RSYS, PRNTHEX; li RPARM, value; syscall
#define PRINT_REG(reg) 		li RSYS, PRNTHEX; daddu RPARM, reg, r0; syscall
#define SYS_MSG(MSG_CONST)	li	RSYS, MSG; li	RPARM, MSG_CONST; syscall

#ifdef NIBTESTDEBUG
#define DEBUG_PRINT(value) li RSYS, PRNTHEX; li RPARM, value; syscall
#define DEBUG_PRINT_ID(value) DEBUG_PRINT(value)
#define DEBUG_PRINT_REG(reg) li RSYS, PRNTHEX; daddu RPARM, reg, r0; syscall
#define DEBUG_PRINT_PASSNUM(reg) DEBUG_PRINT_REG(reg)
#define DEBUG_SYS_MSG(MSG_CONST) li	RSYS, MSG; li	RPARM, MSG_CONST; syscall
#else
#define DEBUG_PRINT(value)
#define DEBUG_PRINT_REG(reg)
#define DEBUG_PRINT_ID(value)
#define DEBUG_PRINT_PASSNUM(reg)
#define DEBUG_SYS_MSG(MSG_CONST)
#endif

#ifdef NIBTESTDEBUG_BIG
#define DEBUG_BIG_PRINT(value) li RSYS, PRNTHEX; li RPARM, value; syscall
#define DEBUG_BIG_PRINT_REG(reg) li RSYS, PRNTHEX; daddu RPARM, reg, r0; syscall
#else
#define DEBUG_BIG_PRINT(value)
#define DEBUG_BIG_PRINT_REG(reg)
#endif

#define PASS() li RSYS, NPASS; syscall
#define FAIL() li RSYS, NFAIL; syscall


/*******************************************************************
	Assumes:
		SHMEM is a register containing the address of shared memory

	Register use:
		SCR
	
	Action:
		Adds value to contents of SHMEM + offset using ll/sc.  If
		sc is not successful, keeps trying until it is successful
********************************************************************/

#define ATOMIC_ADD(offset, reg_value)   \
254:                                    \
	ll      SCR, offset( SHMEM );       \
	nop;                                \
    add	SCR, SCR, reg_value;            \
	sc	SCR, offset( SHMEM );           \
	nop;                                \
	beq	SCR, r0, 254b;                  \
	nop


/*******************************************************************
	Assumes:
		SHMEM is a register containing the address of shared memory

	Register use:
		SCR
	
	Action:
		Adds immed_value to contents of SHMEM + offset using ll/sc. 
		If sc is not successful, keeps trying until it is successful
		This routine is just like ATOMIC_ADD but 'immed_value' is an 
		immediate instead of a register.
********************************************************************/
#define ATOMIC_ADDI(offset, immed_val)  \
254:                                    \
	ll      SCR, offset( SHMEM );       \
	nop;                                \
    addi	SCR, SCR, immed_val;		\
	sc	SCR, offset( SHMEM );           \
	nop;                                \
	beq	SCR, r0, 254b;                  \
	nop

	
/*******************************************************************
	Assumes:
		SHMEM is a register containing the address of shared memory

	Register use:
		SCR
	
	Action:
		Adds 1 to contents of SHMEM + offset using ll/sc. 
		If sc is not successful, keeps trying until it is successful.
		Then we loop, loading the value in SHMEM + offset until we
		see it is NUM_PROCS.
********************************************************************/
#define BARRIER(offset)                 \
	ATOMIC_ADDI(offset, 1);             \
253:                                    \
	lw	SCR, offset(SHMEM);             \
	nop;                                \
	addi	SCR, SCR, -NUM_PROCS;       \
	bne	r0, SCR, 253b;                  \
	nop

#define BARRIER_CLEAR(offset) \
	sw	r0, offset(SHMEM)


/*******************************************************************
	Assumes:
		SHMEM is a register containing the address of shared memory

	Register use:
		SCR
	
	Action:
		Adds 1 to contents of SHMEM + offset using ll/sc. 
		If sc is not successful, keeps trying until it is successful.
		Return the value that was stored to SHMEM+offset in register
		mynum
********************************************************************/
#define UNIQUE_NUM(offset, mynum, n)   \
254:                                   \
	ll	SCR, offset( SHMEM );          \
	nop;                               \
	addi	SCR, SCR, n;               \
	or	mynum, SCR, SCR;               \
	sc	SCR, offset ( SHMEM );         \
	nop;                               \
	beq	SCR, r0, 254b;                 \
	nop



#define C_LOCK( y )						\
253:									\
	ll      y, CLOCK( SHMEM );			\
	nop;								\
	bne     y, r0, 254f;				\
	or		y, r0, r0;					\
	addi    y, y, 1;					\
	sc      y, CLOCK( SHMEM );			\
254:	nop

/* If we get lock, y = 1, else y = 0 */
#define P_LOCK( y )						\
253:									\
	ll      y, PLOCK( SHMEM );			\
	nop;								\
	bne     y, r0, 254f;				\
	or		y, r0, r0;					\
	addi    y, y, 1;					\
	sc      y, PLOCK( SHMEM );			\
254:	nop

#define C_UNLOCK						\
	sw      r0, CLOCK(SHMEM)

#define P_UNLOCK						\
	sw      r0, PLOCK(SHMEM)

