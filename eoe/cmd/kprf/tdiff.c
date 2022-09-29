#ident  "$Revision: 1.1 $"

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

	if (ac != 2)
		usage(av[0]);

	file = av[1];

	if ((fp = fopen(file, "r")) == NULL)
		syserr(file);

	while (fscanf(fp, "%d.%d %s", &s, &u, string) > 0) {

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
