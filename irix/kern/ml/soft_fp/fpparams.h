/* ====================================================================
 * ====================================================================
 *
 * Module: fppparams.h
 * $Revision: 1.14 $
 * $Date: 1997/09/26 20:45:24 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/fpparams.h,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	various constants used by the softfp emulation package
 *
 * ====================================================================
 * ====================================================================
 */

#ifndef __FPPARAMS_H
#define	__FPPARAMS_H

#ifdef __FPT
#include <inttypes.h>
#endif
#ifdef _KERNEL
#include <sys/types.h>
#include <sys/uthread.h>
#endif

/* define __FPT to compile for the fpt test package */

#ifdef __FPT

typedef union fpval_u {
	uint64_t	ll;
	uint32_t	i[2];
} fpval_t;

struct fpglobs {

        void		*p_excpt_fr_ptr;
	int32_t		p_fp_enables;
        int32_t		p_fp_csr;
        char		p_fp_csr_rm;
        char		p_softfp_flags;
        char		p_rdnum;
        fpval_t         p_fpval;
};

#endif	/* __FPT */

/* if the madd instruction is fused as it is on tfp,
 * #define FUSED_MADD
 */

#ifdef TFP

#define FUSED_MADD	1

#endif

#define	SINGLE	0
#define	DOUBLE	1

typedef int32_t	(*PFI)(uint64_t *);
typedef int64_t	(*PFLL)(uint32_t, int32_t, uint64_t, uint64_t);
typedef void     (*PFV)(uint32_t, int32_t, uint64_t, uint64_t);



#define	TRUE	1
#define	FALSE	0
#define	SIGFPE	8

#define RT_SHIFT	16
#define	RT_MASK		0x1f
#define	RT_FPRMASK	0x1f
#define RS_SHIFT	11
#define	RS_MASK		0x1f
#define	RS_FPRMASK	0x1f
#define RD_SHIFT	6
#define	RD_MASK		0x1f
#define	RD_FPRMASK	0x1f
#define RR_SHIFT	21
#define RR_FPRMASK	0x1f

#define	OPCODE_SHIFT	26
#define	OPCODE_25_SHIFT	25
#define	OPCODE_C1	0x11
#define	OPCODE_C1X	0x13
#define	OPCODE_C1_25	0x23

#define COP1		0x11
#define COP1X		0x13

#define C1_FMT_SHIFT	21
#define	C1_FMT_MASK	0xf
#define C1_FMT_SINGLE	0
#define C1_FMT_DOUBLE	1
#define C1_FMT_EXTENDED	2
#define C1_FMT_QUAD	3
#define C1_FMT_WORD	4
#define C1_FMT_LONG	5
#define C1_FMT_MAX	5

#define C1_FUNC_MASK	0x3f
#define C1_FUNC_ADD	0
#define C1_FUNC_SUB	1
#define C1_FUNC_MUL	2
#define C1_FUNC_DIV	3
#define C1_FUNC_SQRT	4
#define C1_FUNC_ABS	5
#define C1_FUNC_MOV	6
#define C1_FUNC_NEG	7
#define C1_FUNC_ROUNDL	8
#define C1_FUNC_TRUNCL	9
#define C1_FUNC_CEILL	10
#define C1_FUNC_FLOORL	11
#define C1_FUNC_ROUND	12
#define C1_FUNC_ROUNDW	12
#define C1_FUNC_TRUNCW	13
#define C1_FUNC_CEILW	14
#define C1_FUNC_FLOOR	15
#define C1_FUNC_FLOORW	15
#define C1_FUNC_RECIP	21
#define C1_FUNC_RSQRT	22
#define C1_FUNC_CVTS	32
#define C1_FUNC_CVTD	33
#define C1_FUNC_CVTW	36
#define C1_FUNC_CVTL	37
#define C1_FUNC_1stCMP	48
#define C1_FUNC_lastCMP	63

#define C1X_FMT_MASK	0x7
#define C1X_FUNC_SHIFT	3
#define C1X_FUNC_MASK	0x7

#define C1X_FUNC_MADD_S		32
#define C1X_FUNC_MADD_D		33
#define C1X_FUNC_MSUB_S		40
#define C1X_FUNC_MSUB_D		41
#define C1X_FUNC_NMADD_S	48
#define C1X_FUNC_NMADD_D	49
#define C1X_FUNC_NMSUB_S	56
#define C1X_FUNC_NMSUB_D	57

