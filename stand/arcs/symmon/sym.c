#include "ctype.h"
#include "dbgmon.h"
#include "libsc.h"
#include "mp.h"
#include "sys/idbg.h"

extern struct dbstbl *dbstab;
extern char *nametab;
extern pd_name_table_t pd_name_table[];
static long fetch_pd_addr(char *name);
extern int symmax;		/* symmax is now received from kernel */

/* 
 *	Search for name, return kernel address
 *	return 0 if can't find name
 *	assumed table is sorted in ascending kernel addr
 *	after last entry has noffst = -1
 */
caddr_t
fetch_kaddr(char *s)
{
	register struct dbstbl *dbptr, *dbend;
	register char *targetname,*curname;
	caddr_t addr;

	if ( symtab() )
		return(0);
	dbptr = &dbstab[0];
	dbend = &dbstab[symmax-1];

	for ( ; dbptr <= dbend; dbptr++ ) {
		if (dbptr->addr == 0)
			goto chk_maddr;
		targetname = s;	
		curname = &nametab[dbptr->noffst];
		while( *targetname == *curname ) {
			if (*targetname == 0) 
				return((caddr_t)dbptr->addr);
			targetname++;
			curname++;
		}
	}

	/* Search the list of loaded modules' symbol tables */
chk_maddr:
	if (do_it2 ("maddr", (__psint_t)&addr, (__psint_t)s))
		return (0);
	if (addr)
		return (addr);
	else	
		return(0);
}

/*
 *	search for kernel address,
 *	linear search for name with address <= addr
 *	return -1 if can't find 
 *	0 otherwise, name is pointed to by nameptr
 */		

char symname[50];

char *
fetch_kname(inst_t *inst_p)		 
{
	register struct dbstbl *dbptr, *dbend, *last;
	char *s = symname;
	__psunsigned_t offst;
	__psunsigned_t addr = (__psunsigned_t)inst_p;

	if (symtab() || addr == 0)
		return((char *)0);
	dbptr = &dbstab[0];
	dbend = &dbstab[symmax-1];
	last = 0;
	for ( ; dbptr <= dbend; dbptr++ ) {
		if (dbptr->addr == 0)
			goto chk_mname;
		if (addr <= dbptr->addr)
			break;
		last = dbptr;
	}
	if (last == 0 && addr != dbptr->addr)
		goto chk_mname;
	if (addr == dbptr->addr)
		last = dbptr;
	if ( (private.noffst = addr - last->addr) > 0x2000 )
		goto chk_mname;
	return &nametab[last->noffst];

	/* 
	 * Search the list of loaded modules' symbol tables. Pass the
	 * address in offst, but an offst is returned in offst if there 
	 * is one.
	 */
chk_mname:

	*s = 0;
	offst = addr;
	if (do_it2 ("mname", (__psint_t)&offst, (__psint_t)s))
		return ((char *)0);
	private.noffst = offst;
	if (*s)
		return (s);
	else
		return ((char *)0);
}

/*
 * fetch_pd_addr -- looks up a platform-dependent name
 * and returns an associated number (usually an address).
 */
long
fetch_pd_addr(char *name)
{
	register pd_name_table_t *pdptr;

	for (pdptr = pd_name_table; pdptr->pd_name; pdptr++)
		if (!strcmp(pdptr->pd_name, name))
			return(pdptr->pd_value);
	return(0);
}


/*
 *	nmtohx
 *
 */
/*ARGSUSED*/
int
nmtohx(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	numinp_t addr;

	if ( argc == 1 )
		return(1);
	while (--argc > 0) {
		argv++;
		if (!parse_sym(*argv,&addr))
			printf("%s \t0x%Ix\n",*argv,addr);
		else	
			printf("%s: cannot find matching address\n",*argv);
	}
	return(0);
}

/*ARGSUSED*/
int
hxtonm(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	char *nameptr;
	numinp_t addr;

	if ( argc == 1 )
		return(1);
	while (--argc > 0) {
		argv++;
		if (*atob_s(*argv, &addr)) {
			printf("illegal address: %s\n", *argv);
			continue;
		}
		if ( (nameptr = fetch_kname((inst_t *)addr)) != (char *)0 )
			printf("0x%Ix\t%s+0x%x\n",addr,nameptr,private.noffst);
		else
			printf("0x%Ix: cannot find matching name\n",addr);
	}
	return(0);
}

