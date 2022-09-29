
#ident "$Revision: 1.10 $"

#define _KMEMUSER 1	/* so sys/types will define k_machreg_t */
#include <fcntl.h>
#include <sys/syssgi.h>
#include "lboot.h"
#include <sys/EVEREST/dang.h>

/* extended probe space mechanism */
/* Parser and scanner for the language:
	ex_probe_spec	= PROBE EQU 6tuple
			| PROBE EQU n6tuple
	n6tuple		= OPAR 6tuple CPAR
			| OPAR 6tuplecom 6tuple
	6tuplecom	= 6tuple COM
	6tuple		= OPAR RWSEQ COM BUS-SPACE COM NUM COM NUM 
			  COM NUM COM NUM CPAR
*/

#define PR_START		1
#define PR_NOTTOK		2
#define PR_OPAR			3
#define PR_CPAR			4
#define PR_COMMA		5
#define PR_RWSEQ		6
#define PR_NUMBER		7
#define PR_OPAR_or_RWSEQ	8
#define PR_COM_or_CPAR		9
#define PR_COMSPACE	       10
#define PR_SPACE	       11


#ifdef DEBUG
static char *tokstr[] = {
	"NO STRING",
	"PR_START",
	"PR_NOTTOK",
	"PR_OPAR",
	"PR_CPAR",
	"PR_COMMA",
	"PR_RWSEQ",
	"PR_NUMBER",
	"PR_OPAR_or_RWSEQ",
	"PR_COM_or_CPAR"
	"PR_COMSPACE",
	"PR_SPACE"
};
#endif

struct Ntuple {
	struct Ntuple *next;
	char *rwseq;
	int space;
	unsigned int values[4];
};

#define POS_ADDR	0
#define POS_SIZE	1
#define POS_NALUE	2
#define POS_MASK	3

#define	N_addr values[POS_ADDR]
#define	N_size values[POS_SIZE]
#define	N_value values[POS_NALUE]
#define	N_mask values[POS_MASK]

static struct Ntuple *Ntuple_list = NULL;

static int nxttok(char *, unsigned long *, int);
static int probe_dang_table(int ndx, struct Ntuple *);


int
exprobesp_parse(int argc, char **argv, int bustype)
{
	register char **argvp = argv;
	int openpar = 0;
	register int state = PR_START;
	register int tok;
	register int inseq = 0;
	unsigned long number;
	struct Ntuple *Ntp = NULL, *tNtp;

	free_Ntuple_list();

	/* Skip over the "exprobe_space=", which has already been parsed by caller */
	argvp += 2;
	argc -= 2;

	for (; argc > 0;) {
		tok = nxttok(*argvp, &number, bustype);
#ifdef PROBEDEBUG
		if (tok == PR_NUMBER)
			fprintf(stderr,"%s %s %ld\n",*argvp,tokstr[tok],number);
		else
			fprintf(stderr,"%s %s\n", *argvp, tokstr[tok]);
#endif
		switch (tok) {
		case PR_NOTTOK:
			goto err_ret;

		case PR_OPAR:
			openpar++;
			if (state == PR_START)
				state = PR_OPAR_or_RWSEQ;
			else if (state == PR_OPAR || state == PR_OPAR_or_RWSEQ)
				state = PR_RWSEQ;
			else
				goto err_ret;
			break;

		case PR_RWSEQ:
			if (state == PR_RWSEQ || state == PR_OPAR_or_RWSEQ) {
				tNtp = (struct Ntuple *)mymalloc(sizeof(*Ntp));
				if (Ntp)
					Ntp->next = tNtp;
				else
					Ntuple_list = tNtp;
				Ntp = tNtp;
				Ntp->next = NULL;
				Ntp->rwseq = *argvp;
				inseq = 1;
				state = PR_COMSPACE;
			} else
				goto err_ret;
			break;

		case PR_COMMA:
			if (state == PR_COMMA)
				state = PR_NUMBER;
			else if (state == PR_COM_or_CPAR)
				state = PR_OPAR;
			else if (state == PR_COMSPACE)
				state = PR_SPACE;
			else
				goto err_ret;
			break;

		case PR_SPACE:
			if (state != PR_SPACE)
				goto err_ret;

			state = PR_COMMA;
			Ntp->space = number;
			inseq++;
			break;

		case PR_NUMBER:
			if (state == PR_NUMBER) {
				if (++inseq == 6)
					state = PR_CPAR;
				else
					state = PR_COMMA;
				if (inseq - 3 == POS_SIZE && number > 4) {
					fprintf(stderr,
					   "exprobe_space: Illegal probe size: %u\n",
					   number);
					goto err_ret;
				}
				Ntp->values[inseq - 3] = number;
			} else
				goto err_ret;
			break;

		case PR_CPAR:
			if (--openpar < 0)
				goto err_ret;
			if (state == PR_CPAR) {
				if (openpar == 0)
					return argvp - argv + 1;
				else
					state = PR_COM_or_CPAR;
			} else if (state == PR_COM_or_CPAR) {
				if (openpar == 0)
					return argvp - argv + 1;
				else
					goto err_ret;
			} else
				goto err_ret;
			break;
		}
		argvp++;
		argc--;
	}
err_ret:
	free_Ntuple_list();
	return 0;
}

