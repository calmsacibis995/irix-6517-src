/* Inventory searching code.
 */

/*
 * NOTE on changes for SN0.
 * These functions are used by the IO6prom for various
 * functions but the current functions do not work at all.
 * The way it is made to work for SN0 is to trap the
 * case where GetChild etc return NULL. Instead of just
 * returning NULL, we try to see if a SN0 specific 
 * function can be called.
 */
#ident "$Revision: 1.8 $"

#include <parser.h>
#include <arcs/hinv.h>
#include <libsc.h>
#include <libsc_internal.h>

static void look_tape(COMPONENT *);
char *kl_inv_find(void) ;

/* Return the name of the cpu board
 * This is used when booting the miniroot to determine which unix to run
 */
char *
inv_findcpu(void)
{
	COMPONENT *c = GetChild(NULL);
	char *p;

	if (!c) {
		if (p = kl_inv_find())
			return (p + 4) ;
		
		return(NULL);
	}

	/* Return cfgroot->id, stripping off the SGI prefix if present
	 */
	p = c->Identifier;
	if (!strncmp(p,"SGI ",3))
		p += 4;
	return(p);
}

int iscdrom;	/* used for sash -m stuff */

char *
fixupname(COMPONENT *tape)
{
	char *name = getpath(tape);

	if (iscdrom = (tape->Type == CDROMController ||
	    GetParent(tape)->Type == CDROMController))
		strcat(name,"partition(8)");

	if (tape->Type == NetworkController) {
		char *cp;

		strcat(name,"bootp()");

		cp = getenv("netinsthost");
		if (cp) {
			strcat(name,cp);
			cp = getenv("netinstfile");
			if (cp) {
				strcat(name,":");
				strcat(name,cp);
			}
		}
	}

	return(name);
}

#define MAXTAPES 16
static COMPONENT *tapes[MAXTAPES];
static int tplist;

/*
 * Return device path for a tapedrive/CDROM.
 */
char *
inv_findtape(void)
{
	int i,j;

	tplist = 0;
	bzero(tapes,MAXTAPES*sizeof(COMPONENT *));

	look_tape(GetChild(NULL));

	if (tplist >= MAXTAPES)
		printf("warning: too many tape/CD drives present.\n"
		       "warning: only %d supported.\n", MAXTAPES);

	if (tplist == 1) {
		return(fixupname(tapes[0]));
	}
	else if (tplist > 1) {
		char first_token[LINESIZE];
		char response[LINESIZE];
getchoice:
		printf("Select device for installation: \n");
		for ( i = 0; i < tplist; i++)
			printf("%d - %s %s\n", i+1,
			       GetParent(tapes[i])->Type == CDROMController ?
			       "CD-ROM" : "tape", getpath(tapes[i]));
		printf("which? ");
		sa_get_response (response);
		printf("\n");
		if ((token (response, first_token) != 1) || first_token[1]) 
			goto getchoice;
		else {
			for ( j = i = 0; first_token[i] && first_token[i] != '\n'; i++ )
				if ( first_token[i] >= '0' && first_token[i] <= '9' )
					j = (j*10) + (first_token[i]-'0');
				else 
					goto error;

				if ( j >= 1 && j <= tplist) {
					return(fixupname(tapes[j-1]));
				}
				else {
					printf("%d is not a valid selection\n", j);
					goto error;
				}
		}
		error:
		printf("Please enter a number between 1 and %d.\n", tplist);
		goto getchoice;
	}
	else
		return(0);
}

static void
look_tape(COMPONENT *c)
{
	COMPONENT *p;

	if (!c || (tplist >= MAXTAPES)) return;

	if (c->Class == ControllerClass) {
		if ((c->Type == TapeController || c->Type == CDROMController)
		    && (p=GetChild(c))) {

			do {
				tapes[tplist++] = p;
			} while (p = GetPeer(p)) ;	/* each peripheral */
		}
	}

	look_tape(GetChild(c));
	look_tape(GetPeer(c));

	return;
}

/*
 * find the first component with the matching type in the configuration tree
 * with c as its root
 */
COMPONENT *
find_type(COMPONENT *c, CONFIGTYPE type)
{
	COMPONENT *c1;

	if (c == (COMPONENT *)NULL)
		return (COMPONENT *)NULL;

	if (c->Type == type)
		return c;

	if ((c1 = find_type(GetChild(c), type)) != (COMPONENT *)NULL)
		return c1;

	if ((c1 = find_type(GetPeer(c), type)) != (COMPONENT *)NULL)
		return c1;

	return (COMPONENT *)NULL;
}

/*
 * count the components with the matching type in the configuration tree
 * with c as its root
 */
int
count_type(COMPONENT *c, CONFIGTYPE type)
{
	int tmp, local;

	if (c == (COMPONENT *)NULL)
		return 0;

	local = (c->Type == type);

	if (tmp=count_type(GetChild(c), type))
		local += tmp;

	if (tmp=count_type(GetPeer(c), type))
		local += tmp;

	return(local);
}
