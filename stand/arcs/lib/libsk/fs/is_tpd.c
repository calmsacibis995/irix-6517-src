#ident	"lib/libsk/fs/is_tpd.c:  $Revision: 1.4 $"

/*
 * is_tpd.c -- tape directory routines
 */

#include <sys/types.h>
#include <libsc.h>
#include <tpd.h>

#define PROOT_LBN		-1
#define PROOT_NBYTES		0
#define PSEUDO_ROOT(x)		\
	((x)->te_lbn == PROOT_LBN && (x)->te_nbytes == PROOT_NBYTES)

void swap_tp_dir (struct tp_dir *);

/*
 * The following three functions (is_tpd, tpd_match, tpd_list) should
 * be static in tpd.c, but are made global for the bootp code.
 */

/*
 * is_tpd -- decide if we're looking at a reasonable volume_header
 */
int
is_tpd(struct tp_dir *td)
{
	register int csum;
	register int *ip;
	extern int Verbose;

	if (td->td_magic != TP_MAGIC) {
		if (td->td_magic == TP_SMAGIC) {
		    if (Verbose)
			    printf ("Tape is swapped tpd format.\n");

		    swap_tp_dir (td);

		    /* forget the checksum
		     */
		    return 1;

		} else 
		    return 0;
	}

	csum = 0;
	for (ip = (int *)td; ip < (int *)(td + 1); ip++)
		csum += *ip;

	return(csum == 0);
}

int
tpd_match(char *name, struct tp_dir *td, struct tp_entry *tep)
{
	register struct tp_entry *te;
	char filename[64];
	
	/*
	 * If a file of the form "(filename)" is given,
	 * strip off the parens.
	 */
	if ( *name == '(' ) {
		char *cp;
		strcpy(filename, name);
		name = filename;
		for (cp = ++name; *cp && *cp != ')'; cp++ )
			;
		*cp = '\0';
		}

	if(*name == '/' && name[1] != '\0') {
		char *cp;
		for(cp = name+1; *cp; cp++)
			cp[-1] = *cp;
		cp[-1] = '\0';
		}

	if (strcmp(name, "/") == 0) {
		/* set up pseudo-root directory
		 */
		tep->te_name[0] = '/';
		tep->te_name[1] = '\0';
		tep->te_lbn = (unsigned)PROOT_LBN;
		tep->te_nbytes = PROOT_NBYTES;
		return(1);
	}

	for (te = td->td_entry; te < &td->td_entry[TP_NENTRIES]; te++) {
		if (strncmp(name, te->te_name, TP_NAMESIZE) == 0) {
			*tep = *te;
			return(1);
		}
	}

	return(0);
}

void
tpd_list(struct tp_dir *td)
{
	register struct tp_entry *te;

	for (te = td->td_entry; te < &td->td_entry[TP_NENTRIES]; te++) {
		if (te->te_nbytes)
			printf("\t%s\t%d bytes @ block %d\n", te->te_name, te->te_nbytes, te->te_lbn);
	}
}

void
swap_tp_dir (struct tp_dir *td)
{
	int entry;

	/* Leave the magic number alone as an indication that
	 * the tape is swapped.
	swapl (&td->td_magic, 1);
	 */
	swapl (&td->td_cksum, 7);

	for (entry = 0; entry < TP_NENTRIES; entry++)
	    swapl (&td->td_entry[entry].te_lbn, 2);
}
