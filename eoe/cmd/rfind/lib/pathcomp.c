#include <limits.h>

#define eos(c) (!*(c))
#define slash(c) (*(c) == SLASH)
#define eocomp(c) (slash(c) || eos(c))
#define dot(c) (*(c) == DOT)
#define dot1(c) (dot(c) && eocomp(c+1))
#define dot2(c) (dot(c) && dot(c+1) && eocomp(c+2))

 char *
pathcomp(char *p)
{
	register char * a=p;
	register char * b=p;
	register char SLASH = '/';
	register char DOT = '.';
	int j;
	int root_based=0;

	if (!p || !*p)
		return p;
	if (slash (p)) {
		*b++ = *a++;
		root_based = 1;
	}

	for (;;) {
		if (slash (a))
			while (slash (++a));
		if (eos (a)) {
			if (b==p) *b++ = DOT;
			*b = '\0';
			return (p);
		} else if (dot1 (a)) {
			a++;
		} else if (dot2(a) && root_based && (b==p+1)) {
			a += 2;
		} else {
			if ((b!=p) && !slash(b-1))
				*b++ = SLASH;
			for (j=NAME_MAX; (j--) && (!eocomp(a)); )
				*b++ = *a++;
			while (!eocomp(a))
				a++;
		}
	}
}
