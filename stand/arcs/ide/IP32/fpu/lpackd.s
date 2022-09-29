	.verstamp	2 10
	.text	
	.align	2
	.align	0
$$7:
	.word	0 : 1
	.text	
	.align	3
	.align	0
$$13:
	.double	1.0e0:1
	.text	
	.align	2
	.align	0
$$15:
	.ascii	"."
	.space	2
	.text	
	.align	2
	.align	0
$$17:
	.ascii	"LPACKD FAILED x1 = "
	.space	2
	.text	
	.align	2
	.align	0
$$19:
	.ascii	" xn = "
	.space	2
	.text	
	.align	2
	.align	0
$$21:
	.ascii	"RESIDN = "
	.space	2
	.text	
	.align	2
	.align	0
$$22:
	.ascii	"RESID = "
	.space	2
	.text	
	.align	2
	.align	0
$$33:
	.word	1 : 1
	.text	
	.align	2
	.file	2 "lpackd.f"
	.globl	lpackd_
	.loc	2 1
 #   1	      SUBROUTINE LPACKD
	.ent	lpackd_ 2
lpackd_:
	.option	O1
	subu	$sp, 325712
	sw	$31, 28($sp)
	.mask	0x80000000, -325684
	.frame	$sp, 325712, $31
	.loc	2 6
 #   2	      DOUBLE PRECISION AA(200,200),A(201,200),B(200),X(200)
 #   3	      DOUBLE PRECISION TIME(8,6),CRAY,OPS,TOTAL,NORMA,NORMX
 #   4	      DOUBLE PRECISION RESID,RESIDN,EPS,EPSLON
 #   5	      INTEGER IPVT(200)
 #   6	      LDA = 201
	li	$14, 201
	sw	$14, 325708($sp)
	.loc	2 7
 #   7	      LDAA = 200
	li	$15, 200
	sw	$15, 325704($sp)
	.loc	2 9
 #   8	C
 #   9	      N = 100
	li	$24, 100
	sw	$24, 325700($sp)
	.loc	2 10
 #  10	      CRAY = .056
	li.d	$f4, .056
	s.d	$f4, 325688($sp)
	.loc	2 12
 #  11	C
 #  12	         CALL MATGEN(A,LDA,N,B,NORMA)
	addu	$4, $sp, 4088
	addu	$5, $sp, 325708
	addu	$6, $sp, 325700
	addu	$7, $sp, 2488
	addu	$25, $sp, 2480
	sw	$25, 16($sp)
	.livereg	0x0F00FF0E,0x00000FFF
	jal	matgen_
	.loc	2 13
 #  13	         CALL DGEFA(A,LDA,N,IPVT,INFO)
	addu	$4, $sp, 4088
	addu	$5, $sp, 325708
	addu	$6, $sp, 325700
	addu	$7, $sp, 1680
	addu	$8, $sp, 1676
	sw	$8, 16($sp)
	.livereg	0x0F00FF0E,0x00000FFF
	jal	dgefa_
	.loc	2 14
 #  14	         CALL DGESL(A,LDA,N,IPVT,B,0)
	addu	$4, $sp, 4088
	addu	$5, $sp, 325708
	addu	$6, $sp, 325700
	addu	$7, $sp, 1680
	addu	$9, $sp, 2488
	sw	$9, 16($sp)
	la	$10, $$7
	sw	$10, 20($sp)
	.livereg	0x0F00FF0E,0x00000FFF
	jal	dgesl_
	.loc	2 18
 #  15	C
 #  16	C     COMPUTE A RESIDUAL TO VERIFY RESULTS.
 #  17	C
 #  18	         DO 10 I = 1,N
	li	$11, 1
	sw	$11, 1672($sp)
	lw	$12, 325700($sp)
	sw	$12, 1668($sp)
	blt	$12, 1, $33
	.loc	2 18
	addu	$13, $12, 1
	sw	$13, 1668($sp)
$32:
	.loc	2 19
 #  19	            X(I) = B(I)
	lw	$14, 1672($sp)
	mul	$15, $14, 8
	addu	$24, $sp, 325712
	addu	$25, $15, $24
	l.d	$f6, -323232($25)
	s.d	$f6, -325656($25)
	.loc	2 20
 #  20	   10    CONTINUE
	lw	$8, 1672($sp)
	addu	$9, $8, 1
	sw	$9, 1672($sp)
	lw	$10, 1668($sp)
	bne	$9, $10, $32
$33:
	.loc	2 21
 #  21	         CALL MATGEN(A,LDA,N,B,NORMA)
	addu	$4, $sp, 4088
	addu	$5, $sp, 325708
	addu	$6, $sp, 325700
	addu	$7, $sp, 2488
	addu	$11, $sp, 2480
	sw	$11, 16($sp)
	.livereg	0x0F00FF0E,0x00000FFF
	jal	matgen_
	.loc	2 22
 #  22	         DO 20 I = 1,N
	li	$12, 1
	sw	$12, 1672($sp)
	lw	$13, 325700($sp)
	sw	$13, 1668($sp)
	blt	$13, 1, $35
	.loc	2 22
	addu	$14, $13, 1
	sw	$14, 1668($sp)
$34:
	.loc	2 23
 #  23	            B(I) = -B(I)
	lw	$15, 1672($sp)
	mul	$24, $15, 8
	addu	$25, $sp, 325712
	addu	$8, $24, $25
	l.d	$f8, -323232($8)
	neg.d	$f10, $f8
	s.d	$f10, -323232($8)
	.loc	2 24
 #  24	   20    CONTINUE
	lw	$9, 1672($sp)
	addu	$10, $9, 1
	sw	$10, 1672($sp)
	lw	$11, 1668($sp)
	bne	$10, $11, $34
$35:
	.loc	2 25
 #  25	         CALL DMXPY(N,B,N,LDA,X,A)
	addu	$12, $sp, 325700
	move	$4, $12
	addu	$5, $sp, 2488
	move	$6, $12
	addu	$7, $sp, 325708
	addu	$13, $sp, 64
	sw	$13, 16($sp)
	addu	$14, $sp, 4088
	sw	$14, 20($sp)
	.livereg	0x0F00FF0E,0x00000FFF
	jal	dmxpy_
	.loc	2 26
 #  26	         RESID = 0.0
	li.d	$f16, 0.0
	s.d	$f16, 56($sp)
	.loc	2 27
 #  27	         NORMX = 0.0
	li.d	$f18, 0.0
	s.d	$f18, 48($sp)
	.loc	2 28
 #  28	         DO 30 I = 1,N
	li	$15, 1
	sw	$15, 1672($sp)
	lw	$24, 325700($sp)
	sw	$24, 1668($sp)
	blt	$24, 1, $37
	.loc	2 28
	addu	$25, $24, 1
	sw	$25, 1668($sp)
$36:
	.loc	2 29
 #  29	            RESID = DMAX1( RESID, DABS(B(I)) )
	l.d	$f4, 56($sp)
	lw	$8, 1672($sp)
	mul	$9, $8, 8
	addu	$10, $sp, 325712
	addu	$11, $9, $10
	l.d	$f6, -323232($11)
	abs.d	$f8, $f6
	c.lt.d	$f4, $f8
	bc1f	$40
	mov.d	$f4, $f8
$40:
	s.d	$f4, 56($sp)
	.loc	2 30
 #  30	            NORMX = DMAX1( NORMX, DABS(X(I)) )
	l.d	$f10, 48($sp)
	l.d	$f16, -325656($11)
	abs.d	$f18, $f16
	c.lt.d	$f10, $f18
	bc1f	$41
	mov.d	$f10, $f18
$41:
	s.d	$f10, 48($sp)
	.loc	2 31
 #  31	   30    CONTINUE
	lw	$12, 1672($sp)
	addu	$13, $12, 1
	sw	$13, 1672($sp)
	lw	$14, 1668($sp)
	bne	$13, $14, $36
$37:
	.loc	2 32
 #  32	         EPS = EPSLON(1.0D0)
	la	$4, $$13
	.livereg	0x0800FF0E,0x00000FFF
	jal	epslon_
	s.d	$f0, 40($sp)
	.loc	2 33
 #  33	         RESIDN = RESID/( N*NORMA*NORMX*EPS )
	lw	$15, 325700($sp)
	mtc1	$15, $f6
	cvt.d.w	$f8, $f6
	l.d	$f4, 2480($sp)
	mul.d	$f16, $f8, $f4
	l.d	$f18, 48($sp)
	mul.d	$f10, $f16, $f18
	l.d	$f6, 40($sp)
	mul.d	$f8, $f10, $f6
	l.d	$f4, 56($sp)
	div.d	$f16, $f4, $f8
	s.d	$f16, 32($sp)
	.loc	2 34
 #  34		 IF ( (1.0E0 - X(1) .LT. 1.0E-13) .AND. (1.0E0 - X(N) .LT. 1.0E-13) .AND. (X(1) .LT. 1.0E0) .AND. (X(N) .LT. 1.0E0)) THEN
	li.d	$f18, 1.0e0
	l.d	$f10, 64($sp)
	sub.d	$f6, $f18, $f10
	li.d	$f4, 1.0e-13
	c.lt.d	$f6, $f4
	bc1f	$38
	li.d	$f8, 1.0e0
	mul	$24, $15, 8
	addu	$25, $sp, 325712
	addu	$8, $24, $25
	l.d	$f16, -325656($8)
	sub.d	$f18, $f8, $f16
	li.d	$f6, 1.0e-13
	c.lt.d	$f18, $f6
	bc1f	$38
	li.d	$f4, 1.0e0
	c.lt.d	$f10, $f4
	bc1f	$38
	l.d	$f8, -325656($8)
	li.d	$f16, 1.0e0
	c.lt.d	$f8, $f16
	bc1f	$38
	.loc	2 35
 #  35			CALL STRPRNT(".")
	la	$4, $$15
	li	$5, 1
	.livereg	0x0C00FF0E,0x00000FFF
	jal	strprnt_
	.loc	2 50
 #  50			RETURN(0)
	move	$2, $0
	b	$39
$38:
	.loc	2 52
 #  51		 ELSE
 #  52			CALL ERR_STRPRNT("LPACKD FAILED x1 = ")
	la	$4, $$17
	li	$5, 19
	.livereg	0x0C00FF0E,0x00000FFF
	jal	err_strprnt_
	.loc	2 53
 #  53			CALL DBLPRNT(X(1))
	addu	$4, $sp, 64
	.livereg	0x0800FF0E,0x00000FFF
	jal	dblprnt_
	.loc	2 54
 #  54			CALL STRPRNT(" xn = ")
	la	$4, $$19
	li	$5, 6
	.livereg	0x0C00FF0E,0x00000FFF
	jal	strprnt_
	.loc	2 55
 #  55			CALL DBLPRNT(X(N))
	lw	$9, 325700($sp)
	mul	$10, $9, 8
	addu	$11, $10, -325656
	addu	$12, $sp, 325712
	addu	$4, $11, $12
	.livereg	0x0800FF0E,0x00000FFF
	jal	dblprnt_
	.loc	2 56
 #  56			CALL NEWLINE
	.livereg	0x0000FF0E,0x00000FFF
	jal	newline_
	.loc	2 57
 #  57			CALL STRPRNT("RESIDN = ")
	la	$4, $$21
	li	$5, 9
	.livereg	0x0C00FF0E,0x00000FFF
	jal	strprnt_
	.loc	2 58
 #  58	         	CALL DBLPRNT(RESIDN)
	addu	$4, $sp, 32
	.livereg	0x0800FF0E,0x00000FFF
	jal	dblprnt_
	.loc	2 59
 #  59			CALL NEWLINE
	.livereg	0x0000FF0E,0x00000FFF
	jal	newline_
	.loc	2 60
 #  60			CALL STRPRNT("RESID = ")
	la	$4, $$22
	li	$5, 8
	.livereg	0x0C00FF0E,0x00000FFF
	jal	strprnt_
	.loc	2 61
 #  61			CALL DBLPRNT(RESID)
	addu	$4, $sp, 56
	.livereg	0x0800FF0E,0x00000FFF
	jal	dblprnt_
	.loc	2 62
 #  62			CALL NEWLINE
	.livereg	0x0000FF0E,0x00000FFF
	jal	newline_
	.loc	2 63
 #  63			RETURN(1)
	li	$2, 1
