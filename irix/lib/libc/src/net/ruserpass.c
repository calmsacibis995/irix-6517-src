/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifdef __STDC__
	#pragma weak ruserpass = _ruserpass
#endif
#include "synonyms.h"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)ruserpass.c	5.1 (Berkeley) 5/30/85";
#endif

#include <sys/types.h>
#include <stdio.h>
#include <utmp.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>

#include <string.h>		/* prototypes for string functions */
#include <strings.h>		/* prototype for index() */
#include <unistd.h>		/* for prototypes */
#include <stdlib.h>		/* for prototypes */
#include "pwd.h"
#include "priv_extern.h"

/*
 * Forward References
 */
static	void blkencrypt(char *, int);
static	void enblkclr(char *, char *);
static	void enblknot(char *, char *);
static	void mkpwclear(char *, char, char *);
static	void nbssetkey(char *);
static	void renv(char *, char **, char **);
static	void rnetrc(char *, char **, char **);
static	void sreverse(char *, char *);
static	char *nbsdecrypt(char *, char *, char *);
static	char *mkenvkey(char, char *);
static	int token(FILE *cfile);

static char *renvlook(char *); 
static struct	utmp *getutmp(char *,struct utmp *);
static char *deblkclr(char *);
static char *nbs8decrypt(char *, char *);

void
ruserpass(char *host, char **aname, char **apass)
{

	renv(host, aname, apass);
	if (*aname == 0 || *apass == 0)
		rnetrc(host, aname, apass);
	if (*aname == 0) {
		char *myname = getlogin();

		if (myname == NULL) {
			struct passwd *pw;

			pw = getpwuid(getuid());
			if (pw == NULL) {
				fprintf(stderr, "Who are you?\n");
				exit(-1);
			}
			myname = pw->pw_name;
		}
		*aname = malloc(16);
		printf("Name (%s:%s): ", host, myname);
		fflush(stdout);
		if (read(2, *aname, 16) <= 0)
			exit(1);
		if ((*aname)[0] == '\n')
			*aname = myname;
		else
			if (index(*aname, '\n'))
				*index(*aname, '\n') = 0;
	}
	if (*aname && *apass == 0) {
		printf("Password (%s:%s): ", host, *aname);
		fflush(stdout);
		*apass = getpass("");
	}
}

static void
renv(char *host, char **aname, char **apass)
{
	register char *cp;
	char *comma;

	cp = renvlook(host);
	if (cp == NULL)
		return;
	if (!isalpha(cp[0]))
		return;
	comma = index(cp, ',');
	if (comma == 0)
		return;
	if (*aname == 0) {
		*aname = malloc((size_t)(comma - cp + 1));
		strncpy(*aname, cp, (size_t)(comma - cp));
	} else
		if (strncmp(*aname, cp, (size_t)(comma - cp)))
			return;
	comma++;
	cp = malloc(strlen(comma)+1);
	strcpy(cp, comma);
	*apass = malloc(16);
	mkpwclear(cp, host[0], *apass);
}

static char *
renvlook(char *host)
{
	register char *cp, **env;

	env = environ;
	for (env = environ; *env != NULL; env++)
		if (!strncmp(*env, "MACH", 4)) {
			cp = index(*env, '=');
			if (cp == 0)
				continue;
			if (strncmp(*env+4, host, (size_t)(cp-(*env+4))))
				continue;
			return (cp+1);
		}
	return (NULL);
}

#define	DEFAULT	1
#define	LOGIN	2
#define	PASSWD	3
#define	NOTIFY	4
#define	WRITE	5
#define	YES	6
#define	NO	7
#define	COMMAND	8
#define	FORCE	9
#define	ID	10
#define	MACHINE	11

/* Allocate to save static space */
#define TOKVALSIZE	128
static char *tokval _INITBSS;

/* Note: compact this and put in read-only const arena */
#define TOKMAXNAME	10
static const struct toktab {
	char tokstr[TOKMAXNAME + 1];
	unsigned char tval;
} toktab[]= {
	"default",	DEFAULT,
	"login",	LOGIN,
	"password",	PASSWD,
	"notify",	NOTIFY,
	"write",	WRITE,
	"yes",		YES,
	"y",		YES,
	"no",		NO,
	"n",		NO,
	"command",	COMMAND,
	"force",	FORCE,
	"machine",	MACHINE,
	"",		0
};

