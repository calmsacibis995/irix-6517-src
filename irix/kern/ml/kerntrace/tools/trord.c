/*
 * Create binary ordinal configuration file
 */

#include	<stdio.h>
#include	<fcntl.h>
#include	"trace.h"

#define	COMMAND		"nm -B /unix"
#define	UNIX_FILE	"/unix"

struct	nameent	nametab[MAXNAMETAB];

main(ac, av)
int	ac;
char	*av[];
{
	FILE	*fp;
	char	line[BUFSIZ];
	long long	addr;
	char	type[128];
	char	name[128];
	int	n;
	int	i;
	int	ord;
	int	fd;
	char	kernel_name[64];
	char	cmd_name[96];
	int	scancnt;

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

	if ((fd = open(av[1], O_CREAT|O_WRONLY, 0666)) < 0)
		syserr(av[1]);

	if ((fp = popen(cmd_name, "r")) == NULL)
		syserr(cmd_name);

	i = 0;

	while ((scancnt = fscanf(fp, "%llx %s %s", &addr, type, name)) != EOF) {

		/* Following "if" statement handles the case of "weak" symbols
		 * which have an extra "(weak)" on the "nm -B"  output.
		 */
		if (scancnt == 0) {
			scancnt = fscanf(fp, "%s", name );
			if (!scancnt) {
				printf("error in nm scan, %llx %s %s\n",
				       addr, type, name);
				break;
			}
			if (strcmp(name, "(weak)") != 0) {
				printf("ERROR in nm scan, found: %s\n", name);
			}
		}

		if ((type[0] != 't') && (type[0] != 'T'))
			continue;

		if (i >= MAXNAMETAB) {
			fprintf(stderr, "nametab overflow!\n");
			exit(1);
		}

		bcopy(name, nametab[i].name, MAXNAMESIZE);
		nametab[i].name[MAXNAMESIZE-1] = '\0';
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
