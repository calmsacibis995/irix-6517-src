/*
 * Driver for the Dallas DS2502 1K bit Add-only memory.
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/mace.h>
#include <libsc.h>
#include <libsk.h>

/* #define NIC_DEBUG 1 */

#ifndef ISA_FLASH_NIC_REG
#define ISA_FLASH_NIC_REG 0x310000
#define ISA_NIC_DEASSERT  0x4
#define ISA_NIC_DATA      0x8
#endif

#define NIC_DATA_IS_ZERO     (!(*nic_reg & ISA_NIC_DATA))
#define NIC_DATA_IS_ONE      (*nic_reg & ISA_NIC_DATA)

/*
 * write a one bit to the DS2502.  This is done by deasserting the
 * signal on the data pin and waiting 60us, the maximum time for
 * the sampling window.
 */
#define WRITE_ONE() { deassert(8); usecwait(60); }

/*
 * write a zero bit to the DS2502.  This is done by deasserting
 * the signal on the data pin for 90us, which is in between the
 * minimum value of >60usec and the maximum value of <120us.  The
 * DS2502 samples the line starting after 15us and samples until
 * 60us.
 */
#define WRITE_ZERO() { deassert(90); }

#define CRC(reg,val)	(reg) = crc8[(reg)^(val)]

static uchar_t crc8[256] = {
	0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
      157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
       35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
      190,224,	2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
       70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
      219,133,103, 57,186,228,	6, 88, 25, 71,165,251,120, 38,196,154,
      101, 59,217,135,	4, 90,184,230,167,249, 27, 69,198,152,122, 36,
      248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
      140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
       17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
      175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
       50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
      202,148,118, 40,171,245, 23, 73,	8, 86,180,234,105, 55,213,139,
       87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
      233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
      116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53
};

volatile long long *nic_reg = (long long *)PHYS_TO_K1(ISA_FLASH_NIC_REG);
extern void usecwait(int);
extern void CRCengine(uint8_t, uint8_t*);
unsigned int CRC16;
uint Crc16(int);


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

/*
 * Deassert the data pin on the DS2502 for "usecs" microseconds.
 */
static void
deassert(int usecs)
{
    *nic_reg &= ~ISA_NIC_DEASSERT;
    usecwait(usecs);
    *nic_reg |= ISA_NIC_DEASSERT;
}

/*
 * this routine resets and detects the ds2502.  This is done
 * by deasserting the pin on the ds2502 for a period of time
 * ranging from 480us to 960us.  500 was chosen arbitrarily.
 * After this pulse has been transmitted, we then read the
 * pin looking for a 1->0 transition to occur.  Once this
 * pulse is seen we have successfully reset and detected
 * the presence of the ds2502.
 */
ds2502_init(void)
{
    int count = 150;

    deassert(500);
    usecwait(15);
    do
    {
	if (NIC_DATA_IS_ZERO)
	    break;
	usecwait(1);
    } while (--count);

    /*
     * !count indicates that the part did not pulse
     * the data line low. return failure.
     */
    if (!count) {
#ifdef NIC_DEBUG
	printf ("ds2502_init: presence pulse not detected\n") ;
#endif
	return 0;
    } else {
	usecwait(1000);
	return 1;
    }
}

/*
 * Write count bytes of data to the DS2502.  data is sent LSB, so
 * the contents of *data need to be in little endian order, LSB first.
 * data[0] is the least significant byte of data.
 */
void
ds2502_write(char *data, int count)
{
    int i, bit;
    char byte;

    for (i = 0; i < count; i++) {
	byte = data[i];
	for (bit = 0; bit < 8; bit++) {
	    if (byte & 0x1)
		WRITE_ONE()
	    else
		WRITE_ZERO()
	    byte = byte >> 1;
	}
    }
}

/*
 * read "count" bytes of data from DS2502.  Data is returned 
 * in little endian order.  
 *
 * In reviewing the doc on this thing it became clear that Trdv
 * was only valid for 15us after the line is deasserted, not that
 * it became valid after 15us.  We now spin for a bit before reading
 * the line to allow it to stabelize at 1 if it is going to float
 * high.
 */
