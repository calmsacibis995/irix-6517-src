#include <parser.h>
#include <libsc.h>
#include <arcs/io.h>
#include <arcs/errno.h>

/*
 * checksum -- calculate the checksum of a block of memory or a file.  the
 * 	code uses the same algorithm as the 'sum -r' unix command
 */
int
checksum(int argc, char *argv[])
{
	register unsigned short	cksum = 0x0000;
	register unsigned long	count;
	register unsigned char	*cp;
	register unsigned short	msb = 0x8000;
	struct range		range;

	if (argc < 2 || argc > 3)
		return 1;

	/* checksum on a file */
	if (argc == 3 && ((argv[1][0] != '-') || (argv[1][1] != 'f')))
		return 1;

	/* checksum on a block of memory */
	if (argc == 2 && !parse_range(argv[1], 1, &range))
		return 1;

	if (argc == 2) {
		cp = (unsigned char *)range.ra_base;
		count = range.ra_count;
		while (count--) {
			cksum = (unsigned short)(cksum & 0x0001 ?
			 	((cksum >> 1) | msb) : (cksum >> 1));
			cksum += *cp++;
		}
	}

#define	BUF_SIZE	16384
	if (argc == 3) {
		unsigned char	*buf;
		unsigned long	cc;
		unsigned long	fd;
		long		rc;
		unsigned long	size = 0;

		buf = align_malloc(BUF_SIZE,BUF_SIZE);
		if (buf == 0) {
			printf("no memory for file buffer\n");
			return 0;
		}

		if ((rc = Open(argv[2], OpenReadOnly, &fd)) != ESUCCESS) {
            		perror(rc, argv[2]);
			return 0;
        	}

        	while ((rc = Read(fd, buf, BUF_SIZE, &cc)) == ESUCCESS && cc) {
			cp = buf;
			count = cc;
			size += count;
			while (count--) {
				cksum = (unsigned short)(cksum & 0x0001 ?
			 		((cksum >> 1) | msb) : (cksum >> 1));
				cksum += *cp++;
			}
		}

        	Close(fd);

        	if (rc != ESUCCESS) {
			perror(rc,"read error");
			/* Still print check-sum anyway */
		}

		align_free(buf);

		printf("file size = %lu\n", size);
	}

	printf("checksum = %u\n", cksum);

	return 0;
}
