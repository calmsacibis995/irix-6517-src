#ident	"lib/libsc/cmd/memdb_cmd.c:  $Revision: 1.21 $"

/*
 * more memory commands
 *
 */

#include <parser.h>
#include <libsc.h>
#include <sgidefs.h>

static void	mem_error(char *);

#define	SW_BYTE		1
#define	SW_HALFWORD	2
#define	SW_WORD		4
#define SW_DOUBLE	8

/*
 * mcopy -- copy a block of memory
 */
int
mcopy(int argc, char **argv)
{
	register char *from, *to;
	register __psint_t len = 0;
	register int size = SW_WORD;

	if (argc < 2)
		return(1);

	from = to = 0;

	while (--argc > 0) {
		if (**++argv == '-') {
			switch ((*argv)[1]) {
			case 'b':
				size = SW_BYTE;
				break;

			case 'h':
				size = SW_HALFWORD;
				break;

			case 'w':
				size = SW_WORD;
				break;

			default:
				return(1);
			}
		} else {
			struct range range;

			if (! parse_range(*argv, size, &range))
				return(1);
			if (from == 0) {
				from = (char *)range.ra_base;
				len = range.ra_count;
			} else {
				to = (char *)range.ra_base;
			}
		}
	}

	printf("copy %d %s from %x to %x\n", len,
		size == SW_BYTE ? "bytes" :
#if _MIPS_SIM != _ABIO32
		size == SW_HALFWORD ? "shorts" : 
		size == SW_WORD ? "words" : "doubles",
#else
		size == SW_HALFWORD ? "shorts" : "words",
#endif
		from, to);

	switch (size) {
	case SW_BYTE:
		while (--len >= 0) {
			*to = *from;
			from += size;
			to += size;
		}
		break;

	case SW_HALFWORD:
		while (--len >= 0) {
			*(short *)to = *(short *)from;
			from += size;
			to += size;
		}
		break;

	case SW_WORD:
		while (--len >= 0) {
			*(__int32_t *)to = *(__int32_t *)from;
			from += size;
			to += size;
		}
		break;

	default:
		mem_error("mcopy");
	}
	return(0);
}

/*
 * mcmp -- compare two blocks of memory
 */
int
mcmp(int argc, char **argv)
{
	register char *from, *to;
	register __psint_t len = 0;
	register int size = SW_WORD;

	if (argc < 2)
		return(1);

	from = to = 0;

	while (--argc > 0) {
		if (**++argv == '-') {
			switch ((*argv)[1]) {
			case 'b':
				size = SW_BYTE;
				break;

			case 'h':
				size = SW_HALFWORD;
				break;

			case 'w':
				size= SW_WORD;
				break;

			default:
				return(1);
			}
		} else {
			struct range range;

			if (! parse_range(*argv, size, &range))
				return(1);
			if (from == 0) {
				from = (char *)range.ra_base;
				len = range.ra_count;
			} else {
				to = (char *)range.ra_base;
			}
		}
	}

	printf("compare %d %s from %x to %x\n", len,
		size == SW_BYTE ? "bytes" :
		size == SW_HALFWORD ? "shorts" :
		"words",
		from, to);

	switch (size) {
	case SW_BYTE:
		while (--len >= 0) {
			if (*to != *from) {
				printf("Miscompare: %x (%x) != %x (%x)\n",
					from, *from & 0xff, to, *to & 0xff);
				return (0);
			}
			from += size;
			to += size;
		}
		break;

	case SW_HALFWORD:
		while (--len >= 0) {
			if (*(short *)to != *(short *)from) {
				printf("Miscompare: %x (%x) != %x (%x)\n",
					from, *(short *)from & 0xffff,
					to,   *(short *)to   & 0xffff);
				return (0);
			}
			from += size;
			to += size;
		}
		break;

	case SW_WORD:
		while (--len >= 0) {
			if (*(int *)to != *(int *)from) {
				printf("Miscompare: %x (%x) != %x (%x)\n",
					from, *(int *)from, to, *(int *)to);
				return (0);
			}
			from += size;
			to += size;
		}
		break;

	default:
		mem_error("mcmp");
	}
	return(0);
}

/*
 * mfind -- search for a value in a block of memory
 */
