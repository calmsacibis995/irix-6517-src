#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char	*alpha;
int	asize = 1;
int	asplit;
char	*buf;
int	dflag;
int	len = 1;
int	nproc = 1;
int	nflag;

void	mkf(int idx, int p);

int
main(int argc, char **argv)
{
	int		a;
	int		stat;
	long long	tot;

	argc--;
	argv++;
	while (argc) {
		if (strcmp(*argv, "-c") == 0) {
			argc--;
			argv++;
			asize = atoi(*argv);
			if (asize > 64 || asize < 1) {
				fprintf(stderr, "bad alpha size %s\n", *argv);
				return 1;
			}
		} else if (strcmp(*argv, "-d") == 0) {
			dflag = 1;
		} else if (strcmp(*argv, "-l") == 0) {
			argc--;
			argv++;
			len = atoi(*argv);
			if (len < 1) {
				fprintf(stderr, "bad name length %s\n", *argv);
				return 1;
			}
		} else if (strcmp(*argv, "-p") == 0) {
			argc--;
			argv++;
			nproc = atoi(*argv);
			if (nproc < 1) {
				fprintf(stderr, "bad process count %s\n",
					*argv);
				return 1;
			}
		} else if (strcmp(*argv, "-n") == 0) {
			nflag = 1;
		}
		argc--;
		argv++;
	}
	if (asize % nproc) {
		fprintf(stderr,
			"alphabet size must be multiple of process count\n");
		return 1;
	}
	if (nflag && nproc > 1) {
		fprintf(stderr, "-n process count must be 1\n");
		return 1;
	}
	asplit = asize / nproc;
	alpha = malloc(asize + 1);
	buf = malloc(len + 1);
	for (a = 0, tot = 1; a < len; a++)
		tot *= asize;
	fprintf(stderr,
		"alpha size = %d, name length = %d, total files = %lld, nproc=%d\n",
		asize, len, tot, nproc);
	fflush(stderr);
	for (a = 0; a < asize; a++) {
		if (a < 26)
			alpha[a] = 'a' + a;
		else if (a < 52)
			alpha[a] = 'A' + a - 26;
		else if (a < 62)
			alpha[a] = '0' + a - 52;
		else if (a == 62)
			alpha[62] = '_';
		else if (a == 63)
			alpha[63] = '@';
	}
	for (a = 0; a < nproc; a++) {
		if (fork() == 0) {
			mkf(0, a);
			return 0;
		}
	}
	while (wait(&stat) > 0)
		continue;
	return 0;
}

void
mkf(int idx, int p)
{
	int	i;
	int	last;

	last = (idx == len - 1);
	if (last) {
		buf[len] = '\0';
		for (i = p * asplit; i < (p + 1) * asplit; i++) {
			buf[idx] = alpha[i];
			if (nflag)
				puts(buf);
			else if (dflag)
				mkdir(buf, 0777);
			else
				close(creat(buf, 0666));
		}
	} else {
		for (i = 0; i < asize; i++) {
			buf[idx] = alpha[i];
			mkf(idx + 1, p);
		}
	}
}
