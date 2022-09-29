/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved.       *
 *                                                                          *
 *    DO NOT DISCLOSE THIS SOURCE CODE OUTSIDE OF SILICON GRAPHICS, INC.    *
 *  This source code contains unpublished proprietary information of        *
 *  Ordinal Technology Corp.  The copyright notice above is not evidence    *
 *  of any actual or intended publication of this information.              *
 *      This source code is licensed to Silicon Graphics, Inc. for          *
 *  its personal use in developing and maintaining products for the IRIX    *
 *  operating system.  This source code may not be disclosed to any third   *
 *  party outside of Silicon Graphics, Inc. without the prior written       *
 *  consent of Ordinal Technology Corp.                                     *
 ****************************************************************************
 *
 *
 *	ordutils.c	- utility time measurement/debug functions
 *
 *	$Ordinal-Id: ordutils.c,v 1.22 1996/11/06 23:12:16 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/times.h>
#include <sys/prctl.h>
#include <sys/syssgi.h>
#include <sys/immu.h>


char *Ordinal_Title = "<current program>";
int Ordinal_Main;		/* pid of main program */
void (*Ordinal_Cleanup)(void);
byte *OrdinalPinStart;
byte *OrdinalPinEnd;

unsigned get_time(void)
{
    struct tms	buf;

    return (times(&buf));
}

/* get_time_us	- return elapsed time since boot epoch in microseconds,
 *		  wrapping every 4000 seconds, okay as long any single
 *		  microsecond timed interval takes less than an hour or so.
 */
unsigned get_time_us(void)
{
    static volatile byte *iotimer_addr = 0;
    static float	scale;
    static int		timer_is_ull;
    unsigned		counter_value;
    unsigned		cycleval;
    ptrdiff_t		phys_addr, raddr;
    int			poffmask;
    int			fd;

    if (iotimer_addr == 0)
    {
#if defined(SGI_CYCLECNTR_SIZE)
	timer_is_ull = syssgi(SGI_CYCLECNTR_SIZE) == 64;
#else
	timer_is_ull = FALSE;
#endif
	phys_addr = syssgi(SGI_QUERY_CYCLECNTR, &cycleval);
	if ((int) phys_addr == -1)
	{
	    if (errno == ENODEV)
		die("SGI_QUERY_CYCLECNTR not supported on this machine\n");
	    else
		die("SGI_QUERY_CYCLECNTR");
	}
	poffmask = getpagesize() - 1;

	raddr = phys_addr & ~poffmask;
	fd = open("/dev/mmem", O_RDONLY);
	iotimer_addr = (volatile byte *) mmap(0, poffmask, PROT_READ,
					      MAP_PRIVATE, fd,
					      (__psint_t) raddr);
	iotimer_addr = iotimer_addr + (phys_addr & poffmask);
	/* to convert counter from pico seconds/count to #microseconds
	 */
	scale = cycleval / 1000000.0;
    }

    counter_value = scale * (timer_is_ull ? *((u8 *) iotimer_addr)
					  : *((u4 *) iotimer_addr));
    return ((unsigned) counter_value);
}

unsigned get_cpu_time(void)
{
    struct tms	buf;

    times(&buf);
    return (buf.tms_utime + buf.tms_stime);
}

unsigned get_relative_time()
{
    static unsigned	last_time;
    unsigned		curr_time, diff_time;
    struct tms		buf;

    curr_time = times(&buf);
    diff_time = curr_time - last_time;
    last_time = curr_time;
    return (diff_time);
}

char *get_time_str(sort_t *sort, char *buf)
{
    unsigned	diff;

    diff = get_time() - TIMES.begin;
    sprintf(buf, "%2d.%02d", diff / 100, diff % 100);
    return (buf);
}

/*
 * get_scale - look for a scaling suffix in **num_end
 *		no white space is allowed between the number and the scale
 *
 *		returns 1, 1K, 1M, or 1G
 */
