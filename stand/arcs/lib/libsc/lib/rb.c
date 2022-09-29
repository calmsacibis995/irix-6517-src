#ident	"$Revision: 1.13 $"

/*
 * rb.c - restart block manipulation
 */

#include <arcs/spb.h>
#include <arcs/restart.h>
#include <libsc.h>
#include <libsc_internal.h>

/*
 * NOTE: malloc must return memory in the firmware
 * permanent memory space
 */

extern int isvalid_rb (void);

static RestartBlock *create_rb (int);
void init_rb (void);

/*
 * init_rb - initialize the restart block for the calling processor,
 * allocating if necessary
 */
void
init_rb (void)
{
    RestartBlock *rb, *next;
    int cpu = getcpuid();

    if (((rb = get_cpu_rb (cpu)) == 0) && ((rb = create_rb (cpu)) == 0))
	return;

    next = rb->Next;
    /* bzero() clears RestartAddress, BootMasterID, and BootStatus */
    bzero (rb, sizeof (*rb));
    rb->Signature = RB_SIGNATURE;
    rb->Length = sizeof (*rb);
    rb->Version = RB_VERSION;
    rb->Revision = RB_REVISION;
    rb->Next = next;
    rb->ProcessorID = cpu;
    rb->SSALength = SSA_LENGTH;
    rb->Checksum = checksum_rb (rb);
}

#if 0			/* currently unused */
/*
 * save_rb - saves the current environment into the restart block
 * for later recovery
 *
 * The state save area is layed out like this
 *
 * 	base	argc
 * 		pointer to argv[0]
 * 		pointer to argv[1]
 * 		pointer to argv[2]
 *		     ....
 * 		pointer to envp[0]
 * 		pointer to envp[1]
 * 		pointer to envp[2]
 *		     ....
 *		string space for env
 *		string space for argv
 *	top
 *
 * The argv and envp pointers grow up from the base, and
 * the string space grows down from the top.  If the envp 
 * pointers and the string space collide, then the full
 * environment will not be saved.
 *
 * Returns 0 on success, 1 on failure.
 */
int
save_rb (int argc, char **argv, char **envp)
{
    int *base;
    char *top, *cp;
    int len, size;
    RestartBlock *rb = get_rb();

    if (!rb)
	return 1;

    base = (int *)rb->SSArea;			/* first address after argc */
    top = (char *)&rb->SSArea[SSA_WORDS];	/* first address after array */

    /* first clear the SSA and set argc
     */
    bzero (base, rb->SSALength);
    *base++ = argc;

    /* next copy argv into the SSA
     */
    while (argc) {
	cp = *argv++;

	/* don't waste space on NULL pointers or NULL strings
	 */
	if (!cp || !(len = strlen(cp))) {
	    rb->SSArea[0]--;		/* one fewer args than expected */
	    continue;
	}

	/* total length needed for string includes NULL and
	 * pointer in argv
	 */
	size = len + sizeof(char *) + sizeof("\0");
	if ((unsigned int)base + size >= (unsigned int)top) {
	    rb->Checksum = checksum_rb (rb);
	    return 1;
	}

	/* copy argv string to save area and write pointer into argv array
	 */
	size = len + sizeof("\0");
	top -= size;
	strncpy (top, cp, size);
	*base++ = (int)top;

	--argc;
    }

    /* copy envp into the SSA
     */
    while (envp && (cp = *envp++)) {

	/* don't waste space on NULL strings
	 */
	if (!(len = strlen(cp)))
	    continue;

	/* total length needed for string includes NULL and
	 * pointer in argv
	 */
	size = len + sizeof(char *) + sizeof("\0");
	if ((unsigned int)base + size >= (unsigned int)top) {
	    rb->Checksum = checksum_rb (rb);
	    return 1;
	}

	/* copy env string to save area and write pointer into envp array
	 */
	size = len + sizeof("\0");
	top -= size;
	strncpy (top, cp, size);
	*base++ = (int)top;
    }

    /* finally, create a NULL termination for the env pointers
     */
    if ((unsigned int)base + sizeof(char *) >= (unsigned int)top) {
	    base[-1] = 0;
	    rb->Checksum = checksum_rb (rb);
	    return 1;
    }

    *base = 0;
    rb->Checksum = checksum_rb (rb);

    return 0;
}

/*
 * restore_rb - restore argc, argv, and envp from restart block
 */
int
restore_rb (int *argc, char ***argv, char ***envp)
{
    RestartBlock *rb = get_rb();

    if (!isvalid_rb())
	return 1;

    *argc = rb->SSArea[0];
    *argv = (char **)&rb->SSArea[1];
    *envp = (char **)&rb->SSArea[*argc+1];
    return 0;
}
#endif	/* 0 */

/*
 * create_rb - allocate memory for a restart block and link it onto the list
 */
/*ARGSUSED*/
static RestartBlock *
create_rb (int cpu)
{
    RestartBlock *rb;

    rb = (RestartBlock *)malloc(sizeof(*rb));
    if (rb == 0)
	return 0;

    rb->Next = (RestartBlock *)SPB->RestartBlock;
    SPB->RestartBlock = (LONG *)rb;

    return rb;
}

/* set and clear restart block's boot status field -- used by prom */
void
rbsetbs(int flag)
{
	RestartBlock *rb = (RestartBlock *)SPB->RestartBlock;

	if (rb != NULL) {
		rb->BootStatus |= flag;
		rb->Checksum = checksum_rb(rb);
	}
}

/*
 * get_cpu_rb - search the restart block list for one with the given cpu number
 */
RestartBlock *
get_cpu_rb (int cpu)
{
    RestartBlock *rb = (RestartBlock *)(SPB->RestartBlock);
    while (rb != 0) {
	if (rb->ProcessorID == cpu)
	    return rb;
	rb = rb->Next;
    }
    return 0;		/* search failed */
}

/*
 * checksum_rb - signed checksum of restartblock that adds block to 0
 */
int
checksum_rb (RestartBlock *rb)
{
    int sum, *ptr;

    for (sum = 0, ptr = (int *)rb;
	    ptr < (int *)(rb + sizeof(rb) - sizeof(rb->Checksum)); ptr++)
	sum += *ptr;
    sum =  -sum;
    return sum;
}