static int
nxttok(char *token, unsigned long *np, int bustype)
{
	char *endtok = token;
	unsigned long val;

	switch (*token) {
	case '(':
		return PR_OPAR;
	case ')':
		return PR_CPAR;
	case ',':
		return PR_COMMA;
	case 'r':
	case 'w':
	case 's':
		while (*++token == 'r' || *token == 'w' || *token == 's')
				;
		if (*token)
			return PR_NOTTOK;
		return PR_RWSEQ;
	default:
		*np = strtoul(token, &endtok, 0);
		if(endtok == token || *endtok) {
			if( (val = get_base(bustype,token)) == -1 )
				return PR_NOTTOK;
			*np = val;
			return PR_SPACE;
		}
		return PR_NUMBER;
	}
}

int
exprobesp(int adap, int btype)
{
	register struct Ntuple *Ntp = Ntuple_list;
	unsigned long buf;
	register char *action;
	int	 i, pci_scan_mode = 0, pci_scan_ok = 0;
	struct lboot_iospace probe;

	if (Ntp == NULL)
		error(ER1, "exprobe_space called without argument list");

	/* handle DANG wild card */
	if (btype == ADAP_DANG
	    && adap == -1 && !Ntp->next && !Ntp->N_addr) {
		for (i = 0; i < DANG_MAX_INDX; i++) {
			if (probe_dang_table(i,Ntp))
				return TRUE;
		}
		return FALSE;
	}

	/* walk down the probe list */
	for (; Ntp; Ntp = Ntp->next) {
		/* get the number of adapters */
		probe.ios_iopaddr = Ntp->N_addr;
		probe.ios_size = Ntp->N_size;
		probe.ios_type = Ntp->space;
		for (action = Ntp->rwseq; *action; action++) {
			if (btype == ADAP_DANG) {
				if (!probe_dang_table(Ntp->N_addr, Ntp))
					return FALSE;
			} else if (*action == 'w') {
				buf = Ntp->N_value & Ntp->N_mask;
				if (syssgi(SGI_IOPROBE, btype, adap,
					&probe, IOPROBE_WRITE, buf ))
					return FALSE;
			} else if (*action == 's') {
				pci_scan_mode = 1;
				buf = Ntp->N_value & Ntp->N_mask;
				/* in scan mode, OR instead of AND results */
				if (!syssgi(SGI_IOPROBE, btype, adap,
					&probe, IOPROBE_WRITE, buf )) 
					pci_scan_ok = 1;
			} else {
				buf = 0;
				if (syssgi(SGI_IOPROBE, btype, adap,
					&probe, IOPROBE_READ, &buf ))
					return FALSE;
				buf >>= (sizeof(ulong) - Ntp->N_size)*8;
				if ((buf & Ntp->N_mask) !=
				    (Ntp->N_value & Ntp->N_mask))
					return FALSE;
			}
		}
	}

	if (pci_scan_mode)
		return pci_scan_ok;
	else
		return TRUE;
}

void
free_Ntuple_list(void)
{
	register struct Ntuple *Ntp, *nNtp;

	for (Ntp = Ntuple_list; Ntp;) {
		nNtp = Ntp->next;
		free(Ntp);
		Ntp = nNtp;
	}
	Ntuple_list = NULL;
}


/*  -----  DANG probing support follows ----- */

static dangid_t *dang_ids = 0;

/* initialize table of dang_ids, by reading from /dev/kmem */
static int
init_dang_table(void)
{
	static int	dang_table_initialized;

	if (dang_table_initialized || dang_ids != NULL) {
		return 0;
	}

	dang_table_initialized = 1;

	dang_ids = calloc (DANG_MAX_INDX, sizeof(dangid_t));
	if (dang_ids == NULL)
		return 0;

	if (syssgi(SGI_READ_DANGID, dang_ids, 
		DANG_MAX_INDX * sizeof(dangid_t))){
		free(dang_ids);
		dang_ids = NULL;
		error (ER7, "syssgi: SGI_READ_DANGID failed: no dang_id list found in kernel");
	}

	return (DANG_MAX_INDX * sizeof(dangid_t));

}

/*
 * Look in the kernel's table of DANG chip IDs.
 */
static int
probe_dang_table(int ndx,struct Ntuple *Ntp)
{
	/* 
	 * If we could not get the dang_ids table from the kernel, then
	 * assume that all h/w is there.
	 */
	(void)init_dang_table();
	if (!dang_ids) 
		return TRUE;

	if (ndx >= DANG_MAX_INDX)
		return FALSE;

	if (((dang_ids[ndx] ^ Ntp->N_value) & Ntp->N_mask) != 0)
		return FALSE;
	else
		return TRUE;
}
