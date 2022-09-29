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

#define NIC_VERBOSE 1

int
check_eaddr(void)
{
	int i, timeout, e_status;
	unsigned char buf[8],bytes[2],tmp[3],p_bytes[6];

	msg_printf(VRB, "check NIC eaddr and crc\n");
	e_status = 0;
	run_cached();
	ds2502_get_eaddr(&buf);

	for (i=0;i<8;i++)
		msg_printf(DBG,"eaddr %02x = %02x\n",i,buf[i]);
#if 0
	CRC16 = 0;

	for (i = 5; i >= 0; i--)
		Crc16(buf[i]);

	bytes[0] = (unsigned char)~(CRC16 & 0xFF) & 0xFF;
	bytes[1] = (unsigned char)~((CRC16 & 0xFF00) >> 8) & 0xFF;

	if (buf[6] != bytes[0]){
		msg_printf(ERR,"eaddr CRC check failed, %02x | = %02x\n",buf[6],bytes[0]);
		e_status++;
	}
	if (buf[7] != bytes[1]){
		msg_printf(ERR,"eaddr CRC check failed, %02x | = %02x\n",buf[7],bytes[1]);
		e_status++;
	}
#endif

	if (buf[0] == 0xff){
		msg_printf(ERR,"NIC eaddr read failed\n");
		e_status++;
	}


	run_uncached();
	return(e_status);

}

#if 0
/*--------------------------------------------------------------------------
 *  Calculate a new CRC16 from the input data integer.  Return the current
 *  CRC16 and also update the global variable CRC16.
 */
static int oddparity[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

uint Crc16(int data)
{
    data = (data ^ (CRC16 & 0xff)) & 0xff;
    CRC16 >>= 8;

    if (oddparity[data & 0xf] ^ oddparity[data >> 4])
        CRC16 ^= 0xc001;

    data <<= 6;
    CRC16  ^= data;
    data <<= 1;
    CRC16  ^= data;

    return CRC16;
}
#endif


