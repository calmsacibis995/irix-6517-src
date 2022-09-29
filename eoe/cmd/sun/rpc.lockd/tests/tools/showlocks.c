#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

char *typestr[] = {
	"<null type>",
	"F_RDLCK",
	"F_WRLCK",
	"F_UNLCK",
};

void
usage(char *progname)
{
	fprintf(stderr, "usage: %s [-r start,len] filenames\n", progname);
}

int
main(int argc, char **argv)
{
	int opt;
	int setlen = 0;
	int count;
	off_t byte;
	char *progname = *argv;
	char *file;
	struct flock fl;
	int fd;
	struct hostent *hp;
	struct in_addr in;
	char *token;
	off_t start;
	off_t len;
	struct stat sb;

	while ((opt = getopt(argc, argv, "r:")) != EOF) {
		switch (opt) {
			case 'r':
				token = strtok(optarg, ", ");
				if (!token) {
					usage(progname);
					exit(-1);
				}
				start = (off_t)strtoll(token, NULL, 0);
				token = strtok(NULL, " ");
				if (token) {
					len = (off_t)strtoll(token, NULL, 0);
				} else {
					setlen = 1;
				}
				break;
			default:
				usage(progname);
				exit(-1);
		}
	}
	for ( count = 0; optind < argc; count = 0, optind++) {
		file = argv[optind];
		fd = open(file, O_RDWR);
		if (fd == -1) {
			perror(file);
		} else {
			if (setlen) {
				if (fstat(fd, &sb) == -1) {
					perror(file);
					close(fd);
					continue;
				}
				len = (off_t)(sb.st_size - start);
			}
			for (byte = start; byte < start + len; byte++) {
				bzero(&fl, sizeof(fl));
				fl.l_type = F_WRLCK;
				fl.l_start = byte;
				fl.l_len = 1;
				if (fcntl(fd, F_RGETLK, &fl) == -1) {
					perror("fcntl");
				} else if (fl.l_type != F_UNLCK) {
					count++;
					hp = gethostbyaddr((char *)&fl.l_sysid, sizeof(fl.l_sysid),
						AF_INET);
					if (hp) {
						printf("%s: %s %ld %ld %s %d\n", file,
							typestr[fl.l_type], fl.l_start, fl.l_len,
							hp->h_name, fl.l_pid);
					} else {
						in.s_addr = ntohl(fl.l_sysid);
						printf("%s: %s %ld %ld %s (0x%x) %d\n", file,
							typestr[fl.l_type], fl.l_start, fl.l_len,
							inet_ntoa(in), (int)fl.l_sysid, fl.l_pid);
					}
				}
			}
			if (!count) {
				printf("%s: no locks\n", file);
			}
			close(fd);
		}
	}
	return(0);
}
