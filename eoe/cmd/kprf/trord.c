#ident  "$Revision: 1.1 $"

/*
 * Create binary ordinal configuration file
 */

#include	<stdio.h>
#include	<fcntl.h>

struct	nameent {
	char	name[28];
	int	ord;
};

#define	MAXNAMETAB	10000
struct	nameent	nametab[MAXNAMETAB];

#define	COMMAND		"nm -B /unix"
#define	UNIX_FILE	"/unix"

main(ac, av)
int	ac;
char	*av[];
{
	FILE	*fp;
	char	line[BUFSIZ];
	char	addr[128];
	char	type[128];
	char	name[128];
	int	n;
	int	i;
	int	ord;
	int	fd;
	char	kernel_name[64];
	char	cmd_name[96];

	if (ac < 2) {
		fprintf(stderr, "Usage:  %s outfile [kernel] \n", av[0]);
		exit(1);
	}
	else
	if (ac == 2) {
		strcpy(kernel_name, UNIX_FILE);
	}
	else
		strcpy(kernel_name, av[2]);

	sprintf(cmd_name, "nm -B %s\n", kernel_name);

	if ((fd = creat(av[1], O_WRONLY)) < 0)
		syserr(av[1]);

	if ((fp = popen(cmd_name, "r")) == NULL)
		syserr(cmd_name);

	i = 0;

	while (fscanf(fp, "%s %s %s", addr, type, name) != EOF) {

		if ((type[0] != 't') && (type[0] != 'T'))
			continue;

		if (i >= MAXNAMETAB) {
			fprintf(stderr, "nametab overflow!\n");
			exit(1);
		}

		bcopy(name, nametab[i].name, 28);
		nametab[i].name[27] = '\0';
		nametab[i].ord = i + 1;

		/*
		 * ignore duplicate names
		 */
		if (i > 0)
			if (strcmp(nametab[i].name, nametab[i-1].name) == 0)
				continue;

		i++;
	}

	fclose(fp);

	if (write(fd, nametab, i * sizeof (struct nameent)) < 0)
		syserr("write");
}

syserr(s)
char	*s;
{
	perror(s);
	exit(1);
}
