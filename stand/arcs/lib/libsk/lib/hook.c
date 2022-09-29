#if !TFP
/*
 * hook.c - save/restore exception vectors
 *
 *
 * These support a four deep save/restore stack
 *
 * $Revision: 1.26 $
 *
 */
#include <sys/sbd.h>
#include <genpda.h>
#include <arcs/spb.h>
#include <libsc.h>
#include <libsk.h>

/* can have at most 32 instructions */
struct vector {
	inst_t	inst1;
	inst_t	inst2;
	inst_t	inst3;
	inst_t	inst4;
	inst_t	inst5;
	inst_t	inst6;
	inst_t	inst7;
	inst_t	inst8;
	inst_t	inst9;
	inst_t	inst10;
	inst_t	inst11;
	inst_t	inst12;
	inst_t	inst13;
	inst_t	inst14;
	inst_t	inst15;
	inst_t	inst16;
#if	EVEREST || _MIPS_SIM == _ABI64
	inst_t	inst17;
	inst_t	inst18;
	inst_t	inst19;
	inst_t	inst20;
#endif
#if _MIPS_SIM == _ABI64		/* 64 bit addresses require more addr loading */
	/* XXX -- this can be decreased if the AT save is moved back to
 	 * XXX -- the second level handler.
	 */
	inst_t	instr21; /*	Extra instructions needed for 64 bit	  */
	inst_t	instr22; /*	address loading/manipulation.		  */
	inst_t	instr23;
	inst_t	instr24;
	inst_t	instr25;
	inst_t	instr26;
	inst_t	instr27;
	inst_t	instr28;
	inst_t	instr29;
	inst_t	instr30;
	inst_t	instr31;
	inst_t	instr32;
#endif
};

void *k1_exceptecc;

static struct vector save_utlb[4];
static struct vector save_norm[4];
static struct vector save_xut[4];
static struct vector save_ecc[4];
static __psint_t save_gevector[4];
static __psint_t save_utlbvector[4];
static int saved;

/*
 * _save_exceptions -- save current exception vectors
 */
void
_save_exceptions(void)
{
	if (saved > 3)
		printf("too many _save_exceptions\n");
#ifdef DEBUG
	printf("save with index %d\n", saved);
#endif

	save_utlb[saved] = *(struct vector *)UT_VEC;
	save_norm[saved] = *(struct vector *)E_VEC;
	save_xut[saved] = *(struct vector *)XUT_VEC;
	save_ecc[saved] = *(struct vector *)ECC_VEC;

	clear_cache((void*)UT_VEC, E_VEC - UT_VEC + sizeof(struct vector));

	save_gevector[saved] = *(volatile __psint_t *)SPB_GEVECTOR;
	save_utlbvector[saved] = *(volatile __psint_t *)SPB_UTLBVECTOR;

	clear_cache((void*)SPB_GEVECTOR, sizeof(__psint_t) * 2);

	saved += 1;
}

/*
 * _restore_exceptions -- restore exception vectors from last save
 */
void
_restore_exceptions(void)
{
	if (!saved) {
#ifdef DEBUG
		printf("not enough _save_exceptions\n");
#endif
		return;
	}
	saved -= 1;

#ifdef DEBUG
	printf("restore with index %d\n",saved);
#endif

	*(struct vector *)UT_VEC = save_utlb[saved];
	*(struct vector *)E_VEC = save_norm[saved];
	*(struct vector *)XUT_VEC = save_xut[saved];
	*(struct vector *)ECC_VEC = save_ecc[saved];

	clear_cache((void*)UT_VEC, E_VEC - UT_VEC + sizeof(struct vector));

	*(volatile __psint_t *)SPB_GEVECTOR = (__psint_t)save_gevector[saved];
	*(volatile __psint_t *)SPB_UTLBVECTOR =
					(__psint_t)save_utlbvector[saved];

	clear_cache((void*)SPB_GEVECTOR, sizeof(__psint_t) * 2);
}

/*
 * _hook_exceptions -- point exception vector to prom exception handler
 */