static void
rnetrc(char *host, char **aname, char **apass)
{
	register FILE *cfile;
	char *hdir, buf[BUFSIZ];
	int t;
	struct stat stb;

	hdir = getenv("HOME");
	if (hdir == NULL)
		hdir = ".";
	sprintf(buf, "%s/.netrc", hdir);
	cfile = fopen(buf, "r");
	if (cfile == NULL) {
		if (_oserror() != ENOENT)
			perror(buf);
		return;
	}
	if (tokval == 0 && ((tokval = (char *)calloc(1, TOKVALSIZE)) == 0))
		return;
next:
	while ((t = token(cfile))) switch(t) {

	case DEFAULT:
		(void) token(cfile);
		continue;

	case MACHINE:
		if (token(cfile) != ID || strcmp(host, tokval))
			continue;
		while ((t = token(cfile)) && t != MACHINE) switch(t) {

		case LOGIN:
			if (token(cfile))
				if (*aname == 0) { 
					*aname = malloc(strlen(tokval) + 1);
					strcpy(*aname, tokval);
				} else {
					if (strcmp(*aname, tokval))
						goto next;
				}
			break;
		case PASSWD:
			if (fstat(fileno(cfile), &stb) >= 0
			    && (stb.st_mode & 077) != 0) {
	fprintf(stderr, "Error - .netrc file not correct mode.\n");
	fprintf(stderr, "Remove password or correct mode.\n");
				exit(1);
			}
			if (token(cfile) && *apass == 0) {
				*apass = malloc(strlen(tokval) + 1);
				strcpy(*apass, tokval);
			}
			break;
		case COMMAND:
		case NOTIFY:
		case WRITE:
		case FORCE:
			(void) token(cfile);
			break;
		default:
	fprintf(stderr, "Unknown .netrc option %s\n", tokval);
			break;
		}
		goto done;
	}
done:
	fclose(cfile);
}

static int
token(FILE *cfile)
{
	char *cp;
	int c;
	const struct toktab *t;

	if (feof(cfile))
		return (0);
	while ((c = getc(cfile)) != EOF &&
	    (c == '\n' || c == '\t' || c == ' ' || c == ','))
		continue;
	if (c == EOF)
		return (0);
	cp = tokval;
	if (c == '"') {
		while ((c = getc(cfile)) != EOF && c != '"') {
			if (c == '\\')
				c = getc(cfile);
			*cp++ = (char)c;
		}
	} else {
		*cp++ = (char)c;
		while ((c = getc(cfile)) != EOF
		    && c != '\n' && c != '\t' && c != ' ' && c != ',') {
			if (c == '\\')
				c = getc(cfile);
			*cp++ = (char)c;
		}
	}
	*cp = 0;
	if (tokval[0] == 0)
		return (0);
	for (t = toktab; t->tokstr[0]; t++)
		if (!strcmp(t->tokstr, tokval))
			return (t->tval);
	return (ID);
}
/* rest is nbs.c stolen from berknet */


/* external crypt data table shared with gen/crypt.c */
extern char __libc_IP[];
extern char __libc_FP[];
extern char __libc_PC1_C[];
extern char __libc_PC1_D[];
extern char __libc_shifts[];
extern char __libc_PC2_C[];
extern char __libc_PC2_D[];
extern char __libc_E[];
extern char __libc_S[8][64];
extern char __libc_P[];

static
char *nbsdecrypt(char *cpt, char *key, char *result)
{
	char *s;
	char c,oldbuf[20];
	result[0] = 0;
	strcpy(oldbuf,key);
	while(*cpt){
		for(s = cpt;*s && *s != '$';s++);
		c = *s;
		*s = 0;
		strcpy(oldbuf,nbs8decrypt(cpt,oldbuf));
		strcat(result,oldbuf);
		if(c == 0)break;
		cpt = s + 1;
		}
	return(result);
}

static
char *nbs8decrypt(char *crp, char *key)
{
	char keyblk[100], blk[100];

	enblkclr(keyblk,key);
	nbssetkey(keyblk);

	enblknot(blk,crp);
	blkencrypt(blk,1);			/* backward dir */

	return(deblkclr(blk));
}

