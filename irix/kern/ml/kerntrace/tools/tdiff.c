
#include	<stdio.h>

char	*file;

main(ac, av)
int	ac;
char	*av[];
{
	FILE	*fp;
	int	s, u;
	int	lasts, lastu;
	int	ds, du;
	char	string[256];
	int	i;
	int	first = 1;
	int	scancnt;

	if (ac != 2)
		usage(av[0]);

	file = av[1];

	if ((fp = fopen(file, "r")) == NULL)
		syserr(file);

	scancnt = fscanf(fp, "%d.%d %s", &s, &u, string);
	if (scancnt == 3)
	  while ((scancnt = fscanf(fp, "%d.%d %s", &s, &u, string)) > 0) {

		if (s == lasts) {
			ds = 0;
			du = u - lastu;
		} else {
			ds = s - lasts - 1;
			du = (1000000 - lastu) + u;
			if (du >= 1000000) {
				ds++;
				du -= 1000000;
			}
		}

		if (first) {
			ds = du = 0;
			first = 0;
		}
		
		printf("+%d.%06d\t%d.%06d\t%s\n",
			ds, du, s, u,  string);

		lasts = s;
		lastu = u;
	  } else {

	    /* not a timstamp, just a counter */

	    unsigned int s, lasts;


	    fseek( fp, 0L, SEEK_SET );

	    while ((scancnt = fscanf(fp, "%x %s", &s, string)) > 0) {

		if (s == lasts) {
			ds = 0;
		} else {
			ds = s - lasts - 1;
		}

		if (first) {
			ds = 0;
			first = 0;
		}
		
		printf("+0x%x\t0x%x\t%s\n",
			ds, s, string);

		lasts = s;
	    }
	  }
}

err(fmt, a1, a2)
char	*fmt, *a1, *a2;
{
	fprintf(stderr, fmt, a1, a2);
	fprintf(stderr, "\n");
	exit(1);
}

usage(s)
char	*s;
{
	fprintf(stderr, "Usage:  %s file\n", s);
	exit(1);
}

syserr(s)
char	*s;
{
	perror(s);
	exit(1);
}
