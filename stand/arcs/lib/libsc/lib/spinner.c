#include <stdarg.h>
#include <libsc.h>

static char spinchars[] = { '|', '/', '-', '\\' };
static int spin_interval;
static int spin_step;
static int spin_count;
 
void
start_spinner(int interval)
{
    spin_step = 0;
    spin_interval = interval;
    spin_count = 0;
    printf("   ");
}


void
spinner(void)
{
    if (++spin_count == spin_interval) {
	putchar(8);
	putchar(spinchars[spin_step++ % 4]);
	spin_count = 0;
    }
}

void
end_spinner(void)
{
    putchar(8);
    putchar(8);
    putchar(8);
    spin_step = 0;
}	 