void
ds2502_read(char *data, int count)
{
    int i, bit;

    for (i = 0; i < count; i++)
	data[i] = 0;

    for (i = 0; i < count; i++) {
	for (bit = 0; bit < 8; bit++) {
	    deassert(8);
	    usecwait(1);
	    data[i] |= ((*nic_reg & ISA_NIC_DATA) != 0) << bit;
	    usecwait(100);
	}
    }
}

/*
 * read serial number from the ds2502 ROM and verify
 * it by checking the CRC for the ROM.  Return 0 if
 * if the read fails, 1 if the serial number is successfully
 * read.
 */
int
ds2502_read_rom(char *data, int count)
{
    int i, bit;
    uint8_t	crc;
    char id, cksum;

    /*
     * issue read rom command.
     */
    id = 0x33;
    ds2502_write(&id, 1);

    /*
     * read the family ID.
     */
    ds2502_read(&id, 1);

    /*
     * check for valid family ID.
     */
    if ((id & 0xff) != 0x9) {
#ifdef NIC_DEBUG
	printf("ds2502_read_rom: bad family id %x\n", id);
#endif
	return 0;
    }

    /*
     * read ethernet address (48 bit serial number).
     */
    ds2502_read(data, count);

    /*
     * read the CRC byte.
     */
    ds2502_read(&cksum, 1);

    crc = 0;
    for (bit = 0; bit < 8; bit++)
	CRCengine((id >> bit) & 1, &crc);

    for (i = 0; i < count; i++)	    /* verify part serial number is valid */
    	for (bit = 0; bit < 8; bit++)
	    CRCengine((data[i] >> bit) & 1, &crc);

    for (bit = 0; bit < 8; bit++)
	CRCengine((cksum >> bit) & 1, &crc);

    if (crc) {
#ifdef NIC_DEBUG
	printf("ds2502_read_rom: bad checksum %x, expected %x\n", crc, cksum);
#endif
	return 0;
    }

    return 1;
}

ds2502_read_ram(char *data, int count, int address)
{
    int i, bit;
    unsigned char stuff[128], cksum;
    unsigned char crc;

    for (i = 0; i < count; i++)
	data[i] = 0;

    /* send read memory cmd/addr */
    stuff[0] = 0xf0;
    stuff[1] = address & 0xff;
    stuff[2] = (address >> 8) & 0xff;

    crc = 0;
    for (i = 0; i < 3; i++)	    /* verify part serial number is valid */
	for (bit = 0; bit < 8; bit++)
	    CRCengine((stuff[i] >> bit) & 1, &crc);

    /* write cmd/addr to NIC */
    ds2502_write(stuff, 3);

    /* read checksum for cmd/addr back */
    for (i = 0; i < 128; i++)
	stuff[i] = 0;
    ds2502_read(stuff, 1);

    /* do they match?? */
    if ((stuff[0] & 0xff) != (crc & 0xff)) {
#ifdef NIC_DEBUG
	printf("ds2502_read_ram: bad cmd cksum %x, expected %x\n", 
	       stuff[0], crc);
#endif
	return 0;
    }

    ds2502_read(stuff, 128 - address);

    ds2502_read(&cksum, 1);

    for (i = 0; i < count; i++)
	data[i] = stuff[i];

    crc = 0;
    for (i = 0; i < 128 - address; i++)	    /* verify */
	for (bit = 0; bit < 8; bit++)
	    CRCengine((stuff[i] >> bit) & 1, &crc);
    for (bit = 0; bit < 8; bit++)
        CRCengine((cksum >> bit) & 1, &crc);

    if (crc) {
#ifdef NIC_DEBUG
	printf("ds2502_read_ram: bad checksum %x, expected %x\n", crc, cksum);
#endif
	return 0;
    }

    return 1;
}

