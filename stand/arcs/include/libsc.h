/*
 * libsc.h - Standalone support library definitions and prototypes
 *
 * Where routines in libsc have POSIX or Standard C counterparts,
 * the standard syntax and semantics should be used.  An exception
 * to this are the buffered I/O routines.
 *
 * $Revision: 1.48 $
 */
#ifndef _LIBSC_H
#define _LIBSC_H

#include <sys/types.h>
#include <arcs/types.h>
#include <arcs/hinv.h>
#include <arcs/io.h>
#include <arcs/dirent.h>

struct RestartBlock;
struct buf;
struct cmd_table;
struct dmamap;
struct gui_obj;
struct htp_state;
struct pcbm;
struct range;
struct string_list;
struct volume_header;
struct in_addr;

#ifndef EOF
#define	EOF	(-1)
#endif
#ifndef NULL
#define NULL 0
#endif

/*
 * Memory reference widths
 */
#define	SW_BYTE		1
#define	SW_HALFWORD	2
#define	SW_WORD		4
#define	SW_DOUBLEWORD	8

/* Misc prototypes */
int _argvize(char *, struct string_list *);		/* lib/parser.c */
char *expand(char *, int);				/* lib/expand.c */
char *makepath(void);					/* lib/path.c */
int autoboot(int, char *, char *);			/* lib/auto.c */
void backtrace(int);					/* ml/btrace.s */
int token(char *, char *);				/* cmd/menu_cmd.c */
int efs_install(void);					/* fs/efs.c */
int xfs_install(void);					/* fs/xfs.c */
void updatetv(void);					/* lib/arcs_stubs.c */
extern int sgivers(void);				/* lib/privstub.c */

/* Restart Block -- lib/rb.c */
void rbsetbs(int);
void rbclrbs(int);

/* lib/scsaio.c */
void __close_fds(void);
void getack_andexit(void);
void panic(char *msg, ...);
int isgraphic(ULONG);
void close_noncons(void);
unsigned long get_tod(void);
void prcuroff(ULONG);

/* lib/dummyfunc.s (these routines do nothing) */
int preemptchk(void);
int dma_mapbp(struct dmamap *, struct buf *, int);
void splx(int);
int untimeout(toid_t);

/* lib/malloc.c */
void _init_malloc (void);
void *malloc(size_t);
void free(void *);
void *realloc (void *, size_t);
void *calloc(size_t, size_t);
void *kern_calloc(size_t, size_t);
void *dmabuf_malloc(size_t);
void dmabuf_free(void *);
void align_free(void *);
void *align_malloc(size_t, uint_t);

/* lib/mp.c */
int _get_numcpus(void);
void _init_libsc_private(void);

/* lib/libasm.s */
void debug(char *);
long get_sp(void);
long getpc(void);

/* lib/inetaddr.c */
struct in_addr inet_makeaddr(int, int);
unsigned int inet_network(char *);
unsigned int inet_lnaof(struct in_addr);
struct in_addr inet_addr(char *);
int inet_netof(struct in_addr);
char *inet_ntoa(struct in_addr);

/* lib/perror.c */
char *arcs_strerror(long);
void perror(long, char *);

/* lib/invfind.c */
char *inv_findcpu(void);
char *fixupname(COMPONENT *);
char *inv_findtape(void);
COMPONENT *find_type(COMPONENT *, CONFIGTYPE);
int count_type(COMPONENT *, CONFIGTYPE);

/* Standard I/O */
void _init_ttyputc(void (*)());
extern int getchar(void);
extern int getc(ULONG);
extern char *fgets(char *, int, ULONG);
extern char *gets(char *);
extern int putchar(int);
extern int putc(int, int);
extern int puts(const char *);
extern int printf(const char *, ...);
extern int vprintf(const char *, /* va_list */ char *);
extern int sprintf(char *, const char *, ...);
extern void sprintn(char *, __scunsigned_t, int);
extern int vsprintf(char *, const char *, /* va_list */ char *);
extern int ttyprintf(const char *, ...);
extern int vttyprintf(const char *, /* va_list */ char *);

extern void cmn_err(int, char *, ...);
extern void panic(char *, ...);

/* Cute routines for waiting spinner */
extern void start_spinner(int);
extern void spinner(void);
extern void end_spinner(void);

/* Conversion */
extern char *	atob(const char *, int *);
extern char *	atob_ptr(const char *, __psint_t *);
extern char *	atob_L(const char *, long long *);
extern int	atoi(const char *);
extern void 	atohex(const char *, int *);
extern void 	atohex_L(const char *, long long *);
extern void 	atohex_ptr(const char *, __psunsigned_t *);
extern char *	atobu(const char *, unsigned *);
extern char *	atobu_ptr(const char *, __psunsigned_t *);
extern char *	atobu_L(const char *, unsigned long long *);
extern char *	btoa(int);

/* Environment */
extern int	setenv(char *, char *);
extern int	_setenv(char *, char *, int);
extern int	syssetenv(char *, char *);
extern char *	getenv(const char *);
extern char *	getpath(COMPONENT *);
extern char *	getargv(char *);

/* String functions */
extern int	strcasecmp(const char *, const char *);
extern char *	strcat(char *, const char *);
extern int	strcmp(const char *, const char *);
extern char *	strcpy(char *, const char *);
extern size_t	strlen(const char *);
extern char *	strdup (const char *);
extern char *	strstr(const char *, const char *);
extern int	strncasecmp(const char *, const char *, size_t);
extern int	strncmp(const char *, const char *, size_t);
extern char *	strncpy(char *, const char *, size_t);
extern char *	strncat(char *, const char *, size_t);
extern char *	index(const char *, int c);
extern char *	rindex(const char *, int c);
extern unsigned long long int strtoull(const char *, char **, int);

