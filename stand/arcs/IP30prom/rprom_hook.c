#include <sys/sbd.h>
#include <genpda.h>
#include <libsc.h>
#include <libsk.h>

/*
 * system exception handler setup for RPROM
 * simpler version of IP30 hook.c
 */

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

extern gen_pda_t        gen_pda_tab[MAXCPU];

/*
 * _hook_exceptions -- point exception vector to prom exception handler
 */
void
_hook_exceptions(void)
{
	extern int rprom_exceptutlb[];
	extern int rprom_exceptnorm[];
	extern int rprom_exceptxut[];
	extern int rprom_exceptecc[];
	register int vid;

#if XXX_RPROM
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
#endif /*XXX_RPROM*/

	/*
	 * each processor now has unique 'nofault' and 'notfirst' 
	 * "globals", all of which must be cleared
	 */
	for (vid = 0; vid < MAXCPU; vid++) {
 		gen_pda_tab[vid].gdata.pda_nofault = 0;
		gen_pda_tab[vid].gdata.notfirst = 0;
		gen_pda_tab[vid].gdata.first_epc = 0;
	}

#if DEBUG
	printf( "Copying exception handlers down\n"
		"%x<-%x\n%x<-%x\n%x<-%x\n%x<-%x\n",
		    UT_VEC,  rprom_exceptutlb,
		    XUT_VEC, rprom_exceptxut,
		    ECC_VEC, rprom_exceptecc,
		    E_VEC,   rprom_exceptnorm
	);
#endif
	*(struct vector *)UT_VEC =  *(struct vector *)rprom_exceptutlb;
	*(struct vector *)XUT_VEC = *(struct vector *)rprom_exceptxut;
	*(struct vector *)ECC_VEC = *(struct vector *)rprom_exceptecc;
	*(struct vector *)E_VEC =   *(struct vector *)rprom_exceptnorm;
#if DEBUG
	printf("clear_cache(%x,%x)\n",
	    UT_VEC, E_VEC - UT_VEC + sizeof(struct vector));
#endif

	clear_cache((void*)UT_VEC, E_VEC - UT_VEC + sizeof(struct vector));

#if XXX_RPROM
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
#endif /*XXX_RPROM*/
#if DEBUG
	printf("exiting _hook_exceptions()\n");
#endif
}

