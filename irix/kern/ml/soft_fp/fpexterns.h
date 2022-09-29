/* ====================================================================
 * ====================================================================
 *
 * Module: fpexterns.h
 * $Revision: 1.13 $
 * $Date: 1997/09/26 20:45:23 $
 * $Author: sfc $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/fpexterns.h,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:  various externs used by the softfp emulation package
 *
 * ====================================================================
 * ====================================================================
 */

#ifndef	__FPEXTERNS_H
#define	__FPEXTERNS_H

/* define __FPT to compile for the fpt test package */

#ifdef	__FPT

extern	struct fpglobs	curp;
#define curuthread	(&curp)
#endif

#ifdef _FPTRACE

extern fp_trace1_w( int32_t precision, int64_t rs, int32_t op );

extern fp_trace1_l( int32_t precision, int64_t rs, int32_t op );

extern fp_trace1_sd( int32_t precision, int64_t rs, int32_t op );

extern fp_trace2_sd( int32_t precision, int64_t rs, int64_t rt, int32_t op );

extern fp_trace3_sd( int32_t precision, int64_t rs, int64_t rt,
			int64_t rr, int32_t op );

#endif

extern const int8_t underflow_tab[4][2][2][2][2];
extern const int8_t round_bit[4][2][2][2][2];

extern const int32_t EXP_BIAS[2];
extern const int32_t EXP_INF[2];
extern const int32_t EXP_MASK[2];
extern const int32_t EXP_MIN[2];
extern const int32_t EXP_NAN[2];
extern const int32_t EXP_SHIFT[2];
extern const int32_t MANTWIDTH[2];
extern const int32_t SIGN_SHIFT[2];

extern const int64_t FRAC_MASK[2];
extern const int64_t IMP_1BIT[2];
extern const int64_t INFINITY[2];
extern const int64_t MTWOP63[2];
extern const int64_t QUIETNAN[2];
extern const int64_t SIGNBIT[2];
extern const int64_t TWOP0[2];
extern const uint64_t SNANBIT_MASK[2];

extern const PFV round[2];
extern const PFLL maddrnd[2];
extern const PFI renorm[2];

/* Utilities */

extern void	fp_init_glob(void *, int32_t, int32_t, int32_t, int32_t,
			     int32_t);
extern uint64_t	mul64(uint64_t, uint64_t, uint64_t *);

extern void	breakout_sd(int32_t, int64_t *, uint32_t *,
				int32_t *, uint64_t *);
extern int32_t	breakout_and_test_sd(int32_t,
				int64_t *, uint32_t *, int32_t *, uint64_t *);
extern int32_t	breakout_and_test2_sd(int32_t,
				int64_t *, uint32_t *, int32_t *, uint64_t *,
				int64_t *, uint32_t *, int32_t *, uint64_t *);
extern int32_t	breakout_and_test3_sd(int32_t,
				int64_t *, uint32_t *, int32_t *, uint64_t *,
				int64_t *, uint32_t *, int32_t *, uint64_t *,
				int64_t *, uint32_t *, int32_t *, uint64_t *);

extern void	fpstore_s(int64_t, int, int);
extern void	fpstore_d(int64_t, int, int);
extern void	fpunit_fpstore_s(int64_t, int);
extern void	fpunit_fpstore_d(int64_t, int, int);

extern void	fpstore_sd(int32_t, int64_t);

extern void	pcb_store(int64_t);

#ifdef _KERNEL
extern int64_t	fpload_s(int32_t, int);
extern int64_t	fpload_d(int32_t, int);
extern int64_t	fpunit_fpload_s(int32_t);
extern int64_t	fpunit_fpload_d(int32_t, int);
extern int32_t	load_fpcsr(void);
#endif


extern void	add(int32_t, uint32_t, int32_t, uint64_t, uint32_t, int32_t,
			uint64_t);
extern void	madd(int32_t, uint32_t, int32_t, uint64_t, uint32_t, int32_t,
			uint64_t, uint32_t, int32_t, uint64_t);
extern int64_t	maddrnd_s(uint32_t, int32_t, uint64_t, uint64_t);
extern int64_t	maddrnd_d(uint32_t, int32_t, uint64_t, uint64_t);
extern int32_t	renorm_d(uint64_t *);
extern int32_t	renorm_s(uint64_t *);
extern void	round_w(uint32_t, int64_t, uint64_t);
extern void	round_l(uint32_t, int64_t, uint64_t);


extern void	round_d(uint32_t, int32_t, uint64_t, uint64_t);
extern void	round_s(uint32_t, int32_t, uint64_t, uint64_t);
extern int32_t	screen_nan_sd(int32_t, int64_t, int32_t, uint64_t );

extern void	post_sig(int32_t);
extern void	store_fpcsr(void);

/* emulation functions */

extern void	_abs_sd(int32_t, int64_t);
extern void	_add_sd(int32_t, int64_t, int64_t);
extern void	_cvtd_s(int64_t);
extern void	_cvts_d(int64_t);
extern void	_cvtw_sd(int32_t, int64_t);
extern void	_cvtl_sd(int32_t, int64_t);
extern void	_cvtsd_l(int32_t, int64_t);
extern void	_cvtsd_w(int32_t, int64_t);
extern void	_comp_sd(int32_t, int64_t, int64_t, int32_t, int32_t);
extern void	_div_sd(int32_t, int64_t, int64_t);
extern void	_madd_sd(int32_t, int64_t, int64_t, int64_t);
extern void	_msub_sd(int32_t, int64_t, int64_t, int64_t);
extern void	_mul_sd(int32_t, int64_t, int64_t);
extern void	_neg_sd(int32_t, int64_t);
extern void	_nmadd_sd(int32_t, int64_t, int64_t, int64_t);
extern void	_nmsub_sd(int32_t, int64_t, int64_t, int64_t);
extern void	_rsqrt_sd(int32_t, int64_t);
extern void	_sqrt_sd(int32_t, int64_t);
extern void	_sub_sd(int32_t, int64_t, int64_t);

#endif	/* __FPEXTERNS_H */

