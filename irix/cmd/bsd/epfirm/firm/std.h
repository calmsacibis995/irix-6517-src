;Common software definitions for the 29K firmware

; Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
;	"$Revision: 1.14 $"


;move between registers without affecting ALU flags
 .macro	mov, reg1,reg2
	  sll	  reg1,reg2,0
 .endm


;conditional call
 .macro	callt, reg,ret,tgt,branch
	CK	reg != ret
    .ifeqs "@branch@",""
	  const	  ret,$1
	  jmpt	  reg,tgt
	   consth ret,$1
$1:
    .else
	  jmpf	  reg,branch
	   const  ret,branch
	  jmp	  tgt
	   consth ret,branch
    .endif
 .endm

;load a constant which we promise is <64K
 .macro	const0, reg,val
  .if	(val) != 0x8000000 && ((val)&0xffff0000) != 0 && ((val)&0xffff0000) != 0xffff0000
	.print	" ** Error: @val@ cannot be set in one instruction"
	.err
  .endif
	constx	reg,val
 .endm


;load a constant
 .macro	constx, reg,val
  .if	((val) & 0xffff0000) == 0xffff0000
	  constn  reg,val
    .exitm
  .endif
  .if	(val) == 0x80000000
	  cpeq	  reg,reg,reg
    .exitm
  .endif
	  const	  reg,val
  .if	((val) & 0xffff0000) != 0
	  consth  reg,val
  .endif
 .endm


;load the contents of a memory location
 .macro	lod,	reg,addr
	  constx reg,addr
	  load	  0,WORD,reg,reg
 .endm


;store a register
 .macro	sto,	val,addr,st1,st2
  .if $ISREG(addr)
    .if $ISREG(val)
	  store	  0,WORD,val,addr
    .else
	  constx  st1,val
	  store	  0,WORD,st1,addr
    .endif
  .else
	  constx  st1,addr
    .if $ISREG(val)
	  store	  0,WORD,val,st1
    .else
	  constx  st2,val
	  store	  0,WORD,st2,st1
    .endif
  .endif
 .endm


;read or write (op) a value (val) an address (tag) within "structure" (strc)
;   which has a pointer in a register (p_strc) using a temporary
;   register (tmp)
 .macro	OPST,	op,wid,strc,p_strc,tag,val,tmp,fs
  .if tag == strc
    .ifeqs "@fs@","fast"
    .else
      .ifnes "@fs@",""
	.print "** Error: @fs@ is not 'fast' or null"
	.err
      .endif
	  mtsrim  MMU,0xff
    .endif
	  op	0,@wid@WORD,val,p_strc
  .else
    .if tag > strc
      .if strc == 0
	  constx  tmp,tag
      .else
	  add	  tmp,p_strc,tag-strc
      .endif
    .else
	  sub	  tmp,p_strc,strc-tag
    .endif
    .ifeqs "@fs@","fast"
    .else
      .ifnes "@fs@",""
	.print "** Error: @fs@ is not 'fast' or null"
	.err
      .endif
	  mtsrim  MMU,0xff
    .endif
	  op	  0,@wid@WORD,val,tmp
  .endif
 .endm


;define a bit
 .macro	DEFBT,	nam, bitno
	.equ	nam,1<<(bitno)
  .if	(bitno)>31
	.print	" ** Error: '@bitno@' is an invalid bit number"
	.err
  .endif
	.equ	@nam@_BT,bitno
 .endm

;define the next bit in a register
;   NEXT_BT should be set =0 before each series of uses of this macro.
 .macro	NXTBT,	nam
	DEFBT	nam,NEXT_BT
	.set	NEXT_BT,NEXT_BT+1
 .endm

;check a bit
;    handy to prevent the common errors of forgetting the "31-" or "_BT"
 .macro	tbit,	tgt, src, bitnam
	  sll	  tgt,src,31-@bitnam@_BT
 .endm


;move a bit from one register to another
 .macro	mv_bt,	tgt,tbit, src,sbit
   .if @tbit@_BT < @sbit@_BT
	  srl	  tgt,src, @sbit@_BT-@tbit@_BT
   .else
     .if @tbit@_BT > @sbit@_BT || tgt != src
	  sll	  tgt,src, @tbit@_BT-@sbit@_BT
     .endif
   .endif
 .endm


;check that something is true
 .macro	CK, exp
  .if	!(exp)
	.print	" ** Error: !(@exp@)"
	.err
  .endif
 .endm

;check that a value is a power of 2
 .macro	CK2, exp
  .if	(exp) != ((exp) & -(exp))
	.print	" ** Error: @exp@ is not a power of 2"
	.err
  .endif
 .endm

