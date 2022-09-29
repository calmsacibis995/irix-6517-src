#ident	"lib/libsc/lib/mp.c:  $Revision: 1.16 $"
#include <libsc.h>
#include <libsc_internal.h>
#include <arcs/hinv.h>

/*
 * mp.c - routines needed for MP support
 */


/* In order to avoid problems with stdio before things are initialized,
   use a simple temporary buffer. */

/* Note this scheme works on IO4/IO6 since initialized data is really in RAM
 * and doesn't work on IP20/22/24/26/28 since initialized data is really,
 * in ROM, while IP30/32, the data is in Flash PROM.  
 * This is ok since we don't change it in _init_libsc_private() and will
 * continue to work until there is more than 1 processor on a system with
 * ROM resident initialized data.  Then a scheme to pre-init this variable
 * via a c statement must be used (like _pre_init_libsc_private() below) and
 * which should (obviously) be called before any of the printf routines (in
 * stdio.c) that use this libsc_private pointer.
 */

/* NOTE: _libsc_private ptr to libsc_private_t tmpbuf 
 *       being discussed above have been move to privstub3.c 
 */


static int
_count_CPUs(COMPONENT *p)
{
	int count;
	COMPONENT *child;

	if ((p->Class == ProcessorClass) && (p->Type == CPU)) {
		count = 1;
	} else {
		count = 0;
	}

	child = GetChild(p);

	while (child != NULL) {
		count += _count_CPUs(child);
		child = GetPeer(child);
	}

	return(count);
}

/*
 * Walk ARCS configuration tree in order to determine how many 
 * cpus are available on this system.
 */
int
_get_numcpus(void)
{
	if (GetChild(NULL) == NULL) /* It must be a HWG based system */
		return (kl_get_num_cpus()) ;

	return(_count_CPUs(GetChild(NULL)));
}

void
_init_libsc_private(void)
{
	int numcpus = _get_numcpus();

#if DEBUG
	printf("%d cpu(s) found.\n", numcpus);
#endif

	if (numcpus > 1)
		_libsc_private=malloc((size_t)numcpus*sizeof(libsc_private_t));

	if (!_libsc_private)
		panic("libsc private area neither init'd nor malloc'd.");
}
