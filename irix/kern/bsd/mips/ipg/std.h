;Common software definitions for the FDDIXPress, 6U FDDI board
;	"$Revision: 1.13 $"


;move between registers without affecting ALU flags or being confusing
 .macro	mov, reg1,reg2
	  sll	  reg1,reg2,0
 .endm


;load a value which we promise is <64K
 .macro	const0, reg,val
 .if	(val)>0xffff
	.print	" ** ERROR: @val@ does not fit in 16 bits"
	.err
 .endif
	  const	  reg,val
 .endm


;load an absolute value
 .macro	constx, reg,val
 .if	((val) & 0xffff0000) == 0xffff0000
	  constn  reg,val
   .exitm
 .endif
	  const	  reg,val
 .if	(val)>0xffff
	  consth  reg,val
 .endif
 .endm


;define a bit
 .macro	DEFBT,	nam, bitno
	.equ	nam,1<<(bitno)
 .if	(bitno)>31
	.print	" ** ERROR: @bitno@ is an invalid bit number"
	.err
 .endif
	.equ	@nam@_BT,bitno
 .endm
	
;define the next bit
 .macro	NXTBT,	nam
	DEFBT	nam,NEXT_BT
	.set	NEXT_BT,NEXT_BT+1
 .endm


;check a bit
;    handy to prevent the common errors of forgetting the "31-" or "_BT"
 .macro	tbit,	tgt, src, bitnam
	  sll	  tgt,src,31-@bitnam@_BT
 .endm


;check that something is true
 .macro	CK, exp
 .if	!(exp)
	.print	" ** ERROR: !(@exp@)"
	.err
 .endif
 .endm

;check that a value is a power of 2
 .macro	CK2, exp
 .if	(exp) != ((exp) & -(exp))
	.print	" ** ERROR: @exp@ is not a power of 2"
	.err
 .endif
 .endm

;align data
 .macro DALIGN, exp
	  .block (0-.) & ((exp)-1)
 .endm

	.equ	TRUE,0x80000000
	.equ	FALSE,0


;Common register definitions
	.reg	rsp, gr1	;Register Stack Pointer

	;.reg	xxx, gr64-gr95	;available

	.reg	v0, gr96	;16 function value return regs
	.reg	v1, gr97
	.reg	v2, gr98
	.reg	v3, gr99
	.reg	v4, gr100
	.reg	v5, gr101
	.reg	v6, gr102
	.reg	v7, gr103
 .ifndef FEW_VS
	.reg	v8, gr104
	.reg	v9, gr105
	.reg	v10,gr106
	.reg	v11,gr107
	.reg	v12,gr108
	.reg	v13,gr109
	.reg	v14,gr110
	.reg	v15,gr111
 .endif

;	.reg	ret,gr96	;First word of return value
	;gr97-gr111 or v1-v15 are the rest of the return value

	.reg	u0, gr112	;"Reserved for Programmer" by Metaware
	.reg	u1, gr113
	.reg	u2, gr114
	.reg	u3, gr115

	.reg    t0, gr116	;temporaries
	.reg    t1, gr117
	.reg    t2, gr118
	.reg    t3, gr119
	.reg    t4, gr120

	.reg	tav,gr121	;Temporary, Argument for Trap Handlers
	.reg	tpc,gr122	;Temporary, Return PC for Trap Handlers
	.reg	lrp,gr123	;Large Return Pointer
	.reg	slp,gr124	;Static Link Pointer
	.reg	msp,gr125	;Memory Stack Pointer
	.reg	rab,gr126	;Register Allocate Bound
	.reg	rfb,gr127	;Register Free Bound


	.reg	raddr,lr0	;subroutine link



;common cntl field values
	.equ	AS, 0x40
	.equ	PA, 0x20
	.equ	SB, 0x10
	.equ	UA, 0x08
	.equ	WORD, 0
	.equ	BYTE, 1
	.equ	HWORD,2

;common cps values
	.equ	PS_DA,	0x001
	.equ	PS_DI,	0x002
	.equ	PS_RE,	0x100
	.equ	PS_FE,	0x400
	.equ	PS_IM,	0x00c
	.equ	PS_TU,	0x0800
	.equ	PS_INTSON, 0x070 | PS_IM
	.equ	PS_INTSOFF, PS_INTSON | PS_DI | PS_DA
	.equ	PS_FREEZE, PS_INTSOFF | PS_FE

;common cfg values
	.equ	CFG_DW,	0x20
	.equ	CFG_VF,	0x10
	.equ	CFG_CD,	0x01

;timer bits
	DEFBT	TMR_IE, 24
	DEFBT	TMR_IN, 25
	DEFBT	TMR_OV, 26


;common trap numbers
	.equ	V_MULIPLY,  32
	.equ	V_DIVIDE,   33
	.equ	V_MULIPLU,  34
	.equ	V_DIVIDU,   35

	.equ	V_Timer,    14

	.equ	V_INTR0,    16
	.equ	V_INTR1,    17
	.equ	V_INTR2,    18
	.equ	V_INTR3,    19
	.equ	V_TRAP0,    20
	.equ	V_TRAP1,    21

	.equ	V_SPILL,64
	.equ	V_FILL,	65

	.equ	NUM_V,	256	;# of traps and interrupts
