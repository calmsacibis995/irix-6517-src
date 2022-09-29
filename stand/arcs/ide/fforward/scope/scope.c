/*
 * scope.c -- consistently write to module so signals can be scoped.
 */
#ident	"$Revision: 1.11 $"

#include <saioctl.h>
#include <setjmp.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <arcs/types.h>
#include <arcs/signal.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>

#if R4000
extern int swap_cache(void);
extern int unswap_cache(void);
#endif

static int catch_intr(void);
static jmp_buf catch;

struct scope_test {
	int (*init)(void);	/* function to run before scoping */
	int (*done)(void);	/* function to clean up after scoping */
	__psunsigned_t addrlo;	/* low address to probe */
	__psunsigned_t addrhi;	/* hi address to probe */
	int size;		/* size of memory location (bytes) */
	char *name;		/* name of tested module */
} st[] = {
	{0, 0, PHYS_TO_K1(0x300), PHYS_TO_K1(0x3fc), 4, "memory"},
#if R4000
	{swap_cache, unswap_cache, PHYS_TO_K0(0x4000), PHYS_TO_K0(0x5ffc),
		4, "icache"},
#endif
	{0, 0, PHYS_TO_K0(0x4000), PHYS_TO_K0(0x5ffc), 4, "dcache"},
	{0, 0, PHYS_TO_K1(SCSI0A_ADDR), PHYS_TO_K1(SCSI0A_ADDR), 1, "scsi"},
	{0, 0, PHYS_TO_K1(0x1fb8005b), PHYS_TO_K1(0x1fb8005b), 1, "enet"},
	{0, 0, (__psunsigned_t)(RT_CLOCK_ADDR)+0x54,
	       (__psunsigned_t)(RT_CLOCK_ADDR)+0x54, 1, "rtc"},
#if IP22 || IP26 || IP28
	{0, 0, DUART1A_CNTRL, DUART1B_CNTRL, 1, "duart0"},
#elif IP20
	{0, 0, DUART1A_CNTRL, DUART0B_CNTRL, 1, "duart0"},
	{0, 0, DUART1A_CNTRL, DUART1B_CNTRL, 1, "duart1"},
#endif
	{0, 0, 0, 0, 0, ""},
};

static struct scope_test *sp = &st[0];

int
scope (int argc, char **argv)
{
    SIGNALHANDLER prev_handler;
    extern int *_intr_jmpbuf;
    volatile int bucket;

    if (argc != 2)
	return (-1);

    for (sp = &st[0]; *sp->name != '\0'; sp++)
	if (!strcmp (sp->name, argv[1]))
	    break;

    if (*sp->name == '\0')
	return (-1);

    printf ("Scoping %s: 0x%x and 0x%x  ", sp->name, sp->addrlo, sp->addrhi);
    printf (" (^C to abort)...\n");

    prev_handler = Signal (SIGINT, (void (*)()) catch_intr);
    busy (0);
    if (!setjmp(catch)) {
	if (sp->init)
	    (sp->init)();
	while (1) {
	    switch (sp->size) {
		case 1:
		    *(volatile char *)(sp->addrlo) = 0xaa;
		    bucket = *(volatile char *)(sp->addrlo);
		    *(volatile char *)(sp->addrhi) = 0xaa;
		    bucket = *(volatile char *)(sp->addrhi);
		    *(volatile char *)(sp->addrlo) = 0x55;
		    bucket = *(volatile char *)(sp->addrlo);
		    *(volatile char *)(sp->addrhi) = 0x55;
		    bucket = *(volatile char *)(sp->addrhi);
		    break;
		case 2:
		    *(volatile short *)(sp->addrlo) = 0xaaaa;
		    bucket = *(volatile short *)(sp->addrlo);
		    *(volatile short *)(sp->addrhi) = 0xaaaa;
		    bucket = *(volatile short *)(sp->addrhi);
		    *(volatile short *)(sp->addrlo) = 0x5555;
		    bucket = *(volatile short *)(sp->addrlo);
		    *(volatile short *)(sp->addrhi) = 0x5555;
		    bucket = *(volatile short *)(sp->addrhi);
		    break;
		case 4:
		    *(volatile int *)(sp->addrlo) = 0xaaaaaaaa;
		    bucket = *(volatile int *)(sp->addrlo);
		    *(volatile int *)(sp->addrhi) = 0xaaaaaaaa;
		    bucket = *(volatile int *)(sp->addrhi);
		    *(volatile int *)(sp->addrlo) = 0x55555555;
		    bucket = *(volatile int *)(sp->addrlo);
		    *(volatile int *)(sp->addrhi) = 0x55555555;
		    bucket = *(volatile int *)(sp->addrhi);
		    break;
	    }
	    busy (1);
	}
    } else {
	/* ends up here on ^C 
	 */
	Signal (SIGINT, prev_handler);
	bzero (catch, sizeof(jmp_buf));
	return 0;
    }
}

static int
catch_intr(void)
{
    if (sp->done)
	(sp->done)();
    busy (0);
    printf ("Done\n");
    longjmp (catch, 1);

    /*NOTREACHED*/
}
