#ifdef _LANGUAGE_ASSEMBLY
	.set noreorder
#endif

#define PASS_LEVEL	7
#define	FAIL_LEVEL	8

#define NIBFAIL_GENERIC	1
#define NIBFAIL_ECC	2
#define NIBFAIL_BADEXC	3
#define NIBFAIL_ERTOIP	4
#define NIBFAIL_BADSCL	5
#define NIBFAIL_TOUT	6
#define NIBFAIL_PROC	7
#define NIBFAIL_COHER	8
#define NIBFAIL_BADPAGE	9
#define NIBFAIL_TIMER	10
#define NIBFAIL_MEM	11
#define NIBFAIL_BADINT	12
#define NIBFAIL_ENDIAN	13
#define NIBFAIL_BARRIER	14
#define NUM_NIBFAILS	15

#define	PASSCLEANUP	\
	j	pass_cleanup;		\
	nop

#define	FAILCLEANUP(_x)	\
	j	fail_cleanup;		\
	ori	k0, zero, (_x)

#define	ECCCLEANUP	\
	li	t0, EV_ILE;		\
	sd	zero, 0(t0);		\
	lw	t0, P_SR(PRIVATE);	\
	mtc0	t0, C0_SR;		\
	ld	ra, P_RA(PRIVATE);	\
	ld	sp, P_SP(PRIVATE);	\
	li	v0, 2;			\
	j	ra;			\
	nop

#define PASS \
	j	pass_test;		\
	nop

#define FAIL(x) \
255:	la	t3, 255b;		\
	or	k0, zero, x;		\
	j	fail_test;		\
	nop

#define ECCFAIL \
255:	li	t0, EV_SENDINT;		\
	li	t1, FAIL_LEVEL << 8;	\
	ori	t1, 127;		\
	sd	t1, 0(t0);		\
	li	t0, EV_SPNUM;		\
	ld	zero, 0(t0);		\
	li	t0, EV_CIPL0;		\
	li	t1, FAIL_LEVEL;		\
	sd	t1, 0(t0);		\
	lw	t0, P_SR(PRIVATE);	\
	la	a0, 255b;		\
	jal	pul_cc_puthex;		\
	nop;				\
	la	a0, crlf;		\
	jal	pul_cc_puts;		\
	nop;				\
	ECCCLEANUP


#if 1
#define PRINT_HEX(x) \
	sd	a0, P_SAVEAREA+0(PRIVATE);	\
	sd	ra, P_SAVEAREA+72(PRIVATE);	\
	jal	print_hex;			\
	move	a0, x; /* (BD) */		\
	ld	a0, P_SAVEAREA+0(PRIVATE);	\
	ld	ra, P_SAVEAREA+72(PRIVATE);	\
	nop
#else
#define PRINT_HEX(x)	\
	nop
#endif

#define PRINT_HEX_NOCR(x) \
	sd	a0, P_SAVEAREA+0(PRIVATE);	\
	sd	ra, P_SAVEAREA+72(PRIVATE);	\
	jal	print_byte_nocr;			\
	move	a0, x; /* (BD) */		\
	ld	a0, P_SAVEAREA+0(PRIVATE);	\
	ld	ra, P_SAVEAREA+72(PRIVATE);	\
	nop

#if 1
#define STRING_DUMP(x) \
	sd	a0, P_SAVEAREA+0(PRIVATE);	\
	sd	ra, P_SAVEAREA+72(PRIVATE);	\
	jal	string_dump;			\
	move	a0, x; /* (BD) */		\
	ld	a0, P_SAVEAREA+0(PRIVATE);	\
	ld	ra, P_SAVEAREA+72(PRIVATE);	\
	nop
#else
#define STRING_DUMP(x)	\
	nop
#endif

#define GETCPUNUM(x) 		\
	li      x, EV_SPNUM;	\
	ld      x, 0(x);	\
	nop