$39:
	lw	$31, 28($sp)
	addu	$sp, 325712
	j	$31
	.end	lpackd_
	.text	
	.align	2
	.file	2 "lpackd.f"
	.globl	matgen_
	.loc	2 74
 #  74	      SUBROUTINE MATGEN(A,LDA,N,B,NORMA)
	.ent	matgen_ 2
matgen_:
	.option	O1
	subu	$sp, 24
	.frame	$sp, 24, $31
	.loc	2 74
	lw	$14, 0($5)
	sw	$14, 20($sp)
	.loc	2 77
 #  75	      DOUBLE PRECISION A(LDA,1),B(1),NORMA
 #  76	C
 #  77	      INIT = 1325
	li	$15, 1325
	sw	$15, 16($sp)
	.loc	2 78
 #  78	      NORMA = 0.0
	li.d	$f4, 0.0
	lw	$24, 40($sp)
	s.d	$f4, 0($24)
	.loc	2 79
 #  79	      DO 30 J = 1,N
	li	$25, 1
	sw	$25, 12($sp)
	lw	$8, 0($6)
	sw	$8, 8($sp)
	lw	$9, 8($sp)
	blt	$9, 1, $45
	.loc	2 79
	lw	$10, 0($6)
	addu	$11, $10, 1
	sw	$11, 8($sp)
$42:
	.loc	2 80
 #  80	         DO 20 I = 1,N
	li	$12, 1
	sw	$12, 4($sp)
	lw	$13, 0($6)
	sw	$13, 0($sp)
	lw	$14, 0($sp)
	blt	$14, 1, $44
	.loc	2 80
	lw	$15, 0($6)
	addu	$24, $15, 1
	sw	$24, 0($sp)
$43:
	.loc	2 81
 #  81	            INIT = MOD(3125*INIT,65536)
	lw	$25, 16($sp)
	mul	$8, $25, 3125
	rem	$9, $8, 65536
	sw	$9, 16($sp)
	.loc	2 82
 #  82	            A(I,J) = (INIT - 32768.0)/16384.0
	mtc1	$9, $f6
	cvt.s.w	$f8, $f6
	li.s	$f10, 32768.0
	sub.s	$f16, $f8, $f10
	li.s	$f18, 16384.0
	div.s	$f4, $f16, $f18
	cvt.d.s	$f6, $f4
	lw	$10, 12($sp)
	lw	$11, 20($sp)
	mul	$12, $10, $11
	lw	$13, 4($sp)
	addu	$14, $13, $12
	subu	$15, $14, $11
	mul	$24, $15, 8
	addu	$25, $4, $24
	s.d	$f6, -8($25)
	.loc	2 83
 #  83	            NORMA = DMAX1(A(I,J), NORMA)
	lw	$8, 12($sp)
	lw	$9, 20($sp)
	mul	$10, $8, $9
	lw	$13, 4($sp)
	addu	$12, $13, $10
	subu	$14, $12, $9
	mul	$11, $14, 8
	addu	$15, $4, $11
	l.d	$f8, -8($15)
	lw	$24, 40($sp)
	l.d	$f10, 0($24)
	c.lt.d	$f8, $f10
	bc1f	$52
	mov.d	$f8, $f10
$52:
	s.d	$f8, 0($24)
	.loc	2 84
 #  84	   20    CONTINUE
	lw	$25, 4($sp)
	addu	$8, $25, 1
	sw	$8, 4($sp)
	lw	$13, 0($sp)
	bne	$8, $13, $43
$44:
	.loc	2 85
 #  85	   30 CONTINUE
	lw	$10, 12($sp)
	addu	$12, $10, 1
	sw	$12, 12($sp)
	lw	$9, 8($sp)
	bne	$12, $9, $42
$45:
	.loc	2 86
 #  86	      DO 35 I = 1,N
	li	$14, 1
	sw	$14, 4($sp)
	lw	$11, 0($6)
	sw	$11, 0($sp)
	lw	$15, 0($sp)
	blt	$15, 1, $47
	.loc	2 86
	lw	$24, 0($6)
	addu	$25, $24, 1
	sw	$25, 0($sp)
$46:
	.loc	2 87
 #  87	          B(I) = 0.0
	li.d	$f16, 0.0
	lw	$8, 4($sp)
	mul	$13, $8, 8
	addu	$10, $7, $13
	s.d	$f16, -8($10)
	.loc	2 88
 #  88	   35 CONTINUE
	lw	$12, 4($sp)
	addu	$9, $12, 1
	sw	$9, 4($sp)
	lw	$14, 0($sp)
	bne	$9, $14, $46
$47:
	.loc	2 89
 #  89	      DO 50 J = 1,N
	li	$11, 1
	sw	$11, 12($sp)
	lw	$15, 0($6)
	sw	$15, 8($sp)
	lw	$24, 8($sp)
	blt	$24, 1, $51
	.loc	2 89
	lw	$25, 0($6)
	addu	$8, $25, 1
	sw	$8, 8($sp)
$48:
	.loc	2 90
 #  90	         DO 40 I = 1,N
	li	$13, 1
	sw	$13, 4($sp)
	lw	$10, 0($6)
	sw	$10, 0($sp)
	lw	$12, 0($sp)
	blt	$12, 1, $50
	.loc	2 90
	lw	$9, 0($6)
	addu	$14, $9, 1
	sw	$14, 0($sp)
$49:
	.loc	2 91
 #  91	            B(I) = B(I) + A(I,J)
	lw	$11, 12($sp)
	lw	$15, 20($sp)
	mul	$24, $11, $15
	lw	$25, 4($sp)
	addu	$8, $25, $24
	subu	$13, $8, $15
	mul	$10, $13, 8
	addu	$12, $4, $10
	l.d	$f18, -8($12)
	mul	$9, $25, 8
	addu	$14, $7, $9
	l.d	$f4, -8($14)
	add.d	$f6, $f4, $f18
	addu	$11, $7, $9
	s.d	$f6, -8($11)
	.loc	2 92
 #  92	   40    CONTINUE
	lw	$24, 4($sp)
	addu	$8, $24, 1
	sw	$8, 4($sp)
	lw	$15, 0($sp)
	bne	$8, $15, $49
$50:
	.loc	2 93
 #  93	   50 CONTINUE
	lw	$13, 12($sp)
	addu	$10, $13, 1
	sw	$10, 12($sp)
	lw	$12, 8($sp)
	bne	$10, $12, $48
	b	$51
$51:
	addu	$sp, 24
	j	$31
	.end	matgen_
	.text	
	.align	2
	.file	2 "lpackd.f"
	.globl	dgefa_
	.loc	2 96
 #  94	      RETURN
 #  95	      END
 #  96	      SUBROUTINE DGEFA(A,LDA,N,IPVT,INFO)
	.ent	dgefa_ 2
dgefa_:
	.option	O1
	subu	$sp, 88
	sw	$31, 36($sp)
	sd	$4, 88($sp)
	sd	$6, 96($sp)
	sw	$16, 32($sp)
	.mask	0x80010000, -52
	.frame	$sp, 88, $31
	.loc	2 96
	lw	$14, 92($sp)
	lw	$15, 0($14)
	sw	$15, 84($sp)
	.loc	2 151
 # 151	      INFO = 0
	lw	$24, 104($sp)
	sw	$0, 0($24)
	.loc	2 152
 # 152	      NM1 = N - 1
	lw	$25, 96($sp)
	lw	$8, 0($25)
	addu	$9, $8, -1
	sw	$9, 80($sp)
	.loc	2 153
 # 153	      IF (NM1 .LT. 1) GO TO 70
	ble	$9, 0, $59
	.loc	2 154
 # 154	      DO 60 K = 1, NM1
	li	$10, 1
	sw	$10, 76($sp)
	lw	$11, 80($sp)
	sw	$11, 72($sp)
	blt	$11, 1, $59
	.loc	2 154
	addu	$12, $11, 1
	sw	$12, 72($sp)
$53:
	.loc	2 155
 # 155	         KP1 = K + 1
	lw	$13, 76($sp)
	addu	$14, $13, 1
	sw	$14, 68($sp)
	.loc	2 159
 # 156	C
 # 157	C        FIND L = PIVOT INDEX
 # 158	C
 # 159	         L = IDAMAX(N-K+1,A(K,K),1) + K - 1
	lw	$15, 96($sp)
	lw	$24, 0($15)
	subu	$25, $24, $13
	addu	$8, $25, 1
	sw	$8, 60($sp)
	addu	$4, $sp, 60
	lw	$9, 84($sp)
	mul	$10, $13, $9
	addu	$11, $13, $10
	subu	$12, $11, $9
	mul	$14, $12, 8
	lw	$15, 88($sp)
	addu	$5, $15, $14
	addu	$5, $5, -8
	la	$6, $$33
	.livereg	0x0E00FF0E,0x00000FFF
	jal	idamax_
	move	$16, $2
	lw	$24, 76($sp)
	addu	$25, $16, $24
	addu	$8, $25, -1
	sw	$8, 64($sp)
	.loc	2 160
 # 160	         IPVT(K) = L
	lw	$13, 100($sp)
	mul	$10, $24, 4
	addu	$11, $13, $10
	sw	$8, -4($11)
	.loc	2 164
 # 161	C
 # 162	C        ZERO PIVOT IMPLIES THIS COLUMN ALREADY TRIANGULARIZED
 # 163	C
 # 164	         IF (A(L,K) .EQ. 0.0D0) GO TO 40
	lw	$9, 76($sp)
	lw	$12, 84($sp)
	mul	$15, $9, $12
	lw	$14, 64($sp)
	addu	$25, $14, $15
	subu	$24, $25, $12
	mul	$13, $24, 8
	lw	$10, 88($sp)
	addu	$8, $10, $13
	l.d	$f4, -8($8)
	li.d	$f6, 0.0e0
	c.eq.d	$f4, $f6
	bc1t	$57
	.loc	2 168
 # 165	C
 # 166	C           INTERCHANGE IF NECESSARY
 # 167	C
 # 168	            IF (L .EQ. K) GO TO 10
	lw	$11, 64($sp)
	lw	$9, 76($sp)
	beq	$11, $9, $54
	.loc	2 169
 # 169	               T = A(L,K)
	lw	$14, 76($sp)
	lw	$15, 84($sp)
	mul	$25, $14, $15
	lw	$12, 64($sp)
	addu	$24, $12, $25
	subu	$10, $24, $15
	mul	$13, $10, 8
	lw	$8, 88($sp)
	addu	$11, $8, $13
	l.d	$f8, -8($11)
	s.d	$f8, 48($sp)
	.loc	2 170
 # 170	               A(L,K) = A(K,K)
	addu	$9, $14, $25
	subu	$12, $9, $15
	mul	$24, $12, 8
	addu	$10, $8, $24
	l.d	$f10, -8($10)
	s.d	$f10, -8($11)
	.loc	2 171
 # 171	               A(K,K) = T
	l.d	$f16, 48($sp)
	lw	$13, 76($sp)
	lw	$14, 84($sp)
	mul	$25, $13, $14
	addu	$9, $13, $25
	subu	$15, $9, $14
	mul	$12, $15, 8
	lw	$8, 88($sp)
	addu	$24, $8, $12
	s.d	$f16, -8($24)
