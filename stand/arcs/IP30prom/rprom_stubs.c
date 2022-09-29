#ident	"IP30prom/rprom_stubs.c:  $Revision: 1.4 $"

/*
 * rprom_stubs.c -- rprom stubs
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <saio.h>
#include <arcs/io.h>
#include <arcs/time.h>
#include <arcs/hinv.h>
#include <arcs/cfgtree.h>
#include <arcs/errno.h>
#include <style.h>
#include <libsc.h>
#include <libsk.h>
#include <standcfg.h>

/****************/
/* arc_stubs */
/****************/

/****************/
/* arcsio */
/****************/

TIMEINFO *
GetTime(void)
{
    static TIMEINFO t;

    cpu_get_tod(&t);
    return(&t);
}

/****************/
/* stubs for scsaio.c */
/****************/

/*VARARGS1*/
void
panic(char *msg, ...)
{
	/*XXX*/
	pon_puts("PANIC");
	pon_puts((char *)msg);
	*(int *)0x00badadd00000000 = 1;
}

/**************/
/* asic stuff */
/**************/

void
cpu_install(COMPONENT *root)
{
    printf("cpu_install\n");
}

/* stdio */

LONG
Write(ULONG fd, void *cv, ULONG len, ULONG *cnt)
{
	char *c = cv;

	*cnt = len;
	if (fd == 1) {
	    while (len--)
		pon_putc(*c++);
	}
	return 0; 
}

/*
 * we wont support input but stub this for response query's from
 * sflash driver always return NULL (i.e. no)
 */
char *
fgets(char *buf, int len, ULONG fd)
{
	buf[0] = 0;
	buf[1] = 0;
	pon_putc(buf[0]);
	pon_puts("\r\n");
	return(NULL);
}