/*
 * CRC16 check - compute 16 bit crc over 6 address bytes and compare with value
 *		read back from NIC. 
 */

int
check_crc16(char* serial)
{

	int i;
	char crc_bytes[2];

        CRC16 = 0;

        for (i = 0; i < 6; i++)
                Crc16(serial[i]);

        crc_bytes[0] = (unsigned char)~(CRC16 & 0xFF) & 0xFF;
        crc_bytes[1] = (unsigned char)~((CRC16 & 0xFF00) >> 8) & 0xFF;

        if (serial[6] != crc_bytes[0] && serial[6] != 0xff){
#ifdef NIC_DEBUG
                printf("eaddr CRC check failed, %02x | = %02x\n",serial[6],crc_bytes[0]);
#endif
		return 1;
	}
        if (serial[7] != crc_bytes[1] && serial[7] != 0xff){
#ifdef NIC_DEBUG
                printf("eaddr CRC check failed, %02x | = %02x\n",serial[7],crc_bytes[1]);
#endif
		return 1;
	}
	return 0;


}

/*
 * read the ethernet address (the ROM serial number)
 * and return it to the caller in the expected byte
 * order (MSB first).
 */
void
ds2502_get_eaddr(char *eaddr)
{
    char serial[24];
    int  idx, i, err ; 

    if (!ds2502_init()) {
      /*
       * fill it up with a default eaddr 
       * if reading the ROM failed.
       */
      eaddr[0] = 0xff;
      eaddr[1] = 0xff;
      eaddr[2] = 0xff;
      eaddr[3] = 0xff;
      eaddr[4] = 0xff;
      eaddr[5] = 0xff;
      return;
    }

    if (!ds2502_read_rom(serial, 6)) {
#ifdef NIC_DEBUG
        printf ("ds2502_get_eaddr: ds2502_read_rom failed!\n") ;
	for (idx = 0; idx < 6; idx++)
	    printf("eaddr[%d] = %x\n", idx, serial[idx]);
#endif
	/*
	 * fill it up with a default eaddr 
	 * if reading the ROM failed.
	 */
	eaddr[0] = 0xff;
	eaddr[1] = 0xff;
	eaddr[2] = 0xff;
	eaddr[3] = 0xff;
	eaddr[4] = 0xff;
	eaddr[5] = 0xff;
	return;
    } 

    if (!ds2502_read_ram(serial, 24, 0)) {
#ifdef NIC_DEBUG
        printf ("ds2502_get_eaddr: ds2502_read_ram failed!\n") ;
	for (idx = 0; idx < 24; idx++)
	    printf("eaddr[%d] = %x\n", idx, serial[idx]);
#endif
	eaddr[0] = 0xff;
	eaddr[1] = 0xff;
	eaddr[2] = 0xff;
	eaddr[3] = 0xff;
	eaddr[4] = 0xff;
	eaddr[5] = 0xff;
    } else {
#ifdef NIC_DEBUG
        printf ("ds2502_get_eaddr: ds2502_read_ram succeeded!\n") ;
	for (idx = 0; idx < 24; idx++)
	    printf("eaddr[%d] = %x\n", idx, serial[idx]);
#endif

	/*
	 * check the 16 bit CRC
	 */

	for (i=0;i<3;i++){
		err = check_crc16(&serial[i*8]);
		if (err){
			eaddr[0] = 0xff;
			eaddr[1] = 0xff;
			eaddr[2] = 0xff;
			eaddr[3] = 0xff;
			eaddr[4] = 0xff;
			eaddr[5] = 0xff;
        	}
		else{
			/*
	 		* put the eaddr into the proper
	 		* byte order.  ROM returns it LSB first.
	 		*/
			eaddr[0] = serial[(i*8)+5];
			eaddr[1] = serial[(i*8)+4];
			eaddr[2] = serial[(i*8)+3];
			eaddr[3] = serial[(i*8)+2];
			eaddr[4] = serial[(i*8)+1];
			eaddr[5] = serial[(i*8)+0];
			return;
		}
	}
    }
}
