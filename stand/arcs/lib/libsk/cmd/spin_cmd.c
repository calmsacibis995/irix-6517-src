#ident	"lib/libsc/cmd/spin_cmd.c:  $Revision: 1.17 $"

/*
 * spin -- repeat memory reference pattern
 */

#include <setjmp.h>
#include <sys/sbd.h>
#include <libsc.h>
#include <libsk.h>

#define	MAXSPINADDRS	10

enum spintype { READ, WRITE, READWRITE, WRITEREAD };

struct spinhow {
	__psunsigned_t addr;
	unsigned int	width;
	enum spintype	spintype;
	unsigned int	value;
	int		count;
};

static void bytespin(char *, enum spintype, unsigned int, int),
    halfwordspin(short *, enum spintype, unsigned int, int),
    wordspin(int *, enum spintype, unsigned int, int);

int
spin(argc, argv)
	int argc;
	char **argv;
{
	struct spinhow spinhow[MAXSPINADDRS+1];
	register struct spinhow *sp = spinhow;
	auto unsigned int value = 0;
	auto int count = 1;
	int vid_me = cpuid();

	if (argc < 2)
		return -1;
	while (--argc > 0 && (*++argv)[0] == '-') {
		register char *ap;

		for (ap = *argv + 1; *ap; ap++) {
			switch (*ap) {
			  case 'c':
				if (--argc < 1 || *atob(*++argv, &count))
					return -1;
				break;

			  case 'v':
				if (--argc < 1 || *atobu(*++argv, &value))
					return -1;
				break;

			  case 'r':
			  case 'w':
				if (sp >= &spinhow[MAXSPINADDRS]) {
					printf("address table filled\n");
					return 0;
				}
				if (--argc < 1)
					return -1;
				if (*atobu_ptr(*++argv, &sp->addr))
					return -1;
				switch (*ap) {
				  case 'r':
					sp->spintype = READ;
					break;
				  case 'w':
					sp->spintype = WRITE;
					break;
				  default:
					return -1;
				}
				if (ap[1] == '+') {
					sp->spintype = (sp->spintype == READ) ?
					    READWRITE : WRITEREAD;
					ap++;
				}
				switch (*++ap) {
				  case 'b':
					sp->width = SW_BYTE;
					break;
				  case 'h':
					sp->width = SW_HALFWORD;
					break;
				  case '\0':
					--ap;	/* FALL THROUGH */
				  case 'w':
					sp->width = SW_WORD;
					break;
				  default:
					return -1;
				}
				sp->value = value;
				sp->count = count;
				sp++;
				break;

			  default:
				return -1;
			}
		}
	}
	if (argc != 0)
		return -1;
	sp->width = 0;		/* sentinel */

	for (sp = spinhow; sp->width; sp++) {
		jmp_buf faultbuf;
/* cannot cache PROM space in IP20 */
#if defined(IP20)
#define dispatch(spinner, sp) \
	((*(int (*)(__psunsigned_t, \
		    enum spintype, \
		    unsigned int, \
		    int))spinner)(sp->addr, sp->spintype, sp->value, \
	    sp->count))
#else
#define dispatch(spinner, sp) \
	((*(int (*)(__psunsigned_t, \
		    enum spintype, \
		    unsigned int, \
		    int))K1_TO_K0(spinner))(sp->addr, sp->spintype, sp->value, \
	    sp->count))
#endif	/* IP20 */

		if (setjmp(faultbuf))
			mp_show_fault(vid_me);	/* retry after an exception */
		set_nofault(faultbuf);

		switch (sp->width) {
		  case SW_BYTE:
			dispatch(bytespin, sp);
			break;
		  case SW_HALFWORD:
			dispatch(halfwordspin, sp);
			break;
		  case SW_WORD:
			dispatch(wordspin, sp);
			break;
		}
		clear_nofault();
#undef	dispatch
	}
	return 0;
}

/*
 * Generic spin function template.  There are several points of interest:
 *
 *  (1)	We use three if statements instead of a switch on spintype, because
 *	dense switches get compiled into jump tables, and we don't want any
 *	memory references other than to addr.
 *
 *  (2)	The *(volatile unsigned Type *)addr forces a load from addr even
 *	with optimization.  MIPS current compilers load into the hard-wired
 *	zero register, and then mask off any unused high bytes of that
 *	register to defeat sign-extension.  Perhaps the optimizer will
 *	remove this useless masking...
 *
 *  (3) As with the MIPS version, a negative count means spin forever.
 */
#define	DEFINE_SPINNER(Name, Type)				\
static void							\
Name(Type *addr, enum spintype spintype, unsigned int value, int count)	\
{								\
	for (;;) {						\
		if (count == 0)					\
			return;					\
		if (spintype == WRITE || spintype == WRITEREAD)	\
			*addr = (Type)value;			\
		if (spintype != WRITE) {			\
			*(volatile unsigned Type *)addr;	\
			if (spintype == READWRITE)		\
				*addr = (Type)value;		\
		}						\
		if (count > 0)					\
			--count;				\
	}							\
}

DEFINE_SPINNER(bytespin, char)
DEFINE_SPINNER(halfwordspin, short)
DEFINE_SPINNER(wordspin, int)