int get_scale(const char **num_end, const char *scales)
{
    int scale = 1;
    char suffix = tolower(**num_end);

    if (strchr(scales, suffix))
    {
	switch (suffix)
	{
	  case 'k':
	    scale = 1024;
	    (*num_end)++;
	    break;

	  case 'm':
	    scale = 1024 * 1024;
	    (*num_end)++;
	    break;

	  case 'g':
	    scale = 1024 * 1024 * 1024;
	    (*num_end)++;
	    break;
	}
    }
    return (scale);
}

unsigned long get_number(const char *str)
{
    unsigned long	value;
    unsigned		scale;
    const char		*num_end;

    value = strtoul(str, (char **) &num_end, 10);
    scale = get_scale(&num_end, "kmg");
    if (scale == 1 && *num_end != '\0')
    {
	fprintf(stderr, "Unrecognized scaling suffix for: %s\n", str);
	exit(1);
    }
    if (value > ULONG_MAX / scale)
    {
	fprintf(stderr, "The number %s would be too large\n", str);
	exit(1);
    }

    return (value * scale);
}

/*
** print_number	- convert an unsigned integer to a text representation
**		  with commas for readability if n >= 10,000
*/
char *print_number(unsigned n)
{
    static char	buf[20];

    if (n >= 1000*1000*1000)
	sprintf(buf, "%d,%03d,%03d,%03d", n/1000000000, (n/1000000) %1000,
					  (n / 1000) % 1000, n % 1000);
    if (n >= 1000*1000)
	sprintf(buf, "%d,%03d,%03d", n/1000000, (n / 1000) % 1000, n % 1000);
    else if (n >= 10*1000)	
	sprintf(buf, "%d,%03d", n / 1000, n %1000);
    else
	sprintf(buf, "%d", n);
    return (buf);
}

void ordinal_debug(void)
{
    char	*ordinal = getenv("ORDINAL_POSTMORTEM");

#if 0
    /* Unpin memory so we can use the debugger without paging to death
     */
    if (Ordinal_Cleanup)
	(*Ordinal_Cleanup)();
#endif

    if (ordinal)
    {
	if (strcmp(ordinal, "hang") == 0)
	{
	    int	loop = TRUE;

	    suspend_others();
	    while (loop)
		sleep(30);
	}
	else if (strcmp(ordinal, "pause") == 0)
	{
	    fprintf(stderr, "\a\a");
	    sleep(60);			/* give time to attach with debugger */
	}
	else
	    kill(getpid(), SIGABRT);	/* dump core of dying process */
    }
}

/*
 * ordinal_exit	- call this function to cause all of nsort's sprocs to exit
 *		  (including the i/o, sorting, and aio sprocs)
 */
void ordinal_exit(void)
{
#if defined(DEBUG2)
    fprintf(stderr, "ordinal_exit of %d\n", getpid());
#endif

    if (Ordinal_Cleanup && getpid() == Ordinal_Main)
	(*Ordinal_Cleanup)();
	
    /* Send the "exit yourself nicely" signal to all this share group's sprocs
     * when this one exits.  That tells them to exit without returning
     * an error termination status back to the shell.
     */
    prctl(PR_SETEXITSIG, SIGUSR1);
    signal(SIGUSR1, SIG_IGN);
    exit(3);
}

/*
 * die	- describe an internal logic error, and kill off the sort
 *
 * XXX This must be done differently for the subroutine library version
 */
void die(const char *fmt, ...)
{
    va_list	args;
    char	*ordinal = getenv("ORDINAL_POSTMORTEM");

    fflush(stdout);
    fprintf(stderr, "\n%s", Ordinal_Title);
    if (ordinal)
	fprintf(stderr, " (process %d)", getpid());
    fprintf(stderr, " detected a fatal error:\n");
    if (fmt != NULL)
    {
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fputc('\n', stderr);
    }

    ordinal_debug();

    ordinal_exit();
}

/*
 * sigdeath	- signal handler which gives notice and a last chance to debug
 *
 * XXX This must be done differently (not at all?) in the subroutine lib version
 *
 *
 *  To find out where a seg fault, etc, really happened (with a stack trace,
 *  for instance), do the following:
 *	1- step or return out of the call to sleep
 *	2- jump to the call to setcontext
 *  The program will restart the instruction at which the signal was raised.
 */

