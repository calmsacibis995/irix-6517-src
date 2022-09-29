
#define SEG0STACK_SIZE	0x10000


/* By calling the epcuart I/O routines directly the user can select between
 * channel A and channel B.
 */
#define CHN_A   (EPC_DUART2 + 0x8)
#define CHN_B   (EPC_DUART1 + 0x0)

#define CTRL(_x)	((_x) & 0x1f)
#define INTR		CTRL('C')
#define DEL             0x7f
#define BELL            0x7
#define isprint(c)	((c >= 0x20) && (c <= 0x7e) || c == 0x09 || c == 0x08)
#define isdigit(c)	((c >= '0') && (c <= '9'))

#define MAGIC_KEY	CTRL('L')

#if _LANGUAGE_ASSEMBLY

#include <sys/regdef.h>

#define NOFAULT_REG	$f1

#define R4K_SR_BEV		0x00400000	/* use boot exception vectors */
#define R4K_SR_KX		0x00000080	/* extended-addr TLB vec in kernel */

#endif /* _LANGUAGE_ASSEMBLY */

#if _LANGUAGE_C

#include <sys/types.h>
#include <sys/EVEREST/promhdr.h>
#include <stdarg.h>

/*
 * Nicer ways to refer to the above flags.
 */

#define SYSCTL	(sl_flags & SL_FLAG_SYSCTL)

char epcuart_getc(int);
void epcuart_flush(int);
void epcuart_putc(char, int);
int epcuart_poll(int);
int epcuart_puthex(__int64_t, int);
int epcuart_puts(char *, int);

char ccuart_getc(void);
void ccuart_flush(void);
void ccuart_putc(char);
int ccuart_poll(void);
int ccuart_puthex(__int64_t);
int ccuart_puts(char *);

char lo_getc(void);
void flush(void);
void lo_putc(char);
int poll(void);
int puthex(__int64_t);
int lo_puts(char *);

int getprid(void);
__int64_t getsp(void);
void jump_addr(void *);
__uint32_t load_lwin_half_store_mult(__int64_t, __int64_t, __int64_t, uint);

int which_segtype(int, int);
int which_segnum(seginfo_t *, int);
char *which_segname(int, int);
char *seg_string(int);
int get_segtable(seginfo_t *, int);
int get_promhdr(evpromhdr_t *, int);
int load_segment(promseg_t *, int);
void list_segments(seginfo_t *);
int load_and_exec(promseg_t *, int);
int get_window(int);

__uint32_t copy_prom(__int64_t, void *, __uint32_t, int);

void loprf(void (*putc)(char), char *, va_list);
void lo_sprintn(char *, __scunsigned_t, int, int);
char *logets(char *, int);
void loprintf(char *, ...);
void loputs(void (*putc)(char), char *);
void loputchar(char);
ulong lo_atoh(char *);

void do_menu(seginfo_t *);

void sysctlr_message(char *);



extern char *hexdigit;
extern char *crlf;

extern char *getversion(void);

/* Processor specific functions */

extern	void	R4000InitSerialPort(void);
extern	void	R8000InitSerialPort(void);
extern	void	R10000InitSerialPort(void);

#endif /* __LANGUAGE_C */