int
parse_sym_range(char *str,numinp_t *adr,int *cnt, int width)
{
	char tmp[64];
	char *t;
	char *s = str;
	char c, oc;
	numinp_t taddr;

	if (!(str && *str))
		return 1;

	if (width == 0)
		width = SW_BYTE;

	oc = '+';

	taddr = 0;
	*adr = 0;

	s = str;
	do {
		/* Gather token into t, skip white space */
		t = tmp;
		while ( *s && *s!='+' && *s!='-' && *s!=':' && *s!='#')
			if ( *s != ' ' && *s != '\t' )
				*t++ = *s++;
			else
				s++;
		/* save terminator in c */
		c = *s++;
		*t = '\0';

		if ( isdigit(*tmp) ) {
			if (*atob_s(tmp,&taddr) ) {
				printf("illegal number -- %s\n", tmp);
				return 1;
			}
		}
		else
		if ( *tmp == '$' ) {
			if ( lookup_regname(tmp+1, &taddr) ) {
				printf("illegal register name == %s\n", tmp);
				return 1;
			}
		}
		else
		if (!(taddr = fetch_pd_addr(tmp))) {
			if (!(taddr = (numinp_t)fetch_kaddr(tmp))) {
				printf("symbol not found -- %s\n", tmp);
				return 1;
			}
		}

		switch ( oc ) {
			case '+': *adr += taddr; break;
			case '-': *adr -= taddr; break;
			case ':':
				if ( cnt )
					*cnt = (taddr-*adr)/width;
				return 0;
			case '#':
				if ( cnt )
					*cnt = taddr;
				return 0;
		}
	} while (oc = c);

	*adr &= ~(width-1);

	return(0);
}

int
parse_sym(char *str, numinp_t *adr)
{
	return parse_sym_range(str,adr,0,0);
}

/*ARGSUSED*/
int
lkaddr(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	register struct dbstbl *dbptr, *dbend, *last;
	static numinp_t saveaddr = 0;
	u_numinp_t addr;
	int count;
	int found = 0;

	if ( symtab() )
		return(0);

	if ( argc == 1 ) {
		addr = saveaddr;
	} else if ( argc == 2 ) {
		argv++;
		if (*atobu_L(*argv, &addr) &&
		    parse_sym(*argv,(numinp_t *)&addr))
			return (1);
	} else
		return(1);

	dbptr = &dbstab[0];
	last = &dbstab[0];
	dbend = &dbstab[symmax-1];
	for ( ; dbptr <= dbend; dbptr++ ) {
		if (dbptr->addr == 0) {
			printf ("%s not found\n", *argv);
			return(1);
		}
		if (addr <= (u_numinp_t)(dbptr->addr)) {
			found = 1;
			break;
		}
		last = dbptr;
	}
	dbptr = last;

	/* check the loaded module symbol tables */
	if (!found) {
		do_it2 ("mlkaddr", (long)addr, 0);
		return (0);
	}

	for (count = 0; count < 10; count++, dbptr++) {
		if (dbptr->addr == (__psunsigned_t)-1)
			break;
		else if (dbptr->addr != 0)
			printf("0x%x\t%s\n",
				dbptr->addr, &nametab[dbptr->noffst]);
	}

	saveaddr = (long long)((dbptr->addr == -1L) ?
				dbstab[0].addr : dbptr->addr);
	return (0);
}

/*ARGSUSED*/
int
lkup(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	register struct dbstbl *dbptr, *dbend;

	if ( argc != 2 ) 
		return(1);

	if ( symtab() )
		return(0);

	argv++;
	dbptr = &dbstab[0];
	dbend = &dbstab[symmax-1];
	for ( ; dbptr <= dbend; dbptr++ ) {
		if (dbptr->addr == 0)
			return(0);

		if (strstr (&nametab[dbptr->noffst],*argv))
			printf("0x%x\t%s\n",
				dbptr->addr, &nametab[dbptr->noffst]);
	}
	/* also do a lookup in the loaded module symbol tables */
	do_it2 ("mlkup", (__psint_t)*argv, 0);
	return (0);
}