/* Memory functions */
extern void     bcopy(const void *, void *, size_t);
extern int      bcmp(const void *, const void *, size_t);
extern void     bzero(void *, size_t);
extern unsigned	_cksum1(void *, unsigned, unsigned);
extern unsigned	nuxi_s(unsigned short);
extern unsigned	nuxi_l(unsigned);
extern void	swapl(void *,unsigned);
extern void	swaps(void *,unsigned);

/* Display Functions */
extern void	p_offset(int);
extern void	p_indent(void);
extern void	p_clear(void);
extern void	p_cursoff(void);
extern void	p_curson(void);
extern void	p_panelmode(void);
extern void	p_textmode(void);
extern void	p_printf(const char *,...);
extern void	p_center(void);
extern void	p_ljust(void);
extern void	p_puts(char *);

/* EnterInteractiveMode() is used a lot w/o including restart.h */
extern void	EnterInteractiveMode(void);

/* Miscellaneous */
extern LONG	ioctl(ULONG,LONG,LONG);
extern int	isatty(ULONG);
extern int	_min(int,int);
extern int	_max(int,int);
extern int 	getcpuid(void);
#undef cpuid
extern int 	cpuid(void);

/* for SN0 */
#undef		splockspl
#undef		spunlockspl
int		splockspl(lock_t, int (*)(void));
void		spunlockspl(lock_t, int);

/* Misc commands */
int go_cmd(int, char **, char **, struct cmd_table *);	/* cmd/go_cmd.c */
int boot(int, char **, char **, struct cmd_table *);	/* cmd/boot_cmd.c */
int cat(int, char **, char **, struct cmd_table *);	/* cmd/cat_cmd.c */
int copy(int, char **, char **, struct cmd_table *);	/* cmd/copy_cmd.c */
int hinv(int, char **, char **, struct cmd_table *);	/* cmd/hinv_cmd.c */
int klhinv(int, char **, char **, struct cmd_table *);	/* cmd/sn0hinv_cmd.c */
int mfind(int, char **, char **, struct cmd_table *);	/* cmd/memdb_cmd.c */
int ls(int, char **, char **, struct cmd_table *);	/* cmd/ls_cmd.c */

/* cmd/mrboot_cmd.c */
int mrboot(int, char **, char **, struct cmd_table *);
int do_custom_boot(int, char**);

/* cmd/env.c */
int printenv_cmd(int, char **, char **, struct cmd_table *);
int setenv_cmd(int, char **, char **, struct cmd_table *);
int unsetenv_cmd(int, char **, char **, struct cmd_table *);

/* cmd/menu_cmd.c */
extern int token(char *, char *);

/* Hardware graph/SN0 private vectors */
char *kl_inv_find(void);
int kl_hinv(int, char **);
int kl_get_num_cpus(void);
int sn0_getcpuid(void);

/* lib/stdio.c */
extern void	showchar(int);
extern void	setscalable(int);
extern int	getscalable(void);

/* Global variables */
extern char **	environ;
extern int	Debug;
extern int	Verbose;

/* Structure definitions */
/*
 * bit field descriptions for printf %r and %R formats
 *
 * printf("%r %R", val, reg_descp);
 * struct reg_desc *reg_descp;
 *
 * the %r and %R formats allow formatted print of bit fields.  individual
 * bit fields are described by a struct reg_desc, multiple bit fields within
 * a single word can be described by multiple reg_desc structures.
 * %r outputs a string of the format "<bit field descriptions>"
 * %R outputs a string of the format "0x%x<bit field descriptions>"
 *
 * The fields in a reg_desc are:
 *	unsigned rd_mask;	An appropriate mask to isolate the bit field
 *				within a word, and'ed with val
 *
 *	int rd_shift;		A shift amount to be done to the isolated
 *				bit field.  done before printing the isolate
 *				bit field with rd_format and before searching
 *				for symbolic value names in rd_values
 *
 *	char *rd_name;		If non-null, a bit field name to label any
 *				out from rd_format or searching rd_values.
 *				if neither rd_format or rd_values is non-null
 *				rd_name is printed only if the isolated
 *				bit field is non-null.
 *
 *	char *rd_format;	If non-null, the shifted bit field value
 *				is printed using this format.
 *
 *	struct reg_values *rd_values;	If non-null, a pointer to a table
 *				matching numeric values with symbolic names.
 *				rd_values are searched and the symbolic
 *				value is printed if a match is found, if no
 *				match is found "???" is printed.
 *				
 */


/*
 * register values
 * map between numeric values and symbolic values
 */
struct reg_values {
	__scunsigned_t rv_value;
	char *rv_name;
};

/*
 * register descriptors are used for formatted prints of register values
 * rd_mask and rd_shift must be defined, other entries may be null
 */
struct reg_desc {
	k_machreg_t rd_mask;	/* mask to extract field */
	int rd_shift;		/* shift for extracted value, - >>, + << */
	char *rd_name;		/* field name */
	char *rd_format;	/* format to print field */
	struct reg_values *rd_values;	/* symbolic names of values */
};

/*
 * libsc's per-processor data.
 */
typedef union libsc_private_u {
	struct privdata_s {
		/* pointers used for buffering in stdio support */
		char		*fputbp;
		char		*fputbb;
		int		fputbs;
		int		fputcn;
		int		column;
		char		*sprintf_cp;
	} privdata;
	char	pad[128];	/* any reasonable power of 2 */
} libsc_private_t;

extern int stdio_init;
extern libsc_private_t *_libsc_private;

#define LIBSC_PRIVATE(x) \
    (_libsc_private[(stdio_init ? cpuid() : 0)].privdata.x)
#endif
