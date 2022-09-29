#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/mace.h>
#include <sys/IP32flash.h>
#include <fault.h>
#include <setjmp.h>
#include <uif.h>
#include <libsc.h>
#include <libsk.h>
#include "sio.h"

#define TIMEOUT 10000
#define CRM_INT_STATUS 0xffffffff14000000
#define MACE_UST_MASK	0xffffffff

extern void flash_write_enable();

int
unlock_flash(void)
{
	int i, timeout, e_status;
	unsigned char nvram, buf[256];
	volatile unsigned long long *int_status = 
		(unsigned long long *)PC_INT_STATUS;
	volatile unsigned char *prom =
				(unsigned char *)PROM;
	unsigned char *p_sector = (unsigned char *)(PROM+0x7ff00);

	unsigned char prom_data;

	msg_printf(VRB, "Unlock FLASHi\n ****WARNING**** \nThis Program Will Destroy the \
				FLASH Prom if run >10,000 times\n");
	e_status = 0;
	

	flash_write_enable();
	/* save 256 bytes from a prom sector to write back at end of sequence */
	
	for (i=0;i<256;i++){
		buf[i]=p_sector[i];
	}
	prom[0x5555] = 0xaa;
	prom[0x2aaa] = 0x55;
	prom[0x5555] = 0x80;
	prom[0x5555] = 0xaa;
	prom[0x2aaa] = 0x55;
	prom[0x5555] = 0x20;
	for (i=0;i<256;i++)
		p_sector[i] = buf[i];
	us_delay(7000);
	timeout = 100;
	while (timeout > 0){
		if (p_sector[i] = buf[i] ){
			timeout = 0;
			e_status = 1;
		}
		timeout--;
		us_delay(1000);
	}
	if (!e_status){
	   msg_printf(ERR,"error read back %x, expected %x\n",p_sector[i],buf[i]);
	   return(1);
	}
	else
	   return(0);
}
void
flash_write_enable(void)
{
    unsigned char wrenreg;

    wrenreg = READ_REG64(FLASH_WENABLE, unsigned char);
    wrenreg |= FLASH_WP_OFF;
    WRITE_REG64(wrenreg, FLASH_WENABLE, unsigned char);
}

void
flash_write_disable(void)
{
    unsigned char wrenreg;

    wrenreg = READ_REG64(FLASH_WENABLE, unsigned char);
    wrenreg &= FLASH_WP_ON;
    WRITE_REG64(wrenreg, FLASH_WENABLE, unsigned char);
}
