# include	<stdio.h>
# include	<fcntl.h>
# include	<unistd.h>

# define	PREFIX		"/dev/dsk/"
# define	SUFFIX		"vh"

struct {
	char	*drive;
	int	lowc;
	int	highc;
	int	lowunit;
	int	highunit;
	int	lowlun;
	int	highlun;
} nmtab[] = {
	"dks", 0, 128, 1, 15, 0, 15,
	"jag", 0,   7, 0, 15, 0, 0,
	"rad", 0, 128, 1, 15, 0, 0,
	"ipi", 0,   3, 0, 15, 0, 0,
	"xyl", 0,   3, 0,  3, 0, 0,
	"ips", 0,   1, 0,  3, 0, 0,
	0
};

int
main(int argc, char **argv)
{
   int		i;
   int		j;
   int		k;
   int		l;
   int		fd;
   char		nbuf[256];

	for (i = 0; nmtab[i].drive != 0; i++) {
		for (j = nmtab[i].lowc; j <= nmtab[i].highc; j++) {
			for (l = nmtab[i].lowlun; l <= nmtab[i].highlun;l++) {
				for (k = nmtab[i].lowunit; k <= nmtab[i].highunit;k++){
					if ( l ) {
						sprintf(nbuf, "%s%s%dd%dl%d%s", PREFIX,
						nmtab[i].drive, j, k, l, SUFFIX);
					} else {
						sprintf(nbuf, "%s%s%dd%d%s", PREFIX,
							nmtab[i].drive, j, k, SUFFIX);
					}
					if ((fd = open(nbuf, O_RDONLY)) == -1)
						continue;
					printf("%s%s%dd%d\n", PREFIX, nmtab[i].drive,
						j, k);
					close(fd);
				}
			}
		}
	}
	exit(0);
	/* NOTREACHED */
}