static void
enblkclr(char *blk, char *str)		/* ignores top bit of chars in string str */
{
	register int i,j;
	char c;
	for(i=0;i<70;i++)blk[i] = 0;
	for(i=0; (c= *str) && i<64; str++){
		for(j=0; j<7; j++, i++)
			blk[i] = (char)((c>>(6-j)) & 01);
		i++;
	}
}

/* move out of function scope so we get a global symbol for use with data cording */
static char iobuf[12] _INITBSSS;

static
char *deblkclr(char *blk)
{
	register int i,j;
	char c;
	for(i=0; i<10; i++){
		c = 0;
		for(j=0; j<7; j++){
			c <<= 1;
			c |= blk[8*i+j];
			}
		iobuf[i] = c;
	}
	iobuf[i] = 0;
	return(iobuf);
}

static void
enblknot(char *blk, char *crp)
{
	register int i,j;
	char c;
	for(i=0;i<70;i++)blk[i] = 0;
	for(i=0; (c= *crp) && i<64; crp++){
		if(c>'Z') c -= 6;
		if(c>'9') c -= 7;
		c -= '.';
		for(j=0; j<6; j++, i++)
			blk[i] = (char)((c>>(5-j)) & 01);
	}
}

/*
 * This program implements the
 * Proposed Federal Information Processing
 *  Data Encryption Standard.
 * See Federal Register, March 17, 1975 (40FR12134)
 */

/*
 * The C and D arrays used to calculate the key schedule.
 */

/*
 * The key schedule.
 * Generated from the key.
 */
#define KSR1 16
#define KSR2 48
struct _KS{
	char elem[KSR1][KSR2];
};
static struct _KS *KS _INITBSS;

/*
 * Set up the key schedule from the key.
 */

static void
nbssetkey(char *key)
{
	register int i, j, k;
	char C[28], D[28];
	int t;

	if (KS == 0 && ((KS = (struct _KS *)calloc(1, KSR1*KSR2)) == 0))
		return;

	/*
	 * First, generate C and D by permuting
	 * the key.  The low order bit of each
	 * 8-bit char is not used, so C and D are only 28
	 * bits apiece.
	 */
	for (i=0; i<28; i++) {
		C[i] = key[__libc_PC1_C[i]-1];
		D[i] = key[__libc_PC1_D[i]-1];
	}
	/*
	 * To generate Ki, rotate C and D according
	 * to schedule and pick up a permutation
	 * using PC2.
	 */
	for (i=0; i<16; i++) {
		/*
		 * rotate.
		 */
		for (k=0; k<__libc_shifts[i]; k++) {
			t = C[0];
			for (j=0; j<28-1; j++)
				C[j] = C[j+1];
			C[27] = (char)t;
			t = D[0];
			for (j=0; j<28-1; j++)
				D[j] = D[j+1];
			D[27] = (char)t;
		}
		/*
		 * get Ki. Note C and D are concatenated.
		 */
		for (j=0; j<24; j++) {
			KS->elem[i][j] = C[__libc_PC2_C[j]-1];
			KS->elem[i][j+24] = D[__libc_PC2_D[j]-28-1];
		}
	}
}


/*
 * The current block, divided into 2 halves.  char L[64], tempL[32], f[32];
 */

/*
 * The combination of the key and the input, before selection. char preS[48];
 */

/*
 * The payoff: encrypt a block.
 */