/*ARGSUSED*/
int
mfind(int argc, char ** argv, char **bunk1, struct cmd_table *bunk2)
{
	auto int value;
	register long cnt = 0;
	register __psint_t base = 0;
	register int size = SW_WORD;
	register int nflag = 0;
	register int dostr = 0;
	char *str;
	int all = 0;
	int rc = 0;

	if (argc < 2)
		return(1);

	while (--argc > 0) {
		if (**++argv == '-') {
			switch ((*argv)[1]) {
			case 'b':
				size = SW_BYTE;
				break;

			case 'h':
				size = SW_HALFWORD;
				break;

			case 'w':
				size= SW_WORD;
				break;

#if _MIPS_SIM != _ABIO32
			case 'd':
				size = SW_DOUBLE;
				break;
#endif

			case 'n':
				nflag++;
				break;

			case 'a':
				all++;
				break;

			case 's':
				dostr++;
				str = *++argv;
				argc--;
				break;

			default:
				return(1);
			}
		} else {
			if (base == 0) {
				struct range range;

				if (! parse_range(*argv, size, &range))
					return(1);
				base = range.ra_base;
				cnt = range.ra_count;
			} else {
				if (!dostr)
					if (*atob(*argv, &value))
						return(1);
			}
		}
	}

	if (dostr)
		printf("search for the string %s from 0x%x to 0x%x\n",
			str, base, base + (cnt * size));
	else
		printf("search for the %s %s%x from %x to %x\n",
			size == SW_BYTE ? "byte" :
			size == SW_HALFWORD ? "short" : "word",
			(nflag) ? "!" : "",
			value,
			base, base + (cnt * size));

	/*
	 * The truth value we seek in comparisons is based
	 * on the setting of nflag.  If nflag is true, we are
	 * searching for MISmatches, else we are searching for
	 * matches.
	 */

	switch (size) {
	case SW_BYTE:
		while (--cnt >= 0) {
			if (dostr) {
				if (strcmp ((char *)base, str) == 0) {
					printf ("Found %s at 0x%x\n", 
						(char *)base, base);
					return (0);
				}
			} else if (((*(char *)base & 0xff) != (value & 0xff))
			    == nflag) {
				printf("First %smatch: %x (%x) %c= %x\n",
					nflag ? "mis" : "", base,
					*(char *)base & 0xff,
					nflag ? '!' : '=',
					value & 0xff);
				if (!all) return (1);
			}
			base += size;
		}
		break;

	case SW_HALFWORD:
		while (--cnt >= 0) {
			if (dostr) {
				if (strcmp ((char *)base, str) == 0) {
					printf ("Found %s at 0x%x\n", 
						(char *)base, base);
					return (0);
				}
			} else if (((*(short *)base & 0xffff) != (value & 0xffff))
			    == nflag) {
				printf("First %smatch: %x (%x) %c= %x\n",
					nflag ? "mis" : "", base,
					*(short *)base & 0xffff,
					nflag ? '!' : '=',
					value & 0xffff);
				if (!all) return (1);
			}
			base += size;
		}
		break;

	case SW_WORD:
		while (--cnt >= 0) {
			if (dostr) {
				if (strcmp ((char *)base, str) == 0) {
					printf ("Found %s at 0x%x\n", 
						(char *)base, base);
					return (0);
				}
			} else if ((*(int *)base != value) == nflag) {
				printf("%smatch: %x (%x) %c= %x\n",
					nflag ? "mis" : "", base,
					*(int *)base,
					nflag ? '!' : '=',
					value);
				if (!all) return(1);
			}
			base += size;
		}
		break;

#if _MIPS_SIM != _ABIO32
	case SW_DOUBLE:
		while (--cnt >= 0) {
			if (dostr) {
				if (strcmp ((char *)base, str) == 0) {
					printf ("Found %s at 0x%x\n", 
						(char *)base, base);
					return (0);
				}
			} else if ((*(long *)base != value) == nflag) {
				printf("First %smatch: %x (%x) %c= %x\n",
					nflag ? "mis" : "", base,
					*(int *)base,
					nflag ? '!' : '=',
					value);
				if (!all) return (1);
			}
			base += size;
		}
		break;
#endif

	default:
		mem_error("mfind");
	}
	return(rc);
}

static void
mem_error (char *string)
{
    printf ("Error in memory debug command: %s\n", string);
}