void
_hook_exceptions(void)
{
	extern int _j_exceptutlb[];
	extern int _j_exceptnorm[];
	extern int _j_exceptxut[];
	extern int _j_exceptecc[];
	extern int exceptecc[];
	register int vid;

	/*
	 * do it here such that we don't need as many instructions in
	 * _j_exceptecc()
	 */
#if _MIPS_SIM == _ABI64
	if ((__psunsigned_t)exceptecc >= COMPAT_K0BASE)
		k1_exceptecc =
			(void *)((__psunsigned_t)exceptecc | COMPAT_K1BASE);
	else
#endif	/* K64U64 */
		k1_exceptecc = (void *)K0_TO_K1(exceptecc);

	/*
	 * each processor now has unique 'nofault' and 'notfirst' 
	 * "globals", all of which must be cleared
	 */
	for (vid = 0; vid < MAXCPU; vid++) {
#ifdef SN
		if (!get_cpuinfo(vid)) continue;
#endif
 		GEN_PDA_TAB(vid).gdata.pda_nofault = 0;
		GEN_PDA_TAB(vid).gdata.notfirst = 0;
		GEN_PDA_TAB(vid).gdata.first_epc = 0;
	}

#if DEBUG
	printf( "Copying exception handlers down\n"
		"%x<-%x\n%x<-%x\n%x<-%x\n%x<-%x\n",
		    UT_VEC,  _j_exceptutlb,
		    XUT_VEC, _j_exceptxut,
		    ECC_VEC, _j_exceptecc,
		    E_VEC,   _j_exceptnorm
	);
#endif
	*(struct vector *)UT_VEC = *(struct vector *)_j_exceptutlb;
	*(struct vector *)XUT_VEC = *(struct vector *)_j_exceptxut;
	*(struct vector *)ECC_VEC = *(struct vector *)_j_exceptecc;
	*(struct vector *)E_VEC = *(struct vector *)_j_exceptnorm;
#if DEBUG
	printf("clear_cache(%x,%x)\n",
	    UT_VEC, E_VEC - UT_VEC + sizeof(struct vector));
#endif

	clear_cache((void*)UT_VEC, E_VEC - UT_VEC + sizeof(struct vector));

#if DEBUG
	printf("%x<-%x\n%x<-%x\n",
	    SPB_GEVECTOR,   exceptnorm, SPB_UTLBVECTOR, exceptutlb
	);
#endif
	*(volatile __psint_t *)SPB_GEVECTOR = (__psint_t)exceptnorm;
	*(volatile __psint_t *)SPB_UTLBVECTOR = (__psint_t)exceptutlb;

#if DEBUG
	printf("clear_cache(%x,%x)\n",
	    (void*)SPB_GEVECTOR, sizeof(struct vector) * 2);
#endif
	clear_cache((void*)SPB_GEVECTOR, sizeof(struct vector) * 2);
#if DEBUG
	printf("exiting _hook_exceptions()\n");
#endif
}


/*
 * _inst_vec -- if exception vectors have been overwritten, return
 * 		the contents of the previous vector at the given address
 */
inst_t
_inst_vec(__psunsigned_t addr)
{
    addr = K1_TO_K0(addr);
    if (saved) {
        if ((addr >= (__psunsigned_t)UT_VEC)
            && (addr < (__psunsigned_t)UT_VEC+sizeof(struct vector)))
                return ((inst_t *)&save_utlb[saved-1])
					[(addr-(__psunsigned_t)UT_VEC)>>2];
        if ((addr >= (__psunsigned_t)E_VEC)
            && (addr < (__psunsigned_t)E_VEC+sizeof(struct vector)))
                return ((inst_t *)&save_norm[saved-1])
					[(addr-(__psunsigned_t)E_VEC)>>2];
	if ((addr >= (__psunsigned_t)XUT_VEC)
	    && (addr < (__psunsigned_t)XUT_VEC+sizeof(struct vector)))
		return ((inst_t *)&save_xut[saved-1])
					[(addr-(__psunsigned_t)XUT_VEC)>>2];
	if ((addr >= (__psunsigned_t)ECC_VEC)
	    && (addr < (__psunsigned_t)ECC_VEC+sizeof(struct vector)))
		return ((inst_t *)&save_ecc[saved-1])
					[(addr-(__psunsigned_t)ECC_VEC)>>2];
    }
    return (inst_t)-1;
}
#endif /* !TFP */