;align data
 .macro	DALIGN, exp
	  .block (0-.) & ((exp)-1)
 .endm

;check alignment
 .macro CKALIGN, exp
  .if	((0-.) & ((exp)-1)) != 0
	.print	" ** Error: not aligned to @exp@"
	.err
  .endif
 .endm


;common constants
	.equ	TRUE,0x80000000
	.equ	FALSE,0

	.equ	USEC_SEC, 1000000


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


;These macros allocate register, saving the hassle of ensuring that the
;	same register is not allocated twice.
;   greg allocates the next general purpose register,
;   lreg allocates the next local register,

;   A variable with the same name as the register but with "_" appended
;	is defined to make it easier to discover the number of the register
;	in the Metaware as29 listing.

;Allocate a general purpose register register
 .macro	greg,	nam
    .if NEXT_GREG >= 112
	.print	" ** Error:  no more rgisters available for @nam@"
	.err
    .endif
	.reg	nam, %%NEXT_GREG
   .if NEXT_GREG==95
	.set	NEXT_GREG, 104		;use the extra V registers
     .ifdef v8
	.set	NEXT_GREG, 112		;unless they are not available
     .endif
   .else
	.set	NEXT_GREG, NEXT_GREG+1
   .endif
 .endm

;Allocate a local register
 .macro	lreg,	nam
    .if NEXT_LREG >= 128
	.print	" ** Error:  no more rgisters available for @nam@"
	.err
    .endif
	.reg	nam, %%(NEXT_LREG+128)
	.set	NEXT_LREG, NEXT_LREG+1
 .endm

;   say how to start generating registers
	.set	NEXT_LREG, 0		;start at lr0
	.set	NEXT_GREG, 64		;start at gr64


;common CNTL field values for load and store instructions.
	.equ	AS, 0x40
	.equ	PA, 0x20
	.equ	SB, 0x10
	.equ	UA, 0x08
	.equ	WORD, 0
	.equ	BYTE, 1
	.equ	HWORD,2

;common CPS and OPS values
	DEFBT	PS_DA,	0
	DEFBT	PS_DI,	1
	DEFBT	PS_SM,	4
	DEFBT	PS_PI,	5
	DEFBT	PS_PD,	6
	DEFBT	PS_WM,	7
    ;	DEFBT	PS_RE,	8		;29000 only
	DEFBT	PS_FZ,	10
	.equ	PS_IM,	0x00c
	DEFBT	PS_TU,	11
	DEFBT	PS_IP,	14
  .ifdef AM20935
	DEFBT	PS_TD,	17
  .endif
	.equ	PS_INTSON, PS_SM | PS_PI | PS_PD | PS_IM
	.equ	PS_INTSOFF, PS_INTSON | PS_DI | PS_DA
	.equ	PS_FREEZE, PS_INTSOFF | PS_FZ

;common CFG values
	.equ	CFG_PMB_16K,0x00030000
  .ifdef AM20935
	DEFBT	CFG_D16,    15
	.equ	CFG_ILU,    0x00000600	;unlock cache
	.equ	CFG_IL,	    0x00000000	;lock cache
  .endif
	DEFBT	CFG_ID,	    8		;disable cache
  .ifdef AM29030
	.equ	CFG_ILU,    0x00000000	;unlock cache
	.equ	CFG_IL,	    0x00000600	;lock cache
  .endif
    ;	DEFBT	CFG_DW,	    5		;29000 only
    ;	DEFBT	CFG_VF,	    4		;29000 only
	DEFBT	CFG_BO,	    2
    ;	DEFBT	CFG_CD,	    0		;29000 only

;bits in Cache Interface Register
	.equ	CIR_FSEL_INST,	0
	.equ	CIR_FSEL_TAG,	0x10000000
	DEFBT	CIR_RW,	    24

;timer bits
	DEFBT	TMR_IE, 24
	DEFBT	TMR_IN, 25
	DEFBT	TMR_OV, 26

	.equ	USEC_PER_SEC, 1000000


;common trap numbers
	.equ	V_MULIPLY,  32
	.equ	V_DIVIDE,   33
	.equ	V_MULIPLU,  34
	.equ	V_DIVIDU,   35

	.equ	V_TIMER,    14

	.equ	V_INTR0,    16
	.equ	V_INTR1,    17
	.equ	V_INTR2,    18
	.equ	V_INTR3,    19
	.equ	V_TRAP0,    20
	.equ	V_TRAP1,    21

	.equ	V_SPILL,64
	.equ	V_FILL,	65

	.equ	NUM_V,	256	;# of traps and interrupts