$54:
	.loc	2 176
 # 172	   10       CONTINUE
 # 173	C
 # 174	C           COMPUTE MULTIPLIERS
 # 175	C
 # 176	            T = -1.0D0/A(K,K)
	lw	$10, 76($sp)
	lw	$11, 84($sp)
	mul	$13, $10, $11
	addu	$25, $10, $13
	subu	$9, $25, $11
	mul	$14, $9, 8
	lw	$15, 88($sp)
	addu	$8, $15, $14
	l.d	$f18, -8($8)
	li.d	$f4, -1.0e0
	div.d	$f6, $f4, $f18
	s.d	$f6, 48($sp)
	.loc	2 177
 # 177	            CALL DSCAL(N-K,T,A(K+1,K),1)
	lw	$12, 96($sp)
	lw	$24, 0($12)
	subu	$13, $24, $10
	sw	$13, 60($sp)
	addu	$4, $sp, 60
	addu	$5, $sp, 48
	move	$6, $8
	la	$7, $$33
	.livereg	0x0F00FF0E,0x00000FFF
	jal	dscal_
	.loc	2 181
 # 178	C
 # 179	C           ROW ELIMINATION WITH COLUMN INDEXING
 # 180	C
 # 181	            DO 30 J = KP1, N
	lw	$25, 68($sp)
	sw	$25, 44($sp)
	lw	$11, 96($sp)
	lw	$9, 0($11)
	sw	$9, 60($sp)
	bgt	$25, $9, $58
	.loc	2 181
	lw	$15, 0($11)
	addu	$14, $15, 1
	sw	$14, 60($sp)
$55:
	.loc	2 182
 # 182	               T = A(L,J)
	lw	$12, 44($sp)
	lw	$24, 84($sp)
	mul	$10, $12, $24
	lw	$13, 64($sp)
	addu	$8, $13, $10
	subu	$25, $8, $24
	mul	$9, $25, 8
	lw	$11, 88($sp)
	addu	$15, $11, $9
	l.d	$f8, -8($15)
	s.d	$f8, 48($sp)
	.loc	2 183
 # 183	               IF (L .EQ. K) GO TO 20
	lw	$14, 76($sp)
	beq	$13, $14, $56
	.loc	2 184
 # 184	                  A(L,J) = A(K,J)
	lw	$12, 44($sp)
	lw	$10, 84($sp)
	mul	$8, $12, $10
	lw	$24, 76($sp)
	addu	$25, $24, $8
	subu	$11, $25, $10
	mul	$9, $11, 8
	lw	$15, 88($sp)
	addu	$13, $15, $9
	l.d	$f10, -8($13)
	lw	$14, 64($sp)
	addu	$12, $14, $8
	subu	$24, $12, $10
	mul	$25, $24, 8
	addu	$11, $15, $25
	s.d	$f10, -8($11)
	.loc	2 185
 # 185	                  A(K,J) = T
	l.d	$f16, 48($sp)
	lw	$9, 44($sp)
	lw	$13, 84($sp)
	mul	$14, $9, $13
	lw	$8, 76($sp)
	addu	$12, $8, $14
	subu	$10, $12, $13
	mul	$24, $10, 8
	lw	$15, 88($sp)
	addu	$25, $15, $24
	s.d	$f16, -8($25)
$56:
	.loc	2 187
 # 186	   20          CONTINUE
 # 187	               CALL DAXPY(N-K,T,A(K+1,K),1,A(K+1,J),1)
	lw	$11, 96($sp)
	lw	$9, 0($11)
	lw	$8, 76($sp)
	subu	$14, $9, $8
	sw	$14, 40($sp)
	addu	$4, $sp, 40
	addu	$5, $sp, 48
	lw	$12, 84($sp)
	mul	$13, $8, $12
	addu	$10, $8, $13
	subu	$15, $10, $12
	mul	$24, $15, 8
	lw	$25, 88($sp)
	addu	$6, $25, $24
	la	$11, $$33
	move	$7, $11
	lw	$9, 44($sp)
	mul	$14, $9, $12
	addu	$13, $8, $14
	subu	$10, $13, $12
	mul	$15, $10, 8
	addu	$24, $25, $15
	sw	$24, 16($sp)
	sw	$11, 20($sp)
	.livereg	0x0F00FF0E,0x00000FFF
	jal	daxpy_
	.loc	2 188
 # 188	   30       CONTINUE
	lw	$9, 44($sp)
	addu	$8, $9, 1
	sw	$8, 44($sp)
	lw	$14, 60($sp)
	bne	$8, $14, $55
	b	$58
$57:
	.loc	2 191
 # 189	         GO TO 50
 # 190	   40    CONTINUE
 # 191	            INFO = K
	lw	$13, 76($sp)
	lw	$12, 104($sp)
	sw	$13, 0($12)
$58:
	.loc	2 193
 # 192	   50    CONTINUE
 # 193	   60 CONTINUE
	lw	$10, 76($sp)
	addu	$25, $10, 1
	sw	$25, 76($sp)
	lw	$15, 72($sp)
	bne	$25, $15, $53
$59:
	.loc	2 195
 # 194	   70 CONTINUE
 # 195	      IPVT(N) = N
	lw	$24, 96($sp)
	lw	$11, 0($24)
	lw	$9, 100($sp)
	mul	$8, $11, 4
	addu	$14, $9, $8
	sw	$11, -4($14)
	.loc	2 196
 # 196	      IF (A(N,N) .EQ. 0.0D0) INFO = N
	lw	$13, 96($sp)
	lw	$12, 0($13)
	lw	$10, 84($sp)
	mul	$25, $12, $10
	addu	$15, $12, $25
	subu	$24, $15, $10
	mul	$9, $24, 8
	lw	$8, 88($sp)
	addu	$11, $8, $9
	l.d	$f4, -8($11)
	li.d	$f18, 0.0e0
	c.eq.d	$f4, $f18
	bc1f	$60
	.loc	2 196
	lw	$14, 104($sp)
	sw	$12, 0($14)
	b	$60
$60:
	lw	$16, 32($sp)
	lw	$31, 36($sp)
	addu	$sp, 88
	j	$31
	.end	dgefa_
	.text	
	.align	2
	.file	2 "lpackd.f"
	.globl	dgesl_
	.loc	2 199
 # 197	      RETURN
 # 198	      END
 # 199	      SUBROUTINE DGESL(A,LDA,N,IPVT,B,JOB)
	.ent	dgesl_ 2
dgesl_:
	.option	O1
	subu	$sp, 88
	sw	$31, 36($sp)
	sd	$4, 88($sp)
	sd	$6, 96($sp)
	s.d	$f20, 24($sp)
	.mask	0x80000000, -52
	.fmask	0x00300000, -64
	.frame	$sp, 88, $31
	.loc	2 199
	lw	$14, 92($sp)
	lw	$15, 0($14)
	sw	$15, 84($sp)
	.loc	2 262
 # 262	      NM1 = N - 1
	lw	$24, 96($sp)
	lw	$25, 0($24)
	addu	$8, $25, -1
	sw	$8, 80($sp)
	.loc	2 263
 # 263	      IF (JOB .NE. 0) GO TO 50
	lw	$9, 108($sp)
	lw	$10, 0($9)
	bne	$10, 0, $65
	.loc	2 268
 # 264	C
 # 265	C        JOB = 0 , SOLVE  A * X = B
 # 266	C        FIRST SOLVE  L*Y = B
 # 267	C
 # 268	         IF (NM1 .LT. 1) GO TO 30
	lw	$11, 80($sp)
	ble	$11, 0, $63
	.loc	2 269
 # 269	         DO 20 K = 1, NM1
	li	$12, 1
	sw	$12, 76($sp)
	lw	$13, 80($sp)
	sw	$13, 72($sp)
	blt	$13, 1, $63
	.loc	2 269
	addu	$14, $13, 1
	sw	$14, 72($sp)
$61:
	.loc	2 270
 # 270	            L = IPVT(K)
	lw	$15, 100($sp)
	lw	$24, 76($sp)
	mul	$25, $24, 4
	addu	$8, $15, $25
	lw	$9, -4($8)
	sw	$9, 68($sp)
	.loc	2 271
 # 271	            T = B(L)
	mul	$10, $9, 8
	lw	$11, 104($sp)
	addu	$12, $11, $10
	l.d	$f4, -8($12)
	s.d	$f4, 56($sp)
	.loc	2 272
 # 272	            IF (L .EQ. K) GO TO 10
	beq	$9, $24, $62
	.loc	2 273
 # 273	               B(L) = B(K)
	lw	$13, 104($sp)
	lw	$14, 76($sp)
	mul	$15, $14, 8
	addu	$25, $13, $15
	l.d	$f6, -8($25)
	lw	$8, 68($sp)
	mul	$11, $8, 8
	addu	$10, $13, $11
	s.d	$f6, -8($10)
	.loc	2 274
 # 274	               B(K) = T
	l.d	$f8, 56($sp)
	lw	$12, 104($sp)
	lw	$9, 76($sp)
	mul	$24, $9, 8
	addu	$14, $12, $24
	s.d	$f8, -8($14)
$62:
	.loc	2 276
 # 275	   10       CONTINUE
 # 276	            CALL DAXPY(N-K,T,A(K+1,K),1,B(K+1),1)
	lw	$15, 96($sp)
	lw	$25, 0($15)
	lw	$8, 76($sp)
	subu	$13, $25, $8
	sw	$13, 52($sp)
	addu	$4, $sp, 52
	addu	$5, $sp, 56
	lw	$11, 84($sp)
	mul	$10, $8, $11
	addu	$9, $8, $10
	subu	$12, $9, $11
	mul	$24, $12, 8
	lw	$14, 88($sp)
	addu	$6, $14, $24
	la	$15, $$33
	move	$7, $15
	lw	$25, 104($sp)
	mul	$13, $8, 8
	addu	$10, $25, $13
	sw	$10, 16($sp)
	sw	$15, 20($sp)
	.livereg	0x0F00FF0E,0x00000FFF
	jal	daxpy_
	.loc	2 277
 # 277	   20    CONTINUE
	lw	$9, 76($sp)
	addu	$11, $9, 1
	sw	$11, 76($sp)
	lw	$12, 72($sp)
	bne	$11, $12, $61
$63:
	.loc	2 282
 # 278	   30    CONTINUE
 # 279	C
 # 280	C        NOW SOLVE  U*X = Y
 # 281	C
 # 282	         DO 40 KB = 1, N
	li	$14, 1
	sw	$14, 48($sp)
	lw	$24, 96($sp)
	lw	$8, 0($24)
	sw	$8, 52($sp)
	blt	$8, 1, $70
	.loc	2 282
	lw	$25, 0($24)
	addu	$13, $25, 1
	sw	$13, 52($sp)
$64:
	.loc	2 283
 # 283	            K = N + 1 - KB
	lw	$10, 96($sp)
	lw	$15, 0($10)
	lw	$9, 48($sp)
	subu	$11, $15, $9
	addu	$12, $11, 1
	sw	$12, 76($sp)
	.loc	2 284
 # 284	            B(K) = B(K)/A(K,K)
	lw	$14, 84($sp)
	mul	$8, $12, $14
	addu	$24, $12, $8
	subu	$25, $24, $14
	mul	$13, $25, 8
	lw	$10, 88($sp)
	addu	$15, $10, $13
	l.d	$f10, -8($15)
	mul	$9, $12, 8
	lw	$11, 104($sp)
	addu	$8, $11, $9
	l.d	$f16, -8($8)
	div.d	$f18, $f16, $f10
	s.d	$f18, -8($8)
	.loc	2 285
 # 285	            T = -B(K)
	lw	$24, 104($sp)
	lw	$14, 76($sp)
	mul	$25, $14, 8
	addu	$10, $24, $25
	l.d	$f4, -8($10)
	neg.d	$f6, $f4
	s.d	$f6, 56($sp)
	.loc	2 286
 # 286	            CALL DAXPY(K-1,T,A(1,K),1,B(1),1)
	addu	$13, $14, -1
	sw	$13, 72($sp)
	addu	$4, $sp, 72
	addu	$5, $sp, 56
	lw	$15, 84($sp)
	mul	$12, $14, $15
	subu	$11, $12, $15
	mul	$9, $11, 8
	lw	$8, 88($sp)
	addu	$6, $8, $9
	la	$7, $$33
	sw	$24, 16($sp)
	la	$25, $$33
	sw	$25, 20($sp)
	.livereg	0x0F00FF0E,0x00000FFF
	jal	daxpy_
	.loc	2 287
 # 287	   40    CONTINUE
	lw	$10, 48($sp)
	addu	$13, $10, 1
	sw	$13, 48($sp)
	lw	$14, 52($sp)
	bne	$13, $14, $64
	b	$70
