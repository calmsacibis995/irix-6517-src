#ifndef _NANOTHREAD_H
#define _NANOTHREAD_H

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#define _KMEMUSER
#include <sys/types.h>
#include <sys/reg.h>
#include <sys/kusharena.h>

/*      Ids of nanothreads       */
typedef short nid_t;

/*      Thread entry points are of this type.  */
typedef void ((*nanostartf_t) (void *));

typedef int (*resumefptr_t) (nid_t, kusharena_t *, nid_t);
typedef int (*runptr_t) (resumefptr_t, kusharena_t *, nid_t);
extern int _run_nid (resumefptr_t, kusharena_t *, nid_t);

/*
 *    upcall_handler_t(upcall_type, bad_nid, bad_rsaid, reserved,
 *                     arg4, arg5, arg6, arg7):
 *              Handler interface expected for OS upcalls.  arg? is
 *              set by the user through setup_upcall()
 */
typedef void ((*upcall_handler_t) (greg_t, greg_t, greg_t, greg_t,
				   greg_t, greg_t, greg_t, greg_t));

/*
 *    set_num_processors(nr, static_rsa_alloc):
 *              Set number of requested processors to nr.
 *              Allocate kernel-user shared arena if needed.
 */
int set_num_processors (nid_t, int);

/*
 *    setup_upcall(upcall_handler, arg4, arg5, arg6, arg7):
 *              Sets registers that will be passed to upcall routine
 *              specified.  First four args are used by the OS to
 *              communicate to the handler.  See sys/kusharena.h
 *              details.
 */
void setup_upcall (upcall_handler_t, greg_t, greg_t, greg_t, greg_t);

/*
 *    start_threads(init_upcall):
 *              Start 'nrequested' threads running init_upcall.
 */
int start_threads (nanostartf_t);
void kill_threads (int signal);
void wait_threads (void);

/*
 *    resume_nid(myid, kusp, resumeid): Give-up processor of the caller
 *              and resume resumeid.
 */
int resume_nid (nid_t myid, kusharena_t * kusp, nid_t resume);
/*
 * Specific paths of resume that can be invoked directly.
 */
int resume_nid_static (nid_t myid, kusharena_t * kusp, nid_t resume);
int resume_nid_dynamic (nid_t myid, kusharena_t * kusp, nid_t resume);
int heavy_resume_nid (nid_t myid, kusharena_t * kusp, nid_t resume);
/*
 *      run_nid(nidresume, kusp, newnid): A long jump to a nanothread.
 *             Also path of resume_nid that may be invoked directly.
 *             The supplied nidresume pointer is used to detect
 *             nested resumes.  If the applications uses the
 *             resume_nid supplied in this library no other resume
 *             may be used.
 */
void run_nid (resumefptr_t nidresume, kusharena_t * kusp, nid_t newnid);

/*
 *    getnid(): Return id of calling nanothread.
 */
nid_t getnid (void);
nid_t getnid_ (void);

/*
 *    block_nid(v), unblock_nid(v): Suspend or resume execution.
 */
void block_nid (nid_t);
void unblock_nid (nid_t);

__inline uint64_t
set_bit (volatile uint64_t * bitv, nid_t nid)
{
	return (__fetch_and_or (&bitv[nid / 64], 1LL < (nid % 64)) & ~(1LL << (nid % 64)));
}

__inline uint64_t
unset_bit (volatile uint64_t * bitv, nid_t nid)
{
	return (__fetch_and_and (&bitv[nid / 64], ~(1LL << (nid % 64))) & (1LL << (nid % 64)));
}