void sigdeath(int sig, siginfo_t *pinfo, ucontext_t *ucp)
{
    char mesg[70];

    fflush(stdout);
    sprintf(mesg, "Signal in %s sproc %d 0x%x", Ordinal_Title, getpid(), 
	ucp->uc_mcontext.gregs[NGREG - 1]); /* pc appears to be last gen reg? */

    psiginfo(pinfo, mesg);
    ordinal_debug();

    /* Send a kill to all this share group's sprocs when this one exits
     */
    prctl(PR_SETEXITSIG, SIGKILL);
    exit(3);

    setcontext(ucp);
}

/*
** catch_signals	- install a handler over a range of signals,
**			  if the signal isn't already handled.
**
**	The installed signal handling specifies the SA_SIGINFO bit,
**	which tells the system to give the handler more information
**	about the source of the signal.  sigdeath() display the extra info.
*/
void catch_signals(int first, int last, SIG_PF handler)
{
    struct sigaction	sigact;

    sigact.sa_flags = 0;	/* XXX should cont. syscalls? */
    sigact.sa_handler = ordinal_exit;
    sigemptyset(&sigact.sa_mask);
    if (sigaction(SIGUSR1, &sigact, NULL) < 0)
	die("catch_signals: set exit: %s", strerror(errno));
    while (first < last)
    {
	if (first != SIGEMT && first != SIGKILL && first != SIGSTOP)
	{
	    if (sigaction(first, NULL, &sigact) < 0)
		die("catch_signals: get %d %s", first, strerror(errno));
	    if (sigact.sa_handler == SIG_DFL)
	    {
		/* Block hangup (prctl(setexitsig)) and usr1 (ordinal_exit)
		 * during the activation of this handler so that it gets to
		 * complete its actions (e.g. nsort_cleanup()->rm_temps())
		 */
		sigact.sa_flags |= SA_SIGINFO;
		sigaddset(&sigact.sa_mask, SIGUSR1);
		sigaddset(&sigact.sa_mask, SIGHUP);
		sigact.sa_handler = handler;
		if (sigaction(first, &sigact, NULL) < 0)
		    die("catch_signals: set %d %s", first, strerror(errno));
	    }
	}
	first++;
    }
}


void assert_failed(char *expr, char *file, int line)
{
    die("Assertion %s failed: %s line %d", expr, file, line);
}


int compare_and_swap(mpcount_t *p, mpcount_t old, mpcount_t new, usptr_t *u)
{
    /* keep trying a compare and swap until it either suceeds or
     * or the value is change by some other process.
     */
    do
    {
	if (uscas((ptrdiff_t *) p, old, new, u))
	    return (TRUE);

	/* the failure could have been cause by an ill-timed interrupt,
	 * try again as long as the old value is still present.
	 */
    } while (*p == old);


    /* value is no longer the old value and we didn't change it.  
     * return failure.
     */
    return (FALSE);
}

mpcount_t atomic_add(mpcount_t *p, ptrdiff_t inc, usptr_t *u)
{
    mpcount_t	old;

    /* keep trying to increment until the compare and swap succeeds
     */
    for (;;)
    {
	old = *p;
	if (uscas((ptrdiff_t *) p, old, old + inc, u))
	    return (old + inc);
    }
}

#if defined(SHOWNAPS)
int ordinal_nap(long ticks , const char *title)
#else
int ordinal_nap(long ticks)
#endif
{
    int		ret;

#if 0
    static int	till_nap = 0;
    char	reschedstr[50];

    if (++till_nap != SYS.n_processors && ticks != 0)
    {
#if defined(SHOWNAPS)
	if (Print_task)
	{
	    sprintf(reschedstr, "%s (resched)", title);
	    print_total_time(SYS.sorts[0], title);
	}
#endif
	return ((int) sginap(0));
    }
    till_nap = 0;
#endif
    ret = ((int) sginap(ticks));
#if defined(SHOWNAPS)
    if (Print_task)
	print_total_time(SYS.sorts[0], title);
#endif
    return (ret);
}

