
#ident "$Revision: 1.14 $"

#include <fcntl.h>
#include "lboot.h"

/* extended probe mechanism */
/* Parser and scanner for the language:
	ex_probe_spec	= PROBE EQU 5tuple
			| PROBE EQU n5tuple
	n5tuple		= OPAR 5tuple CPAR
			| OPAR 5tuplecom 5tuple
	5tuplecom	= 5tuple COM
	5tuple		= OPAR RWSEQ COM NUM COM NUM COM NUM COM NUM CPAR
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
};
#endif

struct Vtuple {
	struct Vtuple *next;
	char *rwseq;
	unsigned long values[4];
};

#define POS_ADDR	0
#define POS_SIZE	1
#define POS_VALUE	2
#define POS_MASK	3

#define	V_addr values[POS_ADDR]
#define	V_size values[POS_SIZE]
#define	V_value values[POS_VALUE]
#define	V_mask values[POS_MASK]

static struct Vtuple *Vtuple_list = NULL;

static int nxttok(char *, unsigned long *);


int
exprobe_parse(register int argc, char **argv)
{
	register char **argvp = argv;
	int openpar = 0;
	register int state = PR_START;
	register int tok;
	register int inseq = 0;
	unsigned long number;
	struct Vtuple *Vtp = NULL, *tVtp;

	free_Vtuple_list();

	/* Skip over the "exprobe=", which has already been parsed by caller */
	argvp += 2;
	argc -= 2;

	for (; argc > 0;) {
		tok = nxttok(*argvp, &number);
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
				tVtp = (struct Vtuple *)mymalloc(sizeof(*Vtp));
				if (Vtp)
					Vtp->next = tVtp;
				else
					Vtuple_list = tVtp;
				Vtp = tVtp;
				Vtp->next = NULL;
				Vtp->rwseq = *argvp;
				inseq = 1;
				state = PR_COMMA;
			} else
				goto err_ret;
			break;

		case PR_COMMA:
			if (state == PR_COMMA)
				state = PR_NUMBER;
			else if (state == PR_COM_or_CPAR)
				state = PR_OPAR;
			else
				goto err_ret;
			break;

		case PR_NUMBER:
			if (state == PR_NUMBER) {
				if (++inseq == 5)
					state = PR_CPAR;
				else
					state = PR_COMMA;
				if (inseq - 2 == POS_SIZE && number > 4) {
					fprintf(stderr,
					   "exprobe: Illegal probe size: %u\n",
					   number);
					goto err_ret;
				}
				Vtp->values[inseq - 2] = number;
			} else
				goto err_ret;
			break;

		case PR_CPAR:
			if (--openpar < 0)
				goto err_ret;
			if (state == PR_CPAR) {
				if (openpar == 0)
					return (int)(argvp - argv + 1);
				else
					state = PR_COM_or_CPAR;
			} else if (state == PR_COM_or_CPAR) {
				if (openpar == 0)
					return (int)(argvp - argv + 1);
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
	free_Vtuple_list();
	return 0;
}

static int
nxttok(register char *token, register unsigned long *np)
{
	char *endtok = token;

	switch (*token) {
	case '(':
		return PR_OPAR;
	case ')':
		return PR_CPAR;
	case ',':
		return PR_COMMA;
	case 'r':
		while (*token == 'r') {
			++token;
			if (*token == 'n')
				++token;
		}
		if (*token)
			return PR_NOTTOK;
		return PR_RWSEQ;
	case 'w':
		while (*++token == 'w');
		if (*token)
			return PR_NOTTOK;
		return PR_RWSEQ;
	default:
		*np = strtoul(token, &endtok, 0);
		if(endtok == token || *endtok)
			return PR_NOTTOK;
		return PR_NUMBER;
	}
}

int
exprobe(void)
{
	register struct Vtuple *Vtp = Vtuple_list;
	register int fd;
	unsigned long buf;
	register char *action;

	if (Vtp == NULL)
		error(ER1, "exprobe called without argument list");

	if ((fd = open("/dev/kmem", O_RDWR)) < 0)
		error(ER2,  "exprobe: open");

	for (; Vtp; Vtp = Vtp->next) {
		for (action = Vtp->rwseq; *action; action++) {
			if (lseek(fd, (off_t)Vtp->V_addr & LBOOT_LSEEK_MASK, 0) < 0)
				goto err_ret;
			if (*action == 'w') {
				buf = Vtp->V_value & Vtp->V_mask;
				if (write(fd, &buf, Vtp->V_size) != Vtp->V_size)
					goto err_ret;
			} else {
				buf = 0;
				if (read(fd, &buf, Vtp->V_size) != Vtp->V_size)
					goto err_ret;
				buf >>= (sizeof(ulong) - Vtp->V_size) * 8;
				if (action[1] == 'n') {
					if ((buf & Vtp->V_mask) ==
					    (Vtp->V_value & Vtp->V_mask))
						goto err_ret;
					++action;
				} else {
					if ((buf & Vtp->V_mask) !=
					    (Vtp->V_value & Vtp->V_mask))
						goto err_ret;
				}
			}
		}
	}
	(void)close(fd);
	return TRUE;

err_ret:
	(void)close(fd);
	return FALSE;
}

void
free_Vtuple_list(void)
{
	register struct Vtuple *Vtp, *nVtp;

	for (Vtp = Vtuple_list; Vtp;) {
		nVtp = Vtp->next;
		free(Vtp);
		Vtp = nVtp;
	}
	Vtuple_list = NULL;
}