#define	SEXP_SHIFT	23
#define	SEXP_MASK	0xff
#define	SEXP_BIAS	127
#define	SIMP_1BIT	0x0000000000800000ull
#define	SSIGN_SHIFT	31
#define	SFRAC_MASK	0x7fffffll
#define	SSIGNBIT	0x80000000ll
#define	SEXP_NAN	0xff
#define	SEXP_INF	0xff
#define	SEXP_MAX	127
#define	SEXP_MIN	-126
#define	SEXP_OU_ADJ	192
#define	SSNANBIT_MASK	0x400000ull
#define SMANTWIDTH	23
#define	SMAXDENORMAL	0x007fffffull
#define	SMINNORMAL	0x00800000ull
#define	SINFINITY	0x7f800000u
#define	SQUIETNAN	0x7fbfffffll
#define	SFRAC_BITS	23
#define MSTWOP31	0xcf000000ll
#define MSTWOP63	0xdf000000ll
#define STWOP0		0x3f800000ll

#define	DLOW24_MASK	0xffffffull
#define	DLOW53_MASK	0x1fffffffffffffull
#define	DHIGH1_MASK	0x1000000000000000ull
#define	DHIGH4_MASK	0xf000000000000000ull
#define	DHIGH8_MASK	0xff00000000000000ull
#define	DHIGH11_MASK	0xffe0000000000000ull
#define	DHIGH16_MASK	0xffff000000000000ull
#define	DHIGH24_MASK	0xffffff0000000000ull
#define	DHIGH32_MASK	0xffffffff00000000ull
#define	DHIGH36_MASK	0xfffffffff0000000ull
#define	DHIGH40_MASK	0xffffffffff000000ull

#define	DMAXDENORMAL	0x000fffffffffffffull
#define	DMINNORMAL	0x0010000000000000ull
#define	DINFINITY	0x7ff0000000000000ll
#define MDINFINITY	0xfff0000000000000ll
#define DMANTWIDTH	52
#define	MAXDBL		0x7fefffffffffffffull
#define MMAXDBL		0xffefffffffffffffull
#define MDTWOP31	0xc1e0000000000000ll
#define MDTWOP63	0xc3e0000000000000ll
#define	DTWOP0		0x3ff0000000000000ll

#define MSINFINITY	0xff800000u
#define	MAXSNGL		0x7f7fffffu
#define MMAXSNGL	0xff7fffffu

#define WORD_MIN        0xffffffff80000000ll
#define WORD_MAX        0x000000007fffffffll
#define UWORD_MIN	0x80000000u
#define UWORD_MAX	0x7fffffffu

#define LONGLONG_MIN	0x8000000000000000ll
#define LONGLONG_MAX	0x7fffffffffffffffll
#define ULONGLONG_MAX	0xffffffffffffffffull

#define	DEXP_SHIFT	52
#define	DEXP_BIAS	0x3ff
#define	DEXP_MASK	0x7ff
#define	DSIGN_SHIFT	63
#define	DSIGNBIT	0x8000000000000000ll
#define	DFRAC_MASK	0xfffffffffffffll
#define	DEXP_MAX	1023
#define	DEXP_MIN	-1022
#define	DEXP_INF	0x7ff
#define	DEXP_NAN	0x7ff
#define	DEXP_OU_ADJ	1536
#define	DSNANBIT_MASK	0x8000000000000ull
#define	DFRAC_BITS	52
#define	DQUIETNAN	0x7ff7ffffffffffffll
#define	DIMP_1BIT	0x0010000000000000ull
#define	DBIT47		0x0000800000000000ull

#define	GUARD_BIT	0x8000000000000000ull
#define	STICKY_BIT	0x4000000000000000ull
#define	STICKY_MASK	0x7fffffffffffffffull

/*
 * These constants refer to fields in the floating-point status and control
 * register.
 */

#define	CSR_CBITSHIFT	23
#define	CSR_CBITMASK	0x1u
#define	CSR_CBITSET	0x00800000u
#define	CSR_CBITCLEAR	0xff7fffffu