/* sproc_hup	- handle a SIGHUP, see if the client process which started the
 *		  first sort has exited; if so we do too
 */
void sproc_hup(void)
{
    int	parent = getppid();

    if (parent == 1)
    {
#if defined(DEBUG2)
	fprintf(stderr, "Process %d exiting: parent gone\n", getpid());
#endif
	_exit(0);
    }
#if defined(DEBUG2)
    fprintf(stderr, "Process %d staying: parent %d still here\n", getpid(), parent);
#endif
}

void exit_with_parent(void)
{
    struct sigaction	sigact;

    sigact.sa_flags = 0;	/* XXX should cont. syscalls? */
    sigact.sa_handler = sproc_hup;
    sigemptyset(&sigact.sa_mask);
    if (sigaction(SIGHUP, &sigact, NULL) < 0)
	die("exit_with_parent: handle HUP failed: %s", strerror(errno));
    if (prctl(PR_TERMCHILD) < 0)
	die("exit_with_parent: prctl(TERMCHILD) failed: %s", strerror(errno));
}

/*
 * next_decimal_value	- get the next 8 digits of the decimal string,
 *			  pack them into 4 bytes
 *
 *	The string may contain a decimal point - a major complication because
 *	the ovc offset cannot be used as the index (in the string) of the next
 *	four bytes of the value. E.g.: a 10 byte decimal string may be
 *		.123456789
 *	or
 *		1234567890
 *	or
 *		    123.45
 *
 *	To handle this the ovc value code is computed as if the numbers were
 *	normalized by left-shifting so that the decimal point is at the right
 *	of the smallest number (multiplying by 10**(key_len-1)). This results in
 *						     
 *		.123456789 -> 0000 0000 00.12 3456 789
 *		  .1       -> 0000 0000 00.10 0000 000
 *		 1.2       -> 0000 0000 01.20 0000 000
 *		1234567890 -> 1234 5678 90.00 0000 000
 *		  123.45   -> 0000 0001 23.45 0000 000
 *		 123.4507  -> 0000 0001 23.45 0700 000
 *	Next_decimal_value() then extracts 8 digits from this conditioned value;
 *		for:	.123456789    1234567890   123.4507  b_so_far  first_t_f
 *	    first 8 are 0000 0000 and 1234 5678   0000 0001	0	0
 *	    next are	0012 3456 and 9000 0000.  2345 0700	4	8/9?
 *	    or (rsort)	0000 1234 and 7890 0000.  0123 4507	3	6/7?
 *	The value of bytes_so_far combines with the decimal point position to
 *	decide whether a particular next_decimal_value() has to do any/all of:
 *	a) return all leading zeroes (i.e. offset is before start of actual num)
 *	b) return some of the digits
 *	c) return some trailing zeroes (i.e. perform left digit shifts)
 *	d) return all trailing zeroes (i.e. offset is past end of actual number)
 *
 *	I tried to pack more digits into an ovc by multiplying by 10 for each
 *	digit, rather than 16.  This fails when bytes_so_far is not a multiple
 *	of digits_per_value (e.g. pointer sorting by (i1, decimal10)).
 *	The second ``hard'' recode will have bytes_so_far == 3, which is
 *	not aligned to a digit boundary. I am not yet sufficiently imaginative
 *	to figure out how to split a binary number such that ovcs work.
 */
