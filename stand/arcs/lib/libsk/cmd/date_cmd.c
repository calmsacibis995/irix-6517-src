#ident	"lib/libsk/cmd/date_cmd.c: $Revision: 1.11 $"

/*
 * date_cmd.c - print out the date stored in the BB clock
 */

#include <arcs/types.h>
#include <arcs/time.h>
#include <libsc.h>
#include <libsk.h>
#include <ctype.h>

#if IP22 || (MFG_USED && IP28) || IP32
#define _SETDATE 1		/* standalone supports setting the date */
#endif

static char *month[] = {
    "",
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

#ifdef _SETDATE
/* convert 2 characters to decimal even if they look like octal */
#define todigit(X)	(X-'0')
#define twochtoi(str,dst)				\
	if (isdigit(str[0]) && isdigit(str[1]))		\
		dst = todigit(str[0])*10 + todigit(str[1]);\
	else						\
		return -1;

/*
 * format like IRIX: mmddhhmm[ccyy|yy][.ss]
 */
int
format_date(TIMEINFO *nt, char *p)
{
	TIMEINFO *t;

	t = GetTime();
	nt->Year = t->Year;		/* year is optional */
	nt->Milliseconds = nt->Seconds = 0;

	if (strlen(p) < 8)
		return -1;

	twochtoi(p,nt->Month); p += 2;
	twochtoi(p,nt->Day); p += 2;
	twochtoi(p,nt->Hour); p += 2;
	twochtoi(p,nt->Minutes); p += 2;

	if (*p == '.') {
seconds:
		p++;				/* skip . */
		if ((*p && *(p+1)) == 0)
			return -1;
		twochtoi(p,nt->Seconds);
		p += 2;
	}
	else if (*p && *(p+1)) {
		int tmp, yr;

		twochtoi(p,yr);
		p += 2;

		if (*p && (*p != '.') && *(p+1)) {
			twochtoi(p,tmp);
			p += 2;
			yr = yr * 100 + tmp;
		}
		else
			/* if yr < 70 assume turn of century */
			yr += (yr < 70) ? 2000 : 1900;

		nt->Year = yr;

		if (*p == '.')
			goto seconds;
	}
	if (*p)
		return -1;

	return 0;
}
#endif

/*ARGSUSED*/
int
date_cmd(int argc, char **argv, char **envp)
{
	TIMEINFO *t;
#ifdef _SETDATE
	TIMEINFO nt;
#endif

	switch (argc) {
#ifdef _SETDATE
	case 2:
		if (strcmp (argv[1], "-d") == 0) {
			t = GetTime();		/* GetTime checks validity */
			printf ("Decimal date: %u %u %u %u %u %u %u.\n",
			    t->Month, t->Day, t->Year, t->Hour,
			    t->Minutes, t->Seconds, t->Milliseconds);
			return(0);
		}
		
		/* format like IRIX: mmddhhmm[ccyy|yy][.ss]
		 */
		if (format_date (&nt, argv[1]) < 0)
			break;

    		cpu_set_tod(&nt);

		/* FALLTHROUGH and print date as set */
#endif
	case 1:
		t = GetTime();		/* GetTime checks validity */
    		printf("%s %u %u, %02u:%02u:%02u GMT\n", month[t->Month],
			t->Day,t->Year,t->Hour,t->Minutes,t->Seconds);
		return(0);
	}

	return(1);
}

#if _SETDATE && MFG_USED
void
set_date_for_mfg(void)
{
	TIMEINFO *t, nt;

	/* format like IRIX: mmddhhmm[ccyy|yy][.ss] */
	format_date(&nt, "0101000090");
	cpu_set_tod(&nt);
	t = GetTime();		/* GetTime checks validity */
    	printf("%s %u %u, %02u:%02u:%02u GMT\n", month[t->Month],
		t->Day,t->Year,t->Hour,t->Minutes,t->Seconds);
}
#endif /* _SETDATE && MFG_USED */
