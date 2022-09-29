#include "sl.h"
#include <sys/EVEREST/epc.h>

char 
lo_getc(void)
{
#if	SABLE
    return ccuart_getc();
#else
    return epcuart_getc(CHN_A);
#endif
}

void 
flush(void)
{
#if 	SABLE
    ccuart_flush();
#else
    epcuart_flush(CHN_A);
#endif
}

void 
lo_putc(char data)
{
#if	SABLE
    ccuart_putc(data);
#else
    epcuart_putc(data, CHN_A);
#endif
}

int 
poll(void)
{
#if	SABLE
    return ccuart_poll();
#else
    return epcuart_poll(CHN_A);
#endif
}

int 
puthex(__int64_t data)
{
#if 	SABLE
    return ccuart_puthex(data);
#else
    return epcuart_puthex(data, CHN_A);
#endif
}

int 
lo_puts(char *data)
{
#if	SABLE
    return ccuart_puts(data);
#else
    return epcuart_puts(data, CHN_A);
#endif
}