#define	CSR_FSBITSET	0x01000000u
#define	UNDERFLOW_CAUSE	0x00002000u
#define	UNDERFLOW_FLAG	0x00000008u

#define	CSR_EXCEPT	0x0003f000u
#define	UNIMP_EXC	0x00020000u
#define	INVALID_EXC	0x00010040u
#define	DIVIDE0_EXC	0x00008020u
#define	OVERFLOW_EXC	0x00004010u
#define	UNDERFLOW_EXC	0x00002008u
#define	INEXACT_EXC	0x00001004u

#define CSR_ENABLE		0x00000f80u
#define	INVALID_ENABLE		0x00000800u
#define	DIVIDE0_ENABLE		0x00000400u
#define	OVERFLOW_ENABLE		0x00000200u
#define	UNDERFLOW_ENABLE	0x00000100u
#define	INEXACT_ENABLE		0x00000080u

#define	CSR_RM_MASK	0x3u
#define	CSR_RM_RN	0u
#define	CSR_RM_RZ	1u
#define	CSR_RM_RPI	2u
#define	CSR_RM_RMI	3u

#define CC_MASK		0x7
#define CC_SHIFT	8

#define COND_MASK	0xf
#define COND_UN_MASK	0x1
#define COND_EQ_MASK	0x2
#define COND_LT_MASK	0x4
#define COND_IN_MASK	0x8

/* defines for p_softfp_flags */

#define	FLUSH_INPUT_DENORMS	0x1
#define	FLUSH_OUTPUT_DENORMS	0x2
#define	STORE_ON_ERROR		0x4
#define	FP_ERROR		0x8
#define	SIG_POSTED		0x10
#define	EXT_REG_MODE		0x20

#ifdef _FPTRACE

/* defines for tracing */

#define ABS_SD		0
#define ADD_SD		1
#define CEILL_SD	2
#define CEILW_SD	3
#define COMP_SD		4
#define CVTD_S		5
#define CVTL_SD		6
#define CVTSD_L		7
#define CVTSD_W		8
#define CVTS_D		9
#define CVTW_SD		10
#define DIV_SD		11
#define FLOORL_SD	12
#define FLOORW_SD	13
#define MADD_SD		14
#define MOV_SD		15
#define MSUB_SD		16
#define MUL_SD		17
#define NEG_SD		18
#define NMADD_SD	19
#define NMSUB_SD	20
#define RECIP_SD	21
#define ROUNDL_SD	22
#define ROUNDW_SD	23
#define RSQRT_SD	24
#define SQRT_SD		25
#define SUB_SD		26
#define TRUNCL_SD	27
#define TRUNCW_SD	28

#endif	/* _FPTRACE */

/* macros for FPSTORE_S, FPSTORE_D */

#ifdef _KERNEL

#define PCB_STORE( val ) PCB( pcb_fp_rounded_result ) = val

#else

#define PCB_STORE( val ) pcb_store( val )

#endif

#ifdef __FPT

#define	FPSTORE_S( val ) \
{ \
	fpstore_s(val, curuthread->ut_rdnum); \
	curuthread->p_fpval.ll = val; \
}

#define	FPSTORE_D( val ) \
{ \
	fpstore_d(val, curuthread->ut_rdnum, GET_SOFTFP_FLAGS() & EXT_REG_MODE); \
	curuthread->p_fpval.ll = val; \
}

#else

#define	FPSTORE_S( val ) \
	fpstore_s(val, curuthread->ut_rdnum, GET_SOFTFP_FLAGS() & EXT_REG_MODE)

#define	FPSTORE_D( val ) \
	fpstore_d(val, curuthread->ut_rdnum, GET_SOFTFP_FLAGS() & EXT_REG_MODE)

#endif

#define	SET_FP_CSR_RM(rm)		(curuthread->ut_fp_csr_rm = (rm))
#define GET_FP_CSR_RM()			(curuthread->ut_fp_csr_rm)

#define	SET_LOCAL_CSR(csr)		(curuthread->ut_fp_csr = (csr))
#define GET_LOCAL_CSR()			(curuthread->ut_fp_csr)

#define	SET_SOFTFP_FLAGS(flags)		(curuthread->ut_softfp_flags = (flags))
#define GET_SOFTFP_FLAGS()		(curuthread->ut_softfp_flags)

#endif	/* __FPPARAMS_H */