$65:
	.loc	2 294
 # 294	         DO 60 K = 1, N
	li	$12, 1
	sw	$12, 76($sp)
	lw	$15, 96($sp)
	lw	$11, 0($15)
	sw	$11, 72($sp)
	blt	$11, 1, $67
	.loc	2 294
	lw	$8, 0($15)
	addu	$9, $8, 1
	sw	$9, 72($sp)
$66:
	.loc	2 295
 # 295	            T = DDOT(K-1,A(1,K),1,B(1),1)
	lw	$24, 76($sp)
	addu	$25, $24, -1
	sw	$25, 52($sp)
	addu	$4, $sp, 52
	lw	$10, 84($sp)
	mul	$13, $24, $10
	subu	$14, $13, $10
	mul	$12, $14, 8
	lw	$11, 88($sp)
	addu	$5, $11, $12
	la	$15, $$33
	move	$6, $15
	lw	$7, 104($sp)
	sw	$15, 16($sp)
	.livereg	0x0F00FF0E,0x00000FFF
	jal	ddot_
	s.d	$f0, 56($sp)
	.loc	2 296
 # 296	            B(K) = (B(K) - T)/A(K,K)
	lw	$8, 104($sp)
	lw	$9, 76($sp)
	mul	$25, $9, 8
	addu	$24, $8, $25
	l.d	$f8, -8($24)
	l.d	$f16, 56($sp)
	sub.d	$f10, $f8, $f16
	lw	$13, 84($sp)
	mul	$10, $9, $13
	addu	$14, $9, $10
	subu	$11, $14, $13
	mul	$12, $11, 8
	lw	$15, 88($sp)
	addu	$8, $15, $12
	l.d	$f18, -8($8)
	div.d	$f4, $f10, $f18
	s.d	$f4, -8($24)
	.loc	2 297
 # 297	   60    CONTINUE
	lw	$25, 76($sp)
	addu	$9, $25, 1
	sw	$9, 76($sp)
	lw	$10, 72($sp)
	bne	$9, $10, $66
$67:
	.loc	2 301
 # 298	C
 # 299	C        NOW SOLVE TRANS(L)*X = Y
 # 300	C
 # 301	         IF (NM1 .LT. 1) GO TO 90
	lw	$14, 80($sp)
	ble	$14, 0, $70
	.loc	2 302
 # 302	         DO 80 KB = 1, NM1
	li	$13, 1
	sw	$13, 48($sp)
	lw	$11, 80($sp)
	sw	$11, 52($sp)
	blt	$11, 1, $70
	.loc	2 302
	addu	$15, $11, 1
	sw	$15, 52($sp)
$68:
	.loc	2 303
 # 303	            K = N - KB
	lw	$12, 96($sp)
	lw	$8, 0($12)
	lw	$24, 48($sp)
	subu	$25, $8, $24
	sw	$25, 76($sp)
	.loc	2 304
 # 304	            B(K) = B(K) + DDOT(N-K,A(K+1,K),1,B(K+1),1)
	lw	$9, 0($12)
	subu	$10, $9, $25
	sw	$10, 72($sp)
	addu	$4, $sp, 72
	lw	$14, 84($sp)
	mul	$13, $25, $14
	addu	$11, $25, $13
	subu	$15, $11, $14
	mul	$8, $15, 8
	lw	$24, 88($sp)
	addu	$5, $24, $8
	la	$6, $$33
	mul	$12, $25, 8
	lw	$9, 104($sp)
	addu	$7, $9, $12
	la	$10, $$33
	sw	$10, 16($sp)
	.livereg	0x0F00FF0E,0x00000FFF
	jal	ddot_
	mov.d	$f20, $f0
	lw	$13, 104($sp)
	lw	$11, 76($sp)
	mul	$14, $11, 8
	addu	$15, $13, $14
	l.d	$f6, -8($15)
	add.d	$f8, $f6, $f20
	s.d	$f8, -8($15)
	.loc	2 305
 # 305	            L = IPVT(K)
	lw	$24, 100($sp)
	lw	$8, 76($sp)
	mul	$25, $8, 4
	addu	$9, $24, $25
	lw	$12, -4($9)
	sw	$12, 68($sp)
	.loc	2 306
 # 306	            IF (L .EQ. K) GO TO 70
	beq	$12, $8, $69
	.loc	2 307
 # 307	               T = B(L)
	lw	$10, 104($sp)
	lw	$11, 68($sp)
	mul	$13, $11, 8
	addu	$14, $10, $13
	l.d	$f16, -8($14)
	s.d	$f16, 56($sp)
	.loc	2 308
 # 308	               B(L) = B(K)
	lw	$15, 76($sp)
	mul	$24, $15, 8
	addu	$25, $10, $24
	l.d	$f10, -8($25)
	s.d	$f10, -8($14)
	.loc	2 309
 # 309	               B(K) = T
	l.d	$f18, 56($sp)
	lw	$9, 104($sp)
	lw	$12, 76($sp)
	mul	$8, $12, 8
	addu	$11, $9, $8
	s.d	$f18, -8($11)
$69:
	.loc	2 311
 # 310	   70       CONTINUE
 # 311	   80    CONTINUE
	lw	$13, 48($sp)
	addu	$15, $13, 1
	sw	$15, 48($sp)
	lw	$10, 52($sp)
	bne	$15, $10, $68
	b	$70
$70:
	l.d	$f20, 24($sp)
	lw	$31, 36($sp)
	addu	$sp, 88
	j	$31
	.end	dgesl_
	.text	
	.align	2
	.file	2 "lpackd.f"
	.globl	daxpy_
	.loc	2 316
 # 312	   90    CONTINUE
 # 313	  100 CONTINUE
 # 314	      RETURN
 # 315	      END
 # 316	      SUBROUTINE DAXPY(N,DA,DX,INCX,DY,INCY)
	.ent	daxpy_ 2
daxpy_:
	.option	O1
	subu	$sp, 24
	.frame	$sp, 24, $31
	.loc	2 325
 # 325	      IF(N.LE.0)RETURN
	lw	$14, 0($4)
	ble	$14, 0, $80
	.loc	2 326
 # 326	      IF (DA .EQ. 0.0D0) RETURN
	l.d	$f4, 0($5)
	li.d	$f6, 0.0e0
	c.eq.d	$f4, $f6
	bc1t	$80
	.loc	2 327
 # 327	      IF(INCX.EQ.1.AND.INCY.EQ.1)GO TO 20
	lw	$15, 0($7)
	bne	$15, 1, $71
	lw	$24, 44($sp)
	lw	$25, 0($24)
	beq	$25, 1, $75
$71:
	.loc	2 332
 # 328	C
 # 329	C        CODE FOR UNEQUAL INCREMENTS OR EQUAL INCREMENTS
 # 330	C          NOT EQUAL TO 1
 # 331	C
 # 332	      IX = 1
	li	$8, 1
	sw	$8, 20($sp)
	.loc	2 333
 # 333	      IY = 1
	li	$9, 1
	sw	$9, 16($sp)
	.loc	2 334
 # 334	      IF(INCX.LT.0)IX = (-N+1)*INCX + 1
	lw	$10, 0($7)
	bge	$10, 0, $72
	.loc	2 334
	lw	$11, 0($4)
	li	$12, 1
	subu	$13, $12, $11
	lw	$14, 0($7)
	mul	$15, $13, $14
	addu	$24, $15, 1
	sw	$24, 20($sp)
$72:
	.loc	2 335
 # 335	      IF(INCY.LT.0)IY = (-N+1)*INCY + 1
	lw	$25, 44($sp)
	lw	$8, 0($25)
	bge	$8, 0, $73
	.loc	2 335
	lw	$9, 0($4)
	li	$10, 1
	subu	$12, $10, $9
	mul	$11, $12, $8
	addu	$13, $11, 1
	sw	$13, 16($sp)
$73:
	.loc	2 336
 # 336	      DO 10 I = 1,N
	li	$14, 1
	sw	$14, 12($sp)
	lw	$15, 0($4)
	sw	$15, 8($sp)
	lw	$24, 8($sp)
	blt	$24, 1, $80
	.loc	2 336
	lw	$25, 0($4)
	addu	$10, $25, 1
	sw	$10, 8($sp)
$74:
	.loc	2 337
 # 337	        DY(IY) = DY(IY) + DA*DX(IX)
	lw	$9, 40($sp)
	lw	$12, 16($sp)
	mul	$8, $12, 8
	addu	$11, $9, $8
	l.d	$f8, -8($11)
	l.d	$f10, 0($5)
	lw	$13, 20($sp)
	mul	$14, $13, 8
	addu	$15, $6, $14
	l.d	$f16, -8($15)
	mul.d	$f18, $f10, $f16
	add.d	$f4, $f8, $f18
	s.d	$f4, -8($11)
	.loc	2 338
 # 338	        IX = IX + INCX
	lw	$24, 20($sp)
	lw	$25, 0($7)
	addu	$10, $24, $25
	sw	$10, 20($sp)
	.loc	2 339
 # 339	        IY = IY + INCY
	lw	$12, 16($sp)
	lw	$9, 44($sp)
	lw	$8, 0($9)
	addu	$13, $12, $8
	sw	$13, 16($sp)
	.loc	2 340
 # 340	   10 CONTINUE
	lw	$14, 12($sp)
	addu	$15, $14, 1
	sw	$15, 12($sp)
	lw	$11, 8($sp)
	bne	$15, $11, $74
	b	$80
$75:
	.loc	2 348
 # 348	   20 M = MOD(N,4)
	lw	$24, 0($4)
	rem	$25, $24, 4
	sw	$25, 4($sp)
	.loc	2 349
 # 349	      IF( M .EQ. 0 ) GO TO 40
	beq	$25, 0, $78
	.loc	2 350
 # 350	      DO 30 I = 1,M
	li	$10, 1
	sw	$10, 12($sp)
	lw	$9, 4($sp)
	sw	$9, 8($sp)
	blt	$9, 1, $77
	.loc	2 350
	addu	$12, $9, 1
	sw	$12, 8($sp)
$76:
	.loc	2 351
 # 351	        DY(I) = DY(I) + DA*DX(I)
	lw	$8, 40($sp)
	lw	$13, 12($sp)
	mul	$14, $13, 8
	addu	$15, $8, $14
	l.d	$f6, -8($15)
	l.d	$f10, 0($5)
	addu	$11, $6, $14
	l.d	$f16, -8($11)
	mul.d	$f8, $f10, $f16
	add.d	$f18, $f6, $f8
	s.d	$f18, -8($15)
	.loc	2 352
 # 352	   30 CONTINUE
	lw	$24, 12($sp)
	addu	$25, $24, 1
	sw	$25, 12($sp)
	lw	$10, 8($sp)
	bne	$25, $10, $76
$77:
	.loc	2 353
 # 353	      IF( N .LT. 4 ) RETURN
	lw	$9, 0($4)
	blt	$9, 4, $80