__inline nid_t
unset_any_bit (volatile uint64_t * bitv, int approx_length)
{
	register uint64_t cmask;
	register nid_t offset = 0;
	register uint8_t delta, location;
	while (approx_length > offset) {
		cmask = *bitv;
		while (cmask != 0) {
			for (location = 32, delta = 16; delta > 0; delta = delta / 2) {
				if ((cmask >> location) != 0) {
					location += delta;
				}
				else {
					location += -delta;
				}
			}
			if ((cmask >> location) == 0) {
				location--;
			}
			cmask = __fetch_and_and (bitv, ~(1LL << location));
			if (cmask & (1LL << location)) {
				return (location + offset);
			}
		}
		offset += 64;
		bitv++;
	}
	return (NULL_NID);
}

/*
 *    kusp: Pointer to kernel-user shared arena,
 *              accessible to all nanothreads/execution vehicles.
 *              Setup by set_num_processors().
 */
extern kusharena_t *kusp;

/* Don't touch the line below.  It's used for string matching. */
#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */

/*
 * Job state
 */
#define NANO_processors_set    0x1
#define NANO_upcall_set        0x2
#define NANO_signaling         0x4	/* signal to all ev's */
#define NANO_exiting           0x8

#if defined(_LANGUAGE_ASSEMBLY)
#if (_MIPS_SIM == _ABIN32)
#define RSA_SAVE_SREGS(contextp, tempreg) \
	REG_S		s0, KUS_RSA+RSA_S0(contextp); \
	REG_S		s1, KUS_RSA+RSA_S1(contextp); \
	REG_S		s2, KUS_RSA+RSA_S2(contextp); \
	REG_S		s3, KUS_RSA+RSA_S3(contextp); \
	REG_S		s4, KUS_RSA+RSA_S4(contextp); \
	REG_S		s5, KUS_RSA+RSA_S5(contextp); \
	REG_S		s6, KUS_RSA+RSA_S6(contextp); \
	REG_S		s7, KUS_RSA+RSA_S7(contextp); \
	REG_S		gp, KUS_RSA+RSA_GP(contextp); \
	REG_S		sp, KUS_RSA+RSA_SP(contextp); \
	REG_S		fp, KUS_RSA+RSA_FP(contextp); \
	REG_S		ra, KUS_RSA+RSA_EPC(contextp); \
	sdc1		fs0, KUS_RSA+RSA_FS0(contextp); \
	sdc1		fs1, KUS_RSA+RSA_FS1(contextp); \
	sdc1		fs2, KUS_RSA+RSA_FS2(contextp); \
	sdc1		fs3, KUS_RSA+RSA_FS3(contextp); \
	sdc1		fs4, KUS_RSA+RSA_FS4(contextp); \
	sdc1		fs5, KUS_RSA+RSA_FS5(contextp); \
	cfc1		tempreg, fpc_csr;
#endif
#if (_MIPS_SIM == _ABI64)
#define RSA_SAVE_SREGS(contextp, tempreg) \
	REG_S		s0, KUS_RSA+RSA_S0(contextp); \
	REG_S		s1, KUS_RSA+RSA_S1(contextp); \
	REG_S		s2, KUS_RSA+RSA_S2(contextp); \
	REG_S		s3, KUS_RSA+RSA_S3(contextp); \
	REG_S		s4, KUS_RSA+RSA_S4(contextp); \
	REG_S		s5, KUS_RSA+RSA_S5(contextp); \
	REG_S		s6, KUS_RSA+RSA_S6(contextp); \
	REG_S		s7, KUS_RSA+RSA_S7(contextp); \
	REG_S		gp, KUS_RSA+RSA_GP(contextp); \
	REG_S		sp, KUS_RSA+RSA_SP(contextp); \
	REG_S		fp, KUS_RSA+RSA_FP(contextp); \
	REG_S		ra, KUS_RSA+RSA_EPC(contextp); \
	sdc1		fs0, KUS_RSA+RSA_FS0(contextp); \
	sdc1		fs1, KUS_RSA+RSA_FS1(contextp); \
	sdc1		fs2, KUS_RSA+RSA_FS2(contextp); \
	sdc1		fs3, KUS_RSA+RSA_FS3(contextp); \
	sdc1		fs4, KUS_RSA+RSA_FS4(contextp); \
	sdc1		fs5, KUS_RSA+RSA_FS5(contextp); \
	sdc1		fs6, KUS_RSA+RSA_FS6(contextp); \
	sdc1		fs7, KUS_RSA+RSA_FS7(contextp); \
	cfc1		tempreg, fpc_csr;
