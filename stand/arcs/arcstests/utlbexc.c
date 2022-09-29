#ident	"arcstests/utlbexc.c:  $Revision: 1.3 $"

#include <arcs/spb.h>
#include <fault.h>

void connect_handlers(void);
void disconnect_handlers(void);
void fault_stack_init(void);

/*
 * utlbexc.c - test interrupt handling of arcs prom
 */

main()
{
    char ch;
    int nread;
    int dummy;

    printf ("Testing arcs prom utlb exception handling...\n");

    /* create and initialize a fault stack
     */
    fault_stack_init ();

    /* put local exception handlers in the SPB
     */
    connect_handlers ();

    /* Generate an exception and see if
     * we can continue execution
     */
    dummy = *(volatile int *)0;
    
    /* restore original exception handlers
     */
    disconnect_handlers ();

    printf ("Test complete [hit any key to continue] >> ");
    while (Read (0, &ch, 1, &nread) || (nread != 1))
	;
    printf ("\n");

    return;
}

long *saved_norm_vec;
long *saved_utlb_vec;

extern int my_norm_vec(), my_utlb_vec();

void
connect_handlers (void)
{
    /* Save the old... */
    saved_norm_vec = SPB->GEVector;
    saved_utlb_vec = SPB->UTLBMissVector;

    /* ...and connect the new. */
    SPB->GEVector = (long *)my_norm_vec;
    SPB->UTLBMissVector = (long *)my_utlb_vec;
}

void
disconnect_handlers (void)
{
    /* Replace the new with the old */
    SPB->GEVector = saved_norm_vec;
    SPB->UTLBMissVector = saved_utlb_vec;
}

static int fault_stack[512];

void
fault_stack_init (void)
{
    _fault_sp = (int)fault_stack;
    _stack_mode = MODE_NORMAL;
}

void
my_exception_handler (void)
{
    extern int _regs[];

    printf ("Exception!\n");
    _regs[R_EPC] += 4;		/* bump pc over offending instruction */
    return;
}
