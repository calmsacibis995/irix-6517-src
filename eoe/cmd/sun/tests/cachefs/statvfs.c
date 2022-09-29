#include <stdio.h>
#include <sys/types.h>
#include <sys/statvfs.h>

int
main(int argc, char **argv)
{
	char *progname;
	struct statvfs sb;

	progname = *argv;
	for (--argc, ++argv; argc; argc--, argv++) {
		if (statvfs(*argv, &sb) == -1) {
			perror(*argv);
		} else {
			printf("flags: ");
			if (sb.f_flag) {
				if (sb.f_flag & ST_RDONLY) {
					printf("ST_RDONLY ");
				}
				if (sb.f_flag & ST_NOSUID) {
					printf("ST_NOSUID ");
				}
				if (sb.f_flag & ST_NOTRUNC) {
					printf("ST_NOTRUNC ");
				}
				if (sb.f_flag & ST_NODEV) {
					printf("ST_NODEV ");
				}
				if (sb.f_flag & ST_GRPID) {
					printf("ST_GRPID ");
				}
				if (sb.f_flag & ST_LOCAL) {
					printf("ST_LOCAL ");
				}
				printf("(0x%x)\n", (int)sb.f_flag);
			} else {
				printf("- NONE -\n");
			}
		}
	}
	return(0);
}