$78:
	.loc	2 354
 # 354	   40 MP1 = M + 1
	lw	$12, 4($sp)
	addu	$13, $12, 1
	sw	$13, 0($sp)
	.loc	2 355
 # 355	      DO 50 I = MP1,N,4
	sw	$13, 12($sp)
	lw	$8, 0($4)
	sw	$8, 8($sp)
	lw	$14, 8($sp)
	bgt	$13, $14, $80
$79:
	.loc	2 356
 # 356	        DY(I) = DY(I) + DA*DX(I)
	lw	$11, 40($sp)
	lw	$15, 12($sp)
	mul	$24, $15, 8
	addu	$25, $11, $24
	l.d	$f4, -8($25)
	l.d	$f10, 0($5)
	addu	$10, $6, $24
	l.d	$f16, -8($10)
	mul.d	$f6, $f10, $f16
	add.d	$f8, $f4, $f6
	s.d	$f8, -8($25)
	.loc	2 357
 # 357	        DY(I + 1) = DY(I + 1) + DA*DX(I + 1)
	lw	$9, 40($sp)
	lw	$12, 12($sp)
	mul	$8, $12, 8
	addu	$13, $9, $8
	l.d	$f18, 0($13)
	l.d	$f10, 0($5)
	addu	$14, $6, $8
	l.d	$f16, 0($14)
	mul.d	$f4, $f10, $f16
	add.d	$f6, $f18, $f4
	s.d	$f6, 0($13)
	.loc	2 358
 # 358	        DY(I + 2) = DY(I + 2) + DA*DX(I + 2)
	lw	$15, 40($sp)
	lw	$11, 12($sp)
	mul	$24, $11, 8
	addu	$10, $15, $24
	l.d	$f8, 8($10)
	l.d	$f10, 0($5)
	addu	$25, $6, $24
	l.d	$f16, 8($25)
	mul.d	$f18, $f10, $f16
	add.d	$f4, $f8, $f18
	s.d	$f4, 8($10)
	.loc	2 359
 # 359	        DY(I + 3) = DY(I + 3) + DA*DX(I + 3)
	lw	$12, 40($sp)
	lw	$9, 12($sp)
	mul	$8, $9, 8
	addu	$14, $12, $8
	l.d	$f6, 16($14)
	l.d	$f10, 0($5)
	addu	$13, $6, $8
	l.d	$f16, 16($13)
	mul.d	$f8, $f10, $f16
	add.d	$f18, $f6, $f8
	s.d	$f18, 16($14)
	.loc	2 360
 # 360	   50 CONTINUE
	lw	$11, 12($sp)
	addu	$15, $11, 4
	sw	$15, 12($sp)
	lw	$24, 8($sp)
	ble	$15, $24, $79
	b	$80
$80:
	addu	$sp, 24
	j	$31
	.end	daxpy_
	.text	
	.align	2
	.file	2 "lpackd.f"
	.globl	ddot_
	.loc	2 363
 # 361	      RETURN
 # 362	      END
 # 363	      DOUBLE PRECISION FUNCTION DDOT(N,DX,INCX,DY,INCY)
	.ent	ddot_ 2
ddot_:
	.option	O1
	subu	$sp, 40
	.frame	$sp, 40, $31
	.loc	2 372
 # 372	      DDOT = 0.0D0
	li.d	$f4, 0.0e0
	s.d	$f4, 32($sp)
	.loc	2 373
 # 373	      DTEMP = 0.0D0
	li.d	$f6, 0.0e0
	s.d	$f6, 24($sp)
	.loc	2 374
 # 374	      IF(N.LE.0)RETURN
	lw	$14, 0($4)
	ble	$14, 0, $92
	.loc	2 375
 # 375	      IF(INCX.EQ.1.AND.INCY.EQ.1)GO TO 20
	lw	$15, 0($6)
	bne	$15, 1, $81
	lw	$24, 56($sp)
	lw	$25, 0($24)
	beq	$25, 1, $86
$81:
	.loc	2 380
 # 376	C
 # 377	C        CODE FOR UNEQUAL INCREMENTS OR EQUAL INCREMENTS
 # 378	C          NOT EQUAL TO 1
 # 379	C
 # 380	      IX = 1
	li	$8, 1
	sw	$8, 20($sp)
	.loc	2 381
 # 381	      IY = 1
	li	$9, 1
	sw	$9, 16($sp)
	.loc	2 382
 # 382	      IF(INCX.LT.0)IX = (-N+1)*INCX + 1
	lw	$10, 0($6)
	bge	$10, 0, $82
	.loc	2 382
	lw	$11, 0($4)
	li	$12, 1
	subu	$13, $12, $11
	lw	$14, 0($6)
	mul	$15, $13, $14
	addu	$24, $15, 1
	sw	$24, 20($sp)
$82:
	.loc	2 383
 # 383	      IF(INCY.LT.0)IY = (-N+1)*INCY + 1
	lw	$25, 56($sp)
	lw	$8, 0($25)
	bge	$8, 0, $83
	.loc	2 383
	lw	$9, 0($4)
	li	$10, 1
	subu	$12, $10, $9
	mul	$11, $12, $8
	addu	$13, $11, 1
	sw	$13, 16($sp)
$83:
	.loc	2 384
 # 384	      DO 10 I = 1,N
	li	$14, 1
	sw	$14, 12($sp)
	lw	$15, 0($4)
	sw	$15, 8($sp)
	lw	$24, 8($sp)
	blt	$24, 1, $85
	.loc	2 384
	lw	$25, 0($4)
	addu	$10, $25, 1
	sw	$10, 8($sp)
$84:
	.loc	2 385
 # 385	        DTEMP = DTEMP + DX(IX)*DY(IY)
	lw	$9, 20($sp)
	mul	$12, $9, 8
	addu	$8, $5, $12
	l.d	$f8, -8($8)
	lw	$11, 16($sp)
	mul	$13, $11, 8
	addu	$14, $7, $13
	l.d	$f10, -8($14)
	mul.d	$f16, $f8, $f10
	l.d	$f18, 24($sp)
	add.d	$f4, $f18, $f16
	s.d	$f4, 24($sp)
	.loc	2 386
 # 386	        IX = IX + INCX
	lw	$15, 0($6)
	addu	$24, $9, $15
	sw	$24, 20($sp)
	.loc	2 387
 # 387	        IY = IY + INCY
	lw	$25, 56($sp)
	lw	$10, 0($25)
	addu	$12, $11, $10
	sw	$12, 16($sp)
	.loc	2 388
 # 388	   10 CONTINUE
	lw	$8, 12($sp)
	addu	$13, $8, 1
	sw	$13, 12($sp)
	lw	$14, 8($sp)
	bne	$13, $14, $84
$85:
	.loc	2 389
 # 389	      DDOT = DTEMP
	l.d	$f6, 24($sp)
	s.d	$f6, 32($sp)
	b	$92
$86:
	.loc	2 397
 # 397	   20 M = MOD(N,5)
	lw	$9, 0($4)
	rem	$15, $9, 5
	sw	$15, 4($sp)
	.loc	2 398
 # 398	      IF( M .EQ. 0 ) GO TO 40
	beq	$15, 0, $89
	.loc	2 399
 # 399	      DO 30 I = 1,M
	li	$24, 1
	sw	$24, 12($sp)
	lw	$25, 4($sp)
	sw	$25, 8($sp)
	blt	$25, 1, $88
	.loc	2 399
	addu	$11, $25, 1
	sw	$11, 8($sp)
$87:
	.loc	2 400
 # 400	        DTEMP = DTEMP + DX(I)*DY(I)
	lw	$10, 12($sp)
	mul	$12, $10, 8
	addu	$8, $5, $12
	l.d	$f8, -8($8)
	addu	$13, $7, $12
	l.d	$f10, -8($13)
	mul.d	$f18, $f8, $f10
	l.d	$f16, 24($sp)
	add.d	$f4, $f16, $f18
	s.d	$f4, 24($sp)
	.loc	2 401
 # 401	   30 CONTINUE
	lw	$14, 12($sp)
	addu	$9, $14, 1
	sw	$9, 12($sp)
	lw	$15, 8($sp)
	bne	$9, $15, $87
$88:
	.loc	2 402
 # 402	      IF( N .LT. 5 ) GO TO 60
	lw	$24, 0($4)
	blt	$24, 5, $91
$89:
	.loc	2 403
 # 403	   40 MP1 = M + 1
	lw	$25, 4($sp)
	addu	$11, $25, 1
	sw	$11, 0($sp)
	.loc	2 404
 # 404	      DO 50 I = MP1,N,5
	sw	$11, 12($sp)
	lw	$10, 0($4)
	sw	$10, 8($sp)
	lw	$8, 8($sp)
	bgt	$11, $8, $91
$90:
	.loc	2 405
 # 405	        DTEMP = DTEMP + DX(I)*DY(I) + DX(I + 1)*DY(I + 1) +
	lw	$12, 12($sp)
	mul	$13, $12, 8
	addu	$14, $5, $13
	l.d	$f6, -8($14)
	addu	$9, $7, $13
	l.d	$f8, -8($9)
	mul.d	$f10, $f6, $f8
	l.d	$f16, 24($sp)
	add.d	$f18, $f16, $f10
	addu	$15, $5, $13
	l.d	$f4, 0($15)
	addu	$24, $7, $13
	l.d	$f6, 0($24)
	mul.d	$f8, $f4, $f6
	add.d	$f16, $f18, $f8
	addu	$25, $5, $13
	l.d	$f10, 8($25)
	addu	$10, $7, $13
	l.d	$f4, 8($10)
	mul.d	$f6, $f10, $f4
	add.d	$f18, $f16, $f6
	addu	$11, $5, $13
	l.d	$f8, 16($11)
	addu	$8, $7, $13
	l.d	$f10, 16($8)
	mul.d	$f4, $f8, $f10
	add.d	$f16, $f18, $f4
	addu	$12, $5, $13
	l.d	$f6, 24($12)
	addu	$14, $7, $13
	l.d	$f8, 24($14)
	mul.d	$f10, $f6, $f8
	add.d	$f18, $f16, $f10
	s.d	$f18, 24($sp)
	.loc	2 407
 # 406	     *   DX(I + 2)*DY(I + 2) + DX(I + 3)*DY(I + 3) + DX(I + 4)*DY(I + 4)
 # 407	   50 CONTINUE
	lw	$9, 12($sp)
	addu	$15, $9, 5
	sw	$15, 12($sp)
	lw	$24, 8($sp)
	ble	$15, $24, $90
$91:
	.loc	2 408
 # 408	   60 DDOT = DTEMP
	l.d	$f4, 24($sp)
	s.d	$f4, 32($sp)
$92:
	.loc	2 410
 # 409	      RETURN
 # 410	      END
	l.d	$f0, 32($sp)
	addu	$sp, 40
	j	$31
	.end	ddot_
	.text	
	.align	2
	.file	2 "lpackd.f"
	.globl	dscal_
	.loc	2 411
 # 411	      SUBROUTINE  DSCAL(N,DA,DX,INCX)
	.ent	dscal_ 2
dscal_:
	.option	O1
	subu	$sp, 24
	.frame	$sp, 24, $31
	.loc	2 420
 # 420	      IF(N.LE.0)RETURN
	lw	$14, 0($4)
	ble	$14, 0, $101
	.loc	2 421
 # 421	      IF(INCX.EQ.1)GO TO 20
	lw	$15, 0($7)
	beq	$15, 1, $96
	.loc	2 425
 # 422	C
 # 423	C        CODE FOR INCREMENT NOT EQUAL TO 1
 # 424	C
 # 425	      NINCX = N*INCX
	lw	$24, 0($4)
	lw	$25, 0($7)
	mul	$8, $24, $25
	sw	$8, 20($sp)
	.loc	2 426
 # 426	      DO 10 I = 1,NINCX,INCX
	sw	$8, 12($sp)
	.loc	2 426
	lw	$9, 0($7)
	sw	$9, 8($sp)
	.loc	2 426
	li	$10, 1
	sw	$10, 16($sp)
	.loc	2 426
	lw	$11, 8($sp)
	ble	$11, 0, $93
	.loc	2 426
	bge	$8, 1, $94
	b	$101