u4 next_decimal_value(const byte *data, int len, int max_len, int bytes_so_far, unsigned digits_per_value)
{
    int		i;
    int		fetch_limit;
    int		before, after = 0;
    int		first_vis, last_vis;
    int		negative = FALSE;
    int		num_digits;
    int		first_to_fetch;	/* virtual conditioned digit index into num */
    int		previously_returned;
    int		left_shift;
    const byte	*orig_data = data;
    int		orig_len = len;
    u4		condition_mask;
    u4		ret;
    u4		sign_nibble;

    for (i = 0; i < len && isspace(data[i]); i++)
	continue;
    if (data[i] == '+')
	i++;
    else if (data[i] == '-')
    {
	/* Skip leading zeroes to help detecting the -0 case
	 */
	while (++i < len && data[i] == '0')
	    continue;
	/* Avoid setting 'negative' if we have already determined that the
	 * value of the field is zero, specifically -0[...] w/o a decimal point
	 */
	if (i < len && (isdigit(data[i]) || data[i] == '.'))
	    negative = TRUE;
    }

    data += i; len -= i;

    /* Find # of significant digits before and after the decimal point position
     */
    for (i = 0; i < len; i++)
    {
	if (isdigit(data[i]))
	    continue;
	if (data[i] == '.')
	{
	    while ((i + after + 1) < len && isdigit(data[i + after + 1]))
		after++;
	    if (negative && i == 0)
	    {
		/* see if all the 'after' digits are also zero, if so
		 * clear negative so that -0 gets the same recode value as +0
		 */
		while (data[i + after] == '0')
		    after--;
		if (after == 0)
		    negative = FALSE;
	    }
	}
	break;
    }
    before = i;

    ret = 0;
    /* We'll xor the accumulated digit string with this conditioning mask so
     * that for negative numbers the value decreases as the magnitude increases
     *	 -33 < -9 < -3
     *  ~x21 ~x09 ~x03
     *	0xde 0xf6 0xfc
     */
    condition_mask = negative ? ~0 : 0;
    sign_nibble = 0;
    if (bytes_so_far == 0)	/* make 'space' for the sign in first value */
    {
	digits_per_value--;
	condition_mask >>= 4;	
	sign_nibble = negative ? 0xa0000000 : 0xf0000000;
	first_to_fetch = 0;
    }
    else
	first_to_fetch = (bytes_so_far << 1) - 1;

    /* There are two digits per byte of ovc, except for the first byte
     * which starts with the sign nibble
     */
    first_vis = max_len - before;
    if (first_to_fetch + digits_per_value <= first_vis)	/* (a) */
	return (sign_nibble | condition_mask);
    last_vis = max_len + after;
    if (first_to_fetch >= last_vis)			/* (d) */
	return (condition_mask);	/* sign_nibble always is 0 here*/

    num_digits = before + after;

    /* Figure out how many significant digits have been returned already
     */
    previously_returned = first_to_fetch - first_vis;
    /* skip over the bytes in front of first_to_fetch, if any */
    if (previously_returned > 0)
    {
	data += previously_returned;
	if (previously_returned > before)
	{
	    /* We must be in the 'after' part of the number now.
	     * Complain if we didn't skip over a decimal point.
	     */
	    ASSERT(memchr(data-previously_returned,'.',previously_returned+1));
	    data++;
	}
    }
    else
	previously_returned = 0;

    /* Figure out how many digits to return in this call. We may reach the
     * end of the digit string; if so we may shift these digits up so that
     * there are in the correct position.
     */
    fetch_limit = min((first_to_fetch + digits_per_value) - first_vis, digits_per_value);
    left_shift = 0;
    if (fetch_limit > (num_digits - previously_returned))
    {
	left_shift = fetch_limit - (num_digits - previously_returned);
	fetch_limit = num_digits - previously_returned;
    }
    if (fetch_limit > 0)
    {
	for (i = 0; i < fetch_limit; i++)
	{
	    if (data[i] == '.')
		data++;	
	    ASSERT(isdigit(data[i]));
	    ret = (ret << 4) + (data[i] - '0');
	}

	ASSERT(left_shift < digits_per_value);
	if (digits_per_value <= 6)	/* XXX ugh record sort shifts more */
	    left_shift += 2;
	ret <<= left_shift << 2;	/* left digit 'shifts' */
    }

    ret |= sign_nibble;
#if defined(DEBUG1)
    if (Print_task)
	fprintf(stderr, "next_decimal: <%.*s> bef:%d aft:%d "
			"first %d lim %d shift %d ret 0x%x\n",
			orig_len, orig_data, before, after,
			first_to_fetch, fetch_limit, left_shift, ret);
#endif
    return (ret ^ condition_mask);
}

/*
 * ulp	- unaligned load pointer, function version. see decl in ordinal.h
 */
#undef ulp
byte *ulp(void *ptr)
{
    return (UNALIGNED_PTR(ptr));
}
