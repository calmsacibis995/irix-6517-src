/*
 * tfphook.c - save/restore exception vectors for TFP
 *
 * These support a four deep save/restore stack
 *
 * $Revision: 1.10 $
 *
 */
#include <sys/tfp.h>
#include <arcs/spb.h>
#include <libsc.h>
#include <libsk.h>
#include <genpda.h>

extern gen_pda_t	gen_pda_tab[MAXCPU];

extern __psint_t	tfp_get_trapbase(void);
extern void	 	tfp_set_trapbase(__psint_t);

#define private		gen_pda_tab[_cpuid()].gdata

#define saved		private.saved
#define save_trapbase	private.save_trapbase
#define save_gevector	private.save_gevector
#define save_utlbvector	private.save_utlbvector

/*
 * _save_exceptions -- save current exception vectors
 */
void _save_exceptions(void)
{
	if (saved > 3)
		printf("too many _save_exceptions\n");
#ifdef DEBUG
	printf("save with index %d\n", saved);
#endif

	save_trapbase[saved] = tfp_get_trapbase();
	save_gevector[saved] = *(volatile __psint_t *)SPB_GEVECTOR;
	save_utlbvector[saved] = *(volatile __psint_t *)SPB_UTLBVECTOR;

	saved += 1;
}

/*
 * _restore_exceptions -- restore exception vectors from last save
 */
void _restore_exceptions(void)
{
	if (!saved) {
#ifdef DEBUG
		printf("not enough _save_exceptions\n");
#endif
		return;
	}
	saved -= 1;

	tfp_set_trapbase(save_trapbase[saved]);
	*(volatile __psint_t *)SPB_GEVECTOR = save_gevector[saved];
	*(volatile __psint_t *)SPB_UTLBVECTOR = save_utlbvector[saved];
}

/*
 * _hook_exceptions -- point exception vector to prom exception handler
 */
void _hook_exceptions(void)
{
	extern __psint_t _trap_table[];
	register int vid;

	/*
	 * each processor now has unique 'nofault' and 'notfirst' 
	 * "globals", all of which must be cleared
	 */
	for (vid = 0; vid < MAXCPU; vid++) {
 		gen_pda_tab[vid].gdata.pda_nofault = 0;
		gen_pda_tab[vid].gdata.notfirst = 0;
		gen_pda_tab[vid].gdata.first_epc = 0;
	}

	tfp_set_trapbase((__psint_t)&_trap_table);

	*(volatile __psint_t *)SPB_GEVECTOR = (__psint_t)exceptnorm;
	*(volatile __psint_t *)SPB_UTLBVECTOR = (__psint_t)exceptutlb;
}

void
_default_exceptions(void)
{
	_hook_exceptions();
}


/*
 * _inst_vec -- if exception vectors have been overwritten, return
 * 		the contents of the previous vector at the given address
 *
 * Doesn't make sense for TFP. I don't think anyone actually calls this
 * routine. Symmon has its own version. Let's see...
 */
/*ARGSUSED*/
inst_t
_inst_vec(__psunsigned_t addr)
{
    /*
    printf("_inst_vec called...\n");
    */
    return (inst_t)-1;
}