$93:
	.loc	2 426
	lw	$12, 16($sp)
	lw	$13, 12($sp)
	blt	$12, $13, $101
$94:
	.loc	2 427
 # 427	        DX(I) = DA*DX(I)
	l.d	$f4, 0($5)
	lw	$14, 16($sp)
	mul	$15, $14, 8
	addu	$24, $6, $15
	l.d	$f6, -8($24)
	mul.d	$f8, $f4, $f6
	addu	$25, $6, $15
	s.d	$f8, -8($25)
	.loc	2 428
 # 428	   10 CONTINUE
	lw	$9, 16($sp)
	lw	$10, 8($sp)
	addu	$11, $9, $10
	sw	$11, 16($sp)
	.loc	2 428
	ble	$10, 0, $95
	.loc	2 428
	lw	$8, 12($sp)
	ble	$11, $8, $94
	b	$101
$95:
	.loc	2 428
	lw	$12, 16($sp)
	lw	$13, 12($sp)
	bge	$12, $13, $94
	b	$101
$96:
	.loc	2 436
 # 436	   20 M = MOD(N,5)
	lw	$14, 0($4)
	rem	$24, $14, 5
	sw	$24, 4($sp)
	.loc	2 437
 # 437	      IF( M .EQ. 0 ) GO TO 40
	beq	$24, 0, $99
	.loc	2 438
 # 438	      DO 30 I = 1,M
	li	$15, 1
	sw	$15, 16($sp)
	lw	$25, 4($sp)
	sw	$25, 8($sp)
	blt	$25, 1, $98
	.loc	2 438
	addu	$9, $25, 1
	sw	$9, 8($sp)
$97:
	.loc	2 439
 # 439	        DX(I) = DA*DX(I)
	l.d	$f10, 0($5)
	lw	$10, 16($sp)
	mul	$11, $10, 8
	addu	$8, $6, $11
	l.d	$f16, -8($8)
	mul.d	$f18, $f10, $f16
	addu	$12, $6, $11
	s.d	$f18, -8($12)
	.loc	2 440
 # 440	   30 CONTINUE
	lw	$13, 16($sp)
	addu	$14, $13, 1
	sw	$14, 16($sp)
	lw	$24, 8($sp)
	bne	$14, $24, $97
$98:
	.loc	2 441
 # 441	      IF( N .LT. 5 ) RETURN
	lw	$15, 0($4)
	blt	$15, 5, $101
$99:
	.loc	2 442
 # 442	   40 MP1 = M + 1
	lw	$25, 4($sp)
	addu	$9, $25, 1
	sw	$9, 0($sp)
	.loc	2 443
 # 443	      DO 50 I = MP1,N,5
	sw	$9, 16($sp)
	lw	$10, 0($4)
	sw	$10, 8($sp)
	lw	$8, 8($sp)
	bgt	$9, $8, $101
$100:
	.loc	2 444
 # 444	        DX(I) = DA*DX(I)
	l.d	$f4, 0($5)
	lw	$11, 16($sp)
	mul	$12, $11, 8
	addu	$13, $6, $12
	l.d	$f6, -8($13)
	mul.d	$f8, $f4, $f6
	addu	$14, $6, $12
	s.d	$f8, -8($14)
	.loc	2 445
 # 445	        DX(I + 1) = DA*DX(I + 1)
	l.d	$f10, 0($5)
	lw	$24, 16($sp)
	mul	$15, $24, 8
	addu	$25, $6, $15
	l.d	$f16, 0($25)
	mul.d	$f18, $f10, $f16
	addu	$10, $6, $15
	s.d	$f18, 0($10)
	.loc	2 446
 # 446	        DX(I + 2) = DA*DX(I + 2)
	l.d	$f4, 0($5)
	lw	$9, 16($sp)
	mul	$8, $9, 8
	addu	$11, $6, $8
	l.d	$f6, 8($11)
	mul.d	$f8, $f4, $f6
	addu	$13, $6, $8
	s.d	$f8, 8($13)
	.loc	2 447
 # 447	        DX(I + 3) = DA*DX(I + 3)
	l.d	$f10, 0($5)
	lw	$12, 16($sp)
	mul	$14, $12, 8
	addu	$24, $6, $14
	l.d	$f16, 16($24)
	mul.d	$f18, $f10, $f16
	addu	$25, $6, $14
	s.d	$f18, 16($25)
	.loc	2 448
 # 448	        DX(I + 4) = DA*DX(I + 4)
	l.d	$f4, 0($5)
	lw	$15, 16($sp)
	mul	$10, $15, 8
	addu	$9, $6, $10
	l.d	$f6, 24($9)
	mul.d	$f8, $f4, $f6
	addu	$11, $6, $10
	s.d	$f8, 24($11)
	.loc	2 449
 # 449	   50 CONTINUE
	lw	$8, 16($sp)
	addu	$13, $8, 5
	sw	$13, 16($sp)
	lw	$12, 8($sp)
	ble	$13, $12, $100
	b	$101
$101:
	addu	$sp, 24
	j	$31
	.end	dscal_
	.text	
	.align	2
	.file	2 "lpackd.f"
	.globl	idamax_
	.loc	2 452
 # 450	      RETURN
 # 451	      END
 # 452	      INTEGER FUNCTION IDAMAX(N,DX,INCX)
	.ent	idamax_ 2
idamax_:
	.option	O1
	subu	$sp, 32
	.frame	$sp, 32, $31
	.loc	2 460
 # 460	      IDAMAX = 0
	sw	$0, 24($sp)
	.loc	2 461
 # 461	      IF( N .LT. 1 ) RETURN
	lw	$14, 0($4)
	ble	$14, 0, $107
	.loc	2 462
 # 462	      IDAMAX = 1
	li	$15, 1
	sw	$15, 24($sp)
	.loc	2 463
 # 463	      IF(N.EQ.1)RETURN
	lw	$24, 0($4)
	beq	$24, 1, $107
	.loc	2 464
 # 464	      IF(INCX.EQ.1)GO TO 20
	lw	$25, 0($6)
	beq	$25, 1, $104
	.loc	2 468
 # 465	C
 # 466	C        CODE FOR INCREMENT NOT EQUAL TO 1
 # 467	C
 # 468	      IX = 1
	li	$8, 1
	sw	$8, 20($sp)
	.loc	2 469
 # 469	      DMAX = DABS(DX(1))
	l.d	$f4, 0($5)
	abs.d	$f6, $f4
	s.d	$f6, 8($sp)
	.loc	2 470
 # 470	      IX = IX + INCX
	lw	$9, 0($6)
	addu	$10, $9, 1
	sw	$10, 20($sp)
	.loc	2 471
 # 471	      DO 10 I = 2,N
	li	$11, 2
	sw	$11, 4($sp)
	lw	$12, 0($4)
	sw	$12, 0($sp)
	lw	$13, 0($sp)
	blt	$13, 2, $107
	.loc	2 471
	lw	$14, 0($4)
	addu	$15, $14, 1
	sw	$15, 0($sp)
$102:
	.loc	2 472
 # 472	         IF(DABS(DX(IX)).LE.DMAX) GO TO 5
	lw	$24, 20($sp)
	mul	$25, $24, 8
	addu	$8, $5, $25
	l.d	$f8, -8($8)
	abs.d	$f10, $f8
	l.d	$f16, 8($sp)
	c.le.d	$f10, $f16
	bc1t	$103
	.loc	2 473
 # 473	         IDAMAX = I
	lw	$9, 4($sp)
	sw	$9, 24($sp)
	.loc	2 474
 # 474	         DMAX = DABS(DX(IX))
	lw	$10, 20($sp)
	mul	$11, $10, 8
	addu	$12, $5, $11
	l.d	$f18, -8($12)
	abs.d	$f4, $f18
	s.d	$f4, 8($sp)
$103:
	.loc	2 475
 # 475	    5    IX = IX + INCX
	lw	$13, 20($sp)
	lw	$14, 0($6)
	addu	$15, $13, $14
	sw	$15, 20($sp)
	.loc	2 476
 # 476	   10 CONTINUE
	lw	$24, 4($sp)
	addu	$25, $24, 1
	sw	$25, 4($sp)
	lw	$8, 0($sp)
	bne	$25, $8, $102
	b	$107
$104:
	.loc	2 481
 # 477	      RETURN
 # 478	C
 # 479	C        CODE FOR INCREMENT EQUAL TO 1
 # 480	C
 # 481	   20 DMAX = DABS(DX(1))
	l.d	$f6, 0($5)
	abs.d	$f8, $f6
	s.d	$f8, 8($sp)
	.loc	2 482
 # 482	      DO 30 I = 2,N
	li	$9, 2
	sw	$9, 4($sp)
	lw	$10, 0($4)
	sw	$10, 0($sp)
	lw	$11, 0($sp)
	blt	$11, 2, $107
	.loc	2 482
	lw	$12, 0($4)
	addu	$13, $12, 1
	sw	$13, 0($sp)
$105:
	.loc	2 483
 # 483	         IF(DABS(DX(I)).LE.DMAX) GO TO 30
	lw	$14, 4($sp)
	mul	$15, $14, 8
	addu	$24, $5, $15
	l.d	$f10, -8($24)
	abs.d	$f16, $f10
	l.d	$f18, 8($sp)
	c.le.d	$f16, $f18
	bc1t	$106
	.loc	2 484
 # 484	         IDAMAX = I
	lw	$25, 4($sp)
	sw	$25, 24($sp)
	.loc	2 485
 # 485	         DMAX = DABS(DX(I))
	mul	$8, $25, 8
	addu	$9, $5, $8
	l.d	$f4, -8($9)
	abs.d	$f6, $f4
	s.d	$f6, 8($sp)
$106:
	.loc	2 486
 # 486	   30 CONTINUE
	lw	$10, 4($sp)
	addu	$11, $10, 1
	sw	$11, 4($sp)
	lw	$12, 0($sp)
	bne	$11, $12, $105
$107:
	.loc	2 488
 # 487	      RETURN
 # 488	      END
	lw	$2, 24($sp)
	addu	$sp, 32
	j	$31
	.end	idamax_
	.text	
	.align	2
	.file	2 "lpackd.f"
	.globl	epslon_
	.loc	2 489
 # 489	      DOUBLE PRECISION FUNCTION EPSLON (X)
	.ent	epslon_ 2
epslon_:
	.option	O1
	subu	$sp, 40
	.frame	$sp, 40, $31
	.loc	2 522
 # 522	      A = 4.0D0/3.0D0
	li.d	$f4, 1.3333333333333333e+00
	s.d	$f4, 24($sp)
$108:
	.loc	2 523
 # 523	   10 B = A - 1.0D0
	l.d	$f6, 24($sp)
	li.d	$f8, 1.0e0
	sub.d	$f10, $f6, $f8
	s.d	$f10, 16($sp)
	.loc	2 524
 # 524	      C = B + B + B
	add.d	$f16, $f10, $f10
	add.d	$f18, $f16, $f10
	s.d	$f18, 8($sp)
	.loc	2 525
 # 525	      EPS = DABS(C-1.0D0)
	li.d	$f4, 1.0e0
	sub.d	$f6, $f18, $f4
	abs.d	$f8, $f6
	s.d	$f8, 0($sp)
	.loc	2 526
 # 526	      IF (EPS .EQ. 0.0D0) GO TO 10
	li.d	$f16, 0.0e0
	c.eq.d	$f8, $f16
	bc1t	$108
	.loc	2 527
 # 527	      EPSLON = EPS*DABS(X)
	l.d	$f10, 0($sp)
	l.d	$f18, 0($4)
	abs.d	$f4, $f18
	mul.d	$f6, $f10, $f4
	s.d	$f6, 32($sp)
	.loc	2 529
 # 528	      RETURN
 # 529	      END
	l.d	$f0, 32($sp)
	addu	$sp, 40
	j	$31
	.end	epslon_
	.text	
	.align	2
	.file	2 "lpackd.f"
	.globl	mm_
	.loc	2 530
 # 530	      SUBROUTINE MM (A, LDA, N1, N3, B, LDB, N2, C, LDC)
	.ent	mm_ 2