#endif

#if (_MIPS_SIM == _ABIN32)
#define RSA_LOAD_SREGS(contextp, tempreg) \
	REG_L		s0, KUS_RSA+RSA_S0(contextp); \
	REG_L		s1, KUS_RSA+RSA_S1(contextp); \
	REG_L		s2, KUS_RSA+RSA_S2(contextp); \
	REG_L		s3, KUS_RSA+RSA_S3(contextp); \
	REG_L		s4, KUS_RSA+RSA_S4(contextp); \
	REG_L		s5, KUS_RSA+RSA_S5(contextp); \
	REG_L		s6, KUS_RSA+RSA_S6(contextp); \
	REG_L		s7, KUS_RSA+RSA_S7(contextp); \
	REG_L		gp, KUS_RSA+RSA_GP(contextp); \
	REG_L		sp, KUS_RSA+RSA_SP(contextp); \
	REG_L		fp, KUS_RSA+RSA_FP(contextp); \
	REG_L		ra, KUS_RSA+RSA_EPC(contextp); \
	ldc1		fs0, KUS_RSA+RSA_FS0(contextp); \
	ldc1		fs1, KUS_RSA+RSA_FS1(contextp); \
	ldc1		fs2, KUS_RSA+RSA_FS2(contextp); \
	ldc1		fs3, KUS_RSA+RSA_FS3(contextp); \
	ldc1		fs4, KUS_RSA+RSA_FS4(contextp); \
	ldc1		fs5, KUS_RSA+RSA_FS5(contextp); \
	lw		tempreg, KUS_RSA+RSA_FPC_CSR(contextp);
#endif
#if (_MIPS_SIM == _ABI64)
#define RSA_LOAD_SREGS(contextp, tempreg) \
	REG_L		s0, KUS_RSA+RSA_S0(contextp); \
	REG_L		s1, KUS_RSA+RSA_S1(contextp); \
	REG_L		s2, KUS_RSA+RSA_S2(contextp); \
	REG_L		s3, KUS_RSA+RSA_S3(contextp); \
	REG_L		s4, KUS_RSA+RSA_S4(contextp); \
	REG_L		s5, KUS_RSA+RSA_S5(contextp); \
	REG_L		s6, KUS_RSA+RSA_S6(contextp); \
	REG_L		s7, KUS_RSA+RSA_S7(contextp); \
	REG_L		gp, KUS_RSA+RSA_GP(contextp); \
	REG_L		sp, KUS_RSA+RSA_SP(contextp); \
	REG_L		fp, KUS_RSA+RSA_FP(contextp); \
	REG_L		ra, KUS_RSA+RSA_EPC(contextp); \
	ldc1		fs0, KUS_RSA+RSA_FS0(contextp); \
	ldc1		fs1, KUS_RSA+RSA_FS1(contextp); \
	ldc1		fs2, KUS_RSA+RSA_FS2(contextp); \
	ldc1		fs3, KUS_RSA+RSA_FS3(contextp); \
	ldc1		fs4, KUS_RSA+RSA_FS4(contextp); \
	ldc1		fs5, KUS_RSA+RSA_FS5(contextp); \
	ldc1		fs6, KUS_RSA+RSA_FS6(contextp); \
	ldc1		fs7, KUS_RSA+RSA_FS7(contextp); \
	lw		tempreg, KUS_RSA+RSA_FPC_CSR(contextp);
#endif
/* Don't touch the line below.  It's used for string matching. */
#endif /* defined(_LANGUAGE_ASSEMBLY) */

#endif /* _NANOTHREAD_H */
