#include <string.h>
#include <unistd.h>
#include <limits.h>
#define NDEBUG
#include <assert.h>
#include "fsdump.h"

/* pwd code - mutated from cmd/pwd/pwd.c */
static char    pwdname[PATH_MAX];
static int     pwdoff = -1;

void pwdprname(void)
{
        write(1, "/", 1);
        if (pwdoff<0)
                pwdoff = 0;
        pwdname[pwdoff] = '\n';
        write(1, pwdname, pwdoff+1);
}

int pwdcat(char *s)
{
        register int i, j;

        i = -1;
        while (s[++i] != 0) 
	        if ((pwdoff+i+2) > PATH_MAX - 1) {
	                pwdprname();
			return 1;		/* tells pwd(): end this path */
		}
        for(j=pwdoff+1; j>=0; --j)
                pwdname[j+i+1] = pwdname[j];
        pwdoff=i+pwdoff+1;
        pwdname[i] = '/';
        for(--i; i>=0; --i)
                pwdname[i] = s[i];
	return 0;				/* normal: pwd() continues */
}

void pwd(ino64_t dino) {
	inod_t *p;
	nlink_t j, nlnk;
	dent_t *ep;

	p = PINO(&mh,dino);
	nlnk = p->i_xnlink;
	for (j=0; j<nlnk; j++) {
		pwdoff = -1;
		for (ep = PLNK(mh,p,j); ep != mh.hp_dent; ep = PDDE(mh,ep))
			if (pwdcat (PNAM(mh,ep)))
				break;
		if (pwdoff != -1)
			pwdprname();
	}
}