mm_:
	.option	O1
	subu	$sp, 64
	sw	$31, 28($sp)
	sd	$4, 64($sp)
	sd	$6, 72($sp)
	.mask	0x80000000, -36
	.frame	$sp, 64, $31
	.loc	2 530
	lw	$14, 68($sp)
	lw	$15, 0($14)
	sw	$15, 60($sp)
	.loc	2 530
	lw	$24, 84($sp)
	lw	$25, 0($24)
	sw	$25, 56($sp)
	.loc	2 530
	lw	$8, 96($sp)
	lw	$9, 0($8)
	sw	$9, 52($sp)
	.loc	2 559
 # 559	      DO 20 J = 1, N3
	li	$10, 1
	sw	$10, 48($sp)
	lw	$11, 76($sp)
	lw	$12, 0($11)
	sw	$12, 44($sp)
	blt	$12, 1, $112
	.loc	2 559
	lw	$13, 0($11)
	addu	$14, $13, 1
	sw	$14, 44($sp)
$109:
	.loc	2 560
 # 560	         DO 10 I = 1, N1
	li	$15, 1
	sw	$15, 40($sp)
	lw	$24, 72($sp)
	lw	$25, 0($24)
	sw	$25, 36($sp)
	blt	$25, 1, $111
	.loc	2 560
	lw	$8, 0($24)
	addu	$9, $8, 1
	sw	$9, 36($sp)
$110:
	.loc	2 561
 # 561	            A(I,J) = 0.0
	li.d	$f4, 0.0
	lw	$10, 48($sp)
	lw	$12, 60($sp)
	mul	$11, $10, $12
	lw	$13, 40($sp)
	addu	$14, $13, $11
	subu	$15, $14, $12
	mul	$25, $15, 8
	lw	$24, 64($sp)
	addu	$8, $24, $25
	s.d	$f4, -8($8)
	.loc	2 562
 # 562	   10    CONTINUE
	lw	$9, 40($sp)
	addu	$10, $9, 1
	sw	$10, 40($sp)
	lw	$13, 36($sp)
	bne	$10, $13, $110
$111:
	.loc	2 563
 # 563	         CALL DMXPY (N2,A(1,J),N1,LDB,C(1,J),B)
	lw	$4, 88($sp)
	lw	$11, 48($sp)
	lw	$14, 60($sp)
	mul	$12, $11, $14
	subu	$15, $12, $14
	mul	$24, $15, 8
	lw	$25, 64($sp)
	addu	$5, $25, $24
	lw	$6, 72($sp)
	lw	$7, 84($sp)
	lw	$8, 52($sp)
	mul	$9, $11, $8
	subu	$10, $9, $8
	mul	$13, $10, 8
	lw	$12, 92($sp)
	addu	$14, $12, $13
	sw	$14, 16($sp)
	lw	$15, 80($sp)
	sw	$15, 20($sp)
	.livereg	0x0F00FF0E,0x00000FFF
	jal	dmxpy_
	.loc	2 564
 # 564	   20 CONTINUE
	lw	$25, 48($sp)
	addu	$24, $25, 1
	sw	$24, 48($sp)
	lw	$11, 44($sp)
	bne	$24, $11, $109
	b	$112
$112:
	lw	$31, 28($sp)
	addu	$sp, 64
	j	$31
	.end	mm_
	.text	
	.align	2
	.file	2 "lpackd.f"
	.globl	dmxpy_
	.loc	2 568
 # 565	C
 # 566	      RETURN
 # 567	      END
 # 568	      SUBROUTINE DMXPY (N1, Y, N2, LDM, X, M)
	.ent	dmxpy_ 2
dmxpy_:
	.option	O1
	subu	$sp, 24
	.frame	$sp, 24, $31
	.loc	2 568
	lw	$14, 0($7)
	sw	$14, 20($sp)
	.loc	2 595
 # 595	      J = MOD(N2,2)
	lw	$15, 0($6)
	rem	$24, $15, 2
	sw	$24, 16($sp)
	.loc	2 596
 # 596	      IF (J .GE. 1) THEN
	ble	$24, 0, $114
	.loc	2 597
 # 597	         DO 10 I = 1, N1
	li	$25, 1
	sw	$25, 12($sp)
	lw	$8, 0($4)
	sw	$8, 8($sp)
	lw	$9, 8($sp)
	blt	$9, 1, $114
	.loc	2 597
	lw	$10, 0($4)
	addu	$11, $10, 1
	sw	$11, 8($sp)
$113:
	.loc	2 598
 # 598	            Y(I) = (Y(I)) + X(J)*M(I,J)
	lw	$12, 40($sp)
	lw	$13, 16($sp)
	mul	$14, $13, 8
	addu	$15, $12, $14
	l.d	$f4, -8($15)
	lw	$24, 20($sp)
	mul	$25, $13, $24
	lw	$8, 12($sp)
	addu	$9, $8, $25
	subu	$10, $9, $24
	mul	$11, $10, 8
	lw	$12, 44($sp)
	addu	$14, $12, $11
	l.d	$f6, -8($14)
	mul.d	$f8, $f4, $f6
	mul	$15, $8, 8
	addu	$13, $5, $15
	l.d	$f10, -8($13)
	add.d	$f16, $f10, $f8
	addu	$25, $5, $15
	s.d	$f16, -8($25)
	.loc	2 599
 # 599	   10    CONTINUE
	lw	$9, 12($sp)
	addu	$24, $9, 1
	sw	$24, 12($sp)
	lw	$10, 8($sp)
	bne	$24, $10, $113
$114:
	.loc	2 604
 # 600	      ENDIF
 # 601	C
 # 602	C   CLEANUP ODD GROUP OF TWO VECTORS
 # 603	C
 # 604	      J = MOD(N2,4)
	lw	$12, 0($6)
	rem	$11, $12, 4
	sw	$11, 16($sp)
	.loc	2 605
 # 605	      IF (J .GE. 2) THEN
	blt	$11, 2, $116
	.loc	2 606
 # 606	         DO 20 I = 1, N1
	li	$14, 1
	sw	$14, 12($sp)
	lw	$8, 0($4)
	sw	$8, 8($sp)
	lw	$13, 8($sp)
	blt	$13, 1, $116
	.loc	2 606
	lw	$15, 0($4)
	addu	$25, $15, 1
	sw	$25, 8($sp)
$115:
	.loc	2 607
 # 607	            Y(I) = ( (Y(I))
	lw	$9, 40($sp)
	lw	$24, 16($sp)
	mul	$10, $24, 8
	addu	$12, $9, $10
	l.d	$f18, -16($12)
	addu	$11, $24, -1
	lw	$14, 20($sp)
	mul	$8, $11, $14
	lw	$13, 12($sp)
	addu	$15, $13, $8
	subu	$25, $15, $14
	mul	$9, $25, 8
	lw	$10, 44($sp)
	addu	$11, $10, $9
	l.d	$f4, -8($11)
	mul.d	$f6, $f18, $f4
	mul	$8, $13, 8
	addu	$15, $5, $8
	l.d	$f10, -8($15)
	add.d	$f8, $f10, $f6
	l.d	$f16, -8($12)
	mul	$25, $24, $14
	addu	$9, $13, $25
	subu	$11, $9, $14
	mul	$15, $11, 8
	addu	$12, $10, $15
	l.d	$f18, -8($12)
	mul.d	$f4, $f16, $f18
	add.d	$f10, $f8, $f4
	addu	$24, $5, $8
	s.d	$f10, -8($24)
	.loc	2 609
 # 608	     $             + X(J-1)*M(I,J-1)) + X(J)*M(I,J)
 # 609	   20    CONTINUE
	lw	$13, 12($sp)
	addu	$25, $13, 1
	sw	$25, 12($sp)
	lw	$9, 8($sp)
	bne	$25, $9, $115
$116:
	.loc	2 614
 # 610	      ENDIF
 # 611	C
 # 612	C   CLEANUP ODD GROUP OF FOUR VECTORS
 # 613	C
 # 614	      J = MOD(N2,8)
	lw	$14, 0($6)
	rem	$11, $14, 8
	sw	$11, 16($sp)
	.loc	2 615
 # 615	      IF (J .GE. 4) THEN
	blt	$11, 4, $118
	.loc	2 616
 # 616	         DO 30 I = 1, N1
	li	$10, 1
	sw	$10, 12($sp)
	lw	$15, 0($4)
	sw	$15, 8($sp)
	lw	$12, 8($sp)
	blt	$12, 1, $118
	.loc	2 616
	lw	$8, 0($4)
	addu	$24, $8, 1
	sw	$24, 8($sp)
$117:
	.loc	2 617
 # 617	            Y(I) = ((( (Y(I))
	lw	$13, 40($sp)
	lw	$25, 16($sp)
	mul	$9, $25, 8
	addu	$14, $13, $9
	l.d	$f6, -32($14)
	addu	$11, $25, -3
	lw	$10, 20($sp)
	mul	$15, $11, $10
	lw	$12, 12($sp)
	addu	$8, $12, $15
	subu	$24, $8, $10
	mul	$13, $24, 8
	lw	$9, 44($sp)
	addu	$11, $9, $13
	l.d	$f16, -8($11)
	mul.d	$f18, $f6, $f16
	mul	$15, $12, 8
	addu	$8, $5, $15
	l.d	$f8, -8($8)
	add.d	$f4, $f8, $f18
	l.d	$f10, -24($14)
	addu	$24, $25, -2
	mul	$13, $24, $10
	addu	$11, $12, $13
	subu	$8, $11, $10
	mul	$24, $8, 8
	addu	$13, $9, $24
	l.d	$f6, -8($13)
	mul.d	$f16, $f10, $f6
	add.d	$f8, $f4, $f16
	l.d	$f18, -16($14)
	addu	$11, $25, -1
	mul	$8, $11, $10
	addu	$24, $12, $8
	subu	$13, $24, $10
	mul	$11, $13, 8
	addu	$8, $9, $11
	l.d	$f10, -8($8)
	mul.d	$f6, $f18, $f10
	add.d	$f4, $f8, $f6
	l.d	$f16, -8($14)
	mul	$24, $25, $10
	addu	$13, $12, $24
	subu	$11, $13, $10
	mul	$8, $11, 8
	addu	$14, $9, $8
	l.d	$f18, -8($14)
	mul.d	$f10, $f16, $f18
	add.d	$f8, $f4, $f10
	addu	$25, $5, $15
	s.d	$f8, -8($25)
	.loc	2 620
 # 618	     $             + X(J-3)*M(I,J-3)) + X(J-2)*M(I,J-2))
 # 619	     $             + X(J-1)*M(I,J-1)) + X(J)  *M(I,J)
 # 620	   30    CONTINUE
	lw	$12, 12($sp)
	addu	$24, $12, 1
	sw	$24, 12($sp)
	lw	$13, 8($sp)
	bne	$24, $13, $117
$118:
	.loc	2 625
 # 621	      ENDIF
 # 622	C
 # 623	C   CLEANUP ODD GROUP OF EIGHT VECTORS
 # 624	C
 # 625	      J = MOD(N2,16)
	lw	$10, 0($6)
	rem	$11, $10, 16
	sw	$11, 16($sp)
	.loc	2 626
 # 626	      IF (J .GE. 8) THEN
	blt	$11, 8, $120
	.loc	2 627
 # 627	         DO 40 I = 1, N1
	li	$9, 1
	sw	$9, 12($sp)
	lw	$8, 0($4)
	sw	$8, 8($sp)
	lw	$14, 8($sp)
	blt	$14, 1, $120
	.loc	2 627
	lw	$15, 0($4)
	addu	$25, $15, 1
	sw	$25, 8($sp)
