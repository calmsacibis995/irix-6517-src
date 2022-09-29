#ident "$Revision: 1.4 $"

#include <arcs/spb.h>
#include <arcs/pvector.h>
#include <libsc.h>

/* SN0 specific routines.
 */

char *
kl_inv_find()
{
        /*
         * Older UP proms don't have this entry.  Since they're
         * all UP, we can just return 0 for cpuid.
         */
        if (SPB->PTVLength <= (ULONG)PTVOffset(kl_inv_find)) {
                return(0);
        }
        return(__PTV->kl_inv_find());
}

int
kl_hinv(int argc, char **argv)
{
        /*
         * Older UP proms don't have this entry.  Since they're
         * all UP, we can just return 0 for cpuid.
         */
        if (SPB->PTVLength <= (ULONG)PTVOffset(kl_hinv)) {
                return(0);
        }
        return(__PTV->kl_hinv(argc, argv));
}

int
kl_get_num_cpus()
{
        /*
         * Older UP proms don't have this entry.  Since they're
         * all UP, we can just return 0 for cpuid.
         */
        if (SPB->PTVLength <= (ULONG)PTVOffset(kl_get_num_cpus)) {
                return(0);
        }
        return(__PTV->kl_get_num_cpus());
}

int
sn0_getcpuid()
{
        if (SPB->PTVLength <= (ULONG)PTVOffset(sn0_getcpuid)) {
                return(-1);
        }
        return(__PTV->sn0_getcpuid());
}