static void
blkencrypt(char *block, int edflag)
{
	int i, ii;
	register int t, j, k;
	char L[64], tempL[32], f[32], preS[48];
	char *R = &L[32];

	if (KS == 0 && ((KS = (struct _KS *)calloc(1, KSR1*KSR2)) == 0))
		return;

	/*
	 * First, permute the bits in the input
	 */
	for (j=0; j<64; j++)
		L[j] = block[__libc_IP[j]-1];
	/*
	 * Perform an encryption operation 16 times.
	 */
	for (ii=0; ii<16; ii++) {
		/*
		 * Set direction
		 */
		if (edflag)
			i = 15-ii;
		else
			i = ii;
		/*
		 * Save the R array,
		 * which will be the new L.
		 */
		for (j=0; j<32; j++)
			tempL[j] = R[j];
		/*
		 * Expand R to 48 bits using the E selector;
		 * exclusive-or with the current key bits.
		 */
		for (j=0; j<48; j++)
			preS[j] = R[__libc_E[j]-1] ^ KS->elem[i][j];
		/*
		 * The pre-select bits are now considered
		 * in 8 groups of 6 bits each.
		 * The 8 selection functions map these
		 * 6-bit quantities into 4-bit quantities
		 * and the results permuted
		 * to make an f(R, K).
		 * The indexing into the selection functions
		 * is peculiar; it could be simplified by
		 * rewriting the tables.
		 */
		for (j=0; j<8; j++) {
			t = 6*j;
			k = __libc_S[j][(preS[t+0]<<5)+
				(preS[t+1]<<3)+
				(preS[t+2]<<2)+
				(preS[t+3]<<1)+
				(preS[t+4]<<0)+
				(preS[t+5]<<4)];
			t = 4*j;
			f[t+0] = (char)((k>>3)&01);
			f[t+1] = (char)((k>>2)&01);
			f[t+2] = (char)((k>>1)&01);
			f[t+3] = (char)((k>>0)&01);
		}
		/*
		 * The new R is L ^ f(R, K).
		 * The f here has to be permuted first, though.
		 */
		for (j=0; j<32; j++)
			R[j] = L[j] ^ f[__libc_P[j]-1];
		/*
		 * Finally, the new L (the original R)
		 * is copied back.
		 */
		for (j=0; j<32; j++)
			L[j] = tempL[j];
	}
	/*
	 * The output L and R are reversed.
	 */
	for (j=0; j<32; j++) {
		t = L[j];
		L[j] = R[j];
		R[j] = (char)t;
	}
	/*
	 * The final output
	 * gets the inverse permutation of the very original.
	 */
	for (j=0; j<64; j++)
		block[j] = L[__libc_FP[j]-1];
}
/*
	getutmp()
	return a pointer to the system utmp structure associated with
	terminal sttyname, e.g. "/dev/tty3"
	Is version independent-- will work on v6 systems
	return NULL if error
*/
static struct utmp *
getutmp(char *sttyname, struct utmp *utmpstr)
{
	FILE *fdutmp;

	if(sttyname == NULL || sttyname[0] == 0)return(NULL);

	fdutmp = fopen("/etc/utmp","r");
	if(fdutmp == NULL)return(NULL);

	while(fread(utmpstr,1,sizeof (struct utmp),fdutmp) == sizeof (struct utmp))
		if(strcmp(utmpstr->ut_line,sttyname+5) == 0){
			fclose(fdutmp);
			return(utmpstr);
		}
	fclose(fdutmp);
	return(NULL);
}

static void
sreverse(register char *sto, register char *sfrom)
{
	register int i;

	i = (int)strlen(sfrom);
	while (i >= 0)
		*sto++ = sfrom[i--];
}

static char *
mkenvkey(char mch, char *skey)
{
	struct utmp putmp;
	char stemp[40], stemp1[40], sttyname[30];
	register char *sk,*p;

	if (isatty(2))
		strcpy(sttyname,ttyname(2));
	else if (isatty(0))
		strcpy(sttyname,ttyname(0));
	else if (isatty(1))
		strcpy(sttyname,ttyname(1));
	else
		return (NULL);
	if (getutmp(sttyname, &putmp) == NULL)
		return (NULL);
	sk = skey;
	p = putmp.ut_line;
	while (*p)
		*sk++ = *p++;
	*sk++ = mch;
	sprintf(stemp, "%d", putmp.ut_time);
	sreverse(stemp1, stemp);
	p = stemp1;
	while (*p)
		*sk++ = *p++;
	*sk = 0;
	return (skey);
}

static void
mkpwclear(char *sencpasswd,char mch,char *spasswd)
{
	char skey[40];

	if (sencpasswd[0] == 0) {
		spasswd[0] = 0;
		return;
	}
	if (mkenvkey(mch, skey) == NULL) {
		fprintf(stderr, "Can't make key\n");
		exit(1);
	}
	nbsdecrypt(sencpasswd, skey, spasswd);
}
