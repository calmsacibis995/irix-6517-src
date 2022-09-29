/*
 * general memory modification commands
 */

#ident	"$Revision: 1.1 $"

#include <libsc.h>
#include <libsk.h>
#include <libkl.h>

/*
 * rstat_cmd -- Display router chip status
 */
int
rstat_cmd(int argc, char **argv)
{
	int width = SW_WORD;
	unsigned long long val;
	unsigned long long vector;

	argv++; argc--;

	if (argc != 1)
		return(1);

	if (*atobu_L(*argv, &vector))
		return(1);

	rstat((net_vec_t)vector);

	return(0);
}