$119:
	.loc	2 628
 # 628	            Y(I) = ((((((( (Y(I))
	lw	$12, 40($sp)
	lw	$24, 16($sp)
	mul	$13, $24, 8
	addu	$10, $12, $13
	l.d	$f6, -64($10)
	addu	$11, $24, -7
	lw	$9, 20($sp)
	mul	$8, $11, $9
	lw	$14, 12($sp)
	addu	$15, $14, $8
	subu	$25, $15, $9
	mul	$12, $25, 8
	lw	$13, 44($sp)
	addu	$11, $13, $12
	l.d	$f16, -8($11)
	mul.d	$f18, $f6, $f16
	mul	$8, $14, 8
	addu	$15, $5, $8
	l.d	$f4, -8($15)
	add.d	$f10, $f4, $f18
	l.d	$f8, -56($10)
	addu	$25, $24, -6
	mul	$12, $25, $9
	addu	$11, $14, $12
	subu	$15, $11, $9
	mul	$25, $15, 8
	addu	$12, $13, $25
	l.d	$f6, -8($12)
	mul.d	$f16, $f8, $f6
	add.d	$f4, $f10, $f16
	l.d	$f18, -48($10)
	addu	$11, $24, -5
	mul	$15, $11, $9
	addu	$25, $14, $15
	subu	$12, $25, $9
	mul	$11, $12, 8
	addu	$15, $13, $11
	l.d	$f8, -8($15)
	mul.d	$f6, $f18, $f8
	add.d	$f10, $f4, $f6
	l.d	$f16, -40($10)
	addu	$25, $24, -4
	mul	$12, $25, $9
	addu	$11, $14, $12
	subu	$15, $11, $9
	mul	$25, $15, 8
	addu	$12, $13, $25
	l.d	$f18, -8($12)
	mul.d	$f8, $f16, $f18
	add.d	$f4, $f10, $f8
	l.d	$f6, -32($10)
	addu	$11, $24, -3
	mul	$15, $11, $9
	addu	$25, $14, $15
	subu	$12, $25, $9
	mul	$11, $12, 8
	addu	$15, $13, $11
	l.d	$f16, -8($15)
	mul.d	$f18, $f6, $f16
	add.d	$f10, $f4, $f18
	l.d	$f8, -24($10)
	addu	$25, $24, -2
	mul	$12, $25, $9
	addu	$11, $14, $12
	subu	$15, $11, $9
	mul	$25, $15, 8
	addu	$12, $13, $25
	l.d	$f6, -8($12)
	mul.d	$f16, $f8, $f6
	add.d	$f4, $f10, $f16
	l.d	$f18, -16($10)
	addu	$11, $24, -1
	mul	$15, $11, $9
	addu	$25, $14, $15
	subu	$12, $25, $9
	mul	$11, $12, 8
	addu	$15, $13, $11
	l.d	$f8, -8($15)
	mul.d	$f6, $f18, $f8
	add.d	$f10, $f4, $f6
	l.d	$f16, -8($10)
	mul	$25, $24, $9
	addu	$12, $14, $25
	subu	$11, $12, $9
	mul	$15, $11, 8
	addu	$10, $13, $15
	l.d	$f18, -8($10)
	mul.d	$f8, $f16, $f18
	add.d	$f4, $f10, $f8
	addu	$24, $5, $8
	s.d	$f4, -8($24)
	.loc	2 633
 # 629	     $             + X(J-7)*M(I,J-7)) + X(J-6)*M(I,J-6))
 # 630	     $             + X(J-5)*M(I,J-5)) + X(J-4)*M(I,J-4))
 # 631	     $             + X(J-3)*M(I,J-3)) + X(J-2)*M(I,J-2))
 # 632	     $             + X(J-1)*M(I,J-1)) + X(J)  *M(I,J)
 # 633	   40    CONTINUE
	lw	$14, 12($sp)
	addu	$25, $14, 1
	sw	$25, 12($sp)
	lw	$12, 8($sp)
	bne	$25, $12, $119
$120:
	.loc	2 638
 # 634	      ENDIF
 # 635	C
 # 636	C   MAIN LOOP - GROUPS OF SIXTEEN VECTORS
 # 637	C
 # 638	      JMIN = J+16
	lw	$9, 16($sp)
	addu	$11, $9, 16
	sw	$11, 4($sp)
	.loc	2 639
 # 639	      DO 60 J = JMIN, N2, 16
	sw	$11, 16($sp)
	lw	$13, 0($6)
	sw	$13, 8($sp)
	lw	$15, 8($sp)
	bgt	$11, $15, $124
$121:
	.loc	2 640
 # 640	         DO 50 I = 1, N1
	li	$10, 1
	sw	$10, 12($sp)
	lw	$8, 0($4)
	sw	$8, 0($sp)
	lw	$24, 0($sp)
	blt	$24, 1, $123
	.loc	2 640
	lw	$14, 0($4)
	addu	$25, $14, 1
	sw	$25, 0($sp)
$122:
	.loc	2 641
 # 641	            Y(I) = ((((((((((((((( (Y(I))
	lw	$12, 40($sp)
	lw	$9, 16($sp)
	mul	$13, $9, 8
	addu	$11, $12, $13
	l.d	$f6, -128($11)
	addu	$15, $9, -15
	lw	$10, 20($sp)
	mul	$8, $15, $10
	lw	$24, 12($sp)
	addu	$14, $24, $8
	subu	$25, $14, $10
	mul	$12, $25, 8
	lw	$13, 44($sp)
	addu	$15, $13, $12
	l.d	$f16, -8($15)
	mul.d	$f18, $f6, $f16
	mul	$8, $24, 8
	addu	$14, $5, $8
	l.d	$f10, -8($14)
	add.d	$f8, $f10, $f18
	l.d	$f4, -120($11)
	addu	$25, $9, -14
	mul	$12, $25, $10
	addu	$15, $24, $12
	subu	$14, $15, $10
	mul	$25, $14, 8
	addu	$12, $13, $25
	l.d	$f6, -8($12)
	mul.d	$f16, $f4, $f6
	add.d	$f10, $f8, $f16
	l.d	$f18, -112($11)
	addu	$15, $9, -13
	mul	$14, $15, $10
	addu	$25, $24, $14
	subu	$12, $25, $10
	mul	$15, $12, 8
	addu	$14, $13, $15
	l.d	$f4, -8($14)
	mul.d	$f6, $f18, $f4
	add.d	$f8, $f10, $f6
	l.d	$f16, -104($11)
	addu	$25, $9, -12
	mul	$12, $25, $10
	addu	$15, $24, $12
	subu	$14, $15, $10
	mul	$25, $14, 8
	addu	$12, $13, $25
	l.d	$f18, -8($12)
	mul.d	$f4, $f16, $f18
	add.d	$f10, $f8, $f4
	l.d	$f6, -96($11)
	addu	$15, $9, -11
	mul	$14, $15, $10
	addu	$25, $24, $14
	subu	$12, $25, $10
	mul	$15, $12, 8
	addu	$14, $13, $15
	l.d	$f16, -8($14)
	mul.d	$f18, $f6, $f16
	add.d	$f8, $f10, $f18
	l.d	$f4, -88($11)
	addu	$25, $9, -10
	mul	$12, $25, $10
	addu	$15, $24, $12
	subu	$14, $15, $10
	mul	$25, $14, 8
	addu	$12, $13, $25
	l.d	$f6, -8($12)
	mul.d	$f16, $f4, $f6
	add.d	$f10, $f8, $f16
	l.d	$f18, -80($11)
	addu	$15, $9, -9
	mul	$14, $15, $10
	addu	$25, $24, $14
	subu	$12, $25, $10
	mul	$15, $12, 8
	addu	$14, $13, $15
	l.d	$f4, -8($14)
	mul.d	$f6, $f18, $f4
	add.d	$f8, $f10, $f6
	l.d	$f16, -72($11)
	addu	$25, $9, -8
	mul	$12, $25, $10
	addu	$15, $24, $12
	subu	$14, $15, $10
	mul	$25, $14, 8
	addu	$12, $13, $25
	l.d	$f18, -8($12)
	mul.d	$f4, $f16, $f18
	add.d	$f10, $f8, $f4
	l.d	$f6, -64($11)
	addu	$15, $9, -7
	mul	$14, $15, $10
	addu	$25, $24, $14
	subu	$12, $25, $10
	mul	$15, $12, 8
	addu	$14, $13, $15
	l.d	$f16, -8($14)
	mul.d	$f18, $f6, $f16
	add.d	$f8, $f10, $f18
	l.d	$f4, -56($11)
	addu	$25, $9, -6
	mul	$12, $25, $10
	addu	$15, $24, $12
	subu	$14, $15, $10
	mul	$25, $14, 8
	addu	$12, $13, $25
	l.d	$f6, -8($12)
	mul.d	$f16, $f4, $f6
	add.d	$f10, $f8, $f16
	l.d	$f18, -48($11)
	addu	$15, $9, -5
	mul	$14, $15, $10
	addu	$25, $24, $14
	subu	$12, $25, $10
	mul	$15, $12, 8
	addu	$14, $13, $15
	l.d	$f4, -8($14)
	mul.d	$f6, $f18, $f4
	add.d	$f8, $f10, $f6
	l.d	$f16, -40($11)
	addu	$25, $9, -4
	mul	$12, $25, $10
	addu	$15, $24, $12
	subu	$14, $15, $10
	mul	$25, $14, 8
	addu	$12, $13, $25
	l.d	$f18, -8($12)
	mul.d	$f4, $f16, $f18
	add.d	$f10, $f8, $f4
	l.d	$f6, -32($11)
	addu	$15, $9, -3
	mul	$14, $15, $10
	addu	$25, $24, $14
	subu	$12, $25, $10
	mul	$15, $12, 8
	addu	$14, $13, $15
	l.d	$f16, -8($14)
	mul.d	$f18, $f6, $f16
	add.d	$f8, $f10, $f18
	l.d	$f4, -24($11)
	addu	$25, $9, -2
	mul	$12, $25, $10
	addu	$15, $24, $12
	subu	$14, $15, $10
	mul	$25, $14, 8
	addu	$12, $13, $25
	l.d	$f6, -8($12)
	mul.d	$f16, $f4, $f6
	add.d	$f10, $f8, $f16
	l.d	$f18, -16($11)
	addu	$15, $9, -1
	mul	$14, $15, $10
	addu	$25, $24, $14
	subu	$12, $25, $10
	mul	$15, $12, 8
	addu	$14, $13, $15
	l.d	$f4, -8($14)
	mul.d	$f6, $f18, $f4
	add.d	$f8, $f10, $f6
	l.d	$f16, -8($11)
	mul	$25, $9, $10
	addu	$12, $24, $25
	subu	$15, $12, $10
	mul	$14, $15, 8
	addu	$11, $13, $14
	l.d	$f18, -8($11)
	mul.d	$f4, $f16, $f18
	add.d	$f10, $f8, $f4
	addu	$9, $5, $8
	s.d	$f10, -8($9)
	.loc	2 650
 # 650	   50    CONTINUE
	lw	$24, 12($sp)
	addu	$25, $24, 1
	sw	$25, 12($sp)
	lw	$12, 0($sp)
	bne	$25, $12, $122
$123:
	.loc	2 651
 # 651	   60 CONTINUE
	lw	$10, 16($sp)
	addu	$15, $10, 16
	sw	$15, 16($sp)
	lw	$13, 8($sp)
	ble	$15, $13, $121
	b	$124
$124:
	addu	$sp, 24
	j	$31
	.end	dmxpy_
