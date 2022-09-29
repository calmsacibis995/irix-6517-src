#ident	"lib/libsc/cmd/memlist_cmd.c: $Revision: 1.8 $"

/*
 * memlist_cmd.c - print out the system list of memory descriptors
 */

#include <arcs/hinv.h>
#include <libsc.h>

extern void	mem_list(void);

/*ARGSUSED*/
int
memlist(int argc, char **argv)
{
	mem_list();
	return(0);
}
