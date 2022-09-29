/*
 * WARNING:	There is more than one copy of this file in different isms.
 *		All copies must be kept exactly in sync.
 *		Do not modify this file without also updating the following:
 *
 *		irix/kern/io/nic.c
 *		stand/arcs/lib/libsk/ml/nic.c
 *
 *	(from time to time they might not be in sync but that's due to bringup
 *	 activity - this comment is to remind us that they eventually have to
 *	 get back together) 
 *
 * nic.c
 *
 *      >>> Dallas number-in-can (NIC) driver. <<<
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/*
 * Dallas Number-in-Can (NIC) Part Overview
 *
 * All parts contain a 48 bit unique serial number,
 * laser written at the factory. Reading of this serial
 * number is protected by an 8 bit CRC.
 *
 * A family code is represented by a byte; this
 * allows a determination of which part(s) are
 * being read.  In addition, the type code
 * implicitly specifies which commands may be
 * used.
 *
 * How it works:
 *
 * A single signal pin and ground characterize all the NIC
 * parts. Power is provided by current flow though a 5k pullup
 * resistor to 5v.
 *
 * This single signal is used to:
 *
 * a) reset the device to a known state
 *
 * b) detect the presence of one or more devices
 *
 * c) read the serial number(s) of one or more devices
 *
 * d) individually select a particular device when
 *    multiple ones are present
 *
 * e) address & read data pages from a device
 *
 * Algorithm for determining how many parts are attached to
 * a single line, and how to read their serial numbers:
 *
 * a) First give a reset pulse.  This brings all parts to
 *    a known reset state.  The reset pulse is longer than
 *    the read and write pulses, so parts know it is reset.
 *
 * b) Then write a search rom command.  The host can write
 *    a logical '1' with a low pulse of 5us, or a logical
 *    '0' with a low pulse of 80us.
 *
 * c) Then read the first bit of serial number (a total of
 *    64 bits are read, which includes the family code
 *    byte, the 48 bit serial number, and a one byte CRC
 *    covering the other 56 bits).
 *
 * d) Since the signal line is open collector driven, if all
 *    the parts have a '1' in the first bit of their serial
 *    number, we read a '1'.  If all have a '0' as the first
 *    bit, we read a '0'.
 *
 * e) Then read the *complement* of the first bit of each
 *    parts serial number.  Note that if each had a '1' as
 *    the first bit, they all now send a '0'; if each had
 *    a '0' they now all send a '1'.
 *
 * f) The secret: if there were some parts with a '1' and some
 *    with a '0', the parts with a '0' would pull the line low
 *    on the read bit, then the parts with a '1' would pull
 *    it low on the read complement bit.  This is how we
 *    discover that there are multiple parts with both bits
 *    at this particular bit position.
 *
 * g) So, the read & read-complement return:
 *
 *        all parts have a '0':           0 1
 *
 *        all parts have a '1':           1 0
 *
 *        some of each:                   0 0
 *
 *        nothing connected:              1 1
 *
 * h) Then, we go searching down both branches by telling
 *    all the parts which bit (0 or 1) we want to continue
 *    with.  For example, if we see parts with both, we *tell*
 *    all of them that we are interested in the '0' parts;
 *    all of them continue to operate, the others effectively
 *    disconnect & wait for a new reset pulse.
 *
 */

#include <sys/nic.h>
#include <sys/param.h>
#include <sys/cmn_err.h>

#if NIC_EMULATE
#include <stdio.h>
#include "search.h"
#else
#include <sys/systm.h>
#include <sys/iograph.h>
#include <sys/pda.h>
#if SN0
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/agent.h>
#endif	/* SN0 */
#endif	/* NIC_EMULATE */

#ifdef _STANDALONE
#include <stdio.h>
#ifdef SN0
#include "rtc.h"
#undef DELAY
#define DELAY(usec)	rtc_sleep(usec)
#else
#include <libsk.h>
#endif	/* SN0 */
#elif _STANDALONE
#define DELAY(usec)	microsec_delay(usec)
#include <string.h>
#endif

#ifndef	_STANDALONE
#include <sys/atomic_ops.h>
#include <sys/hwgraph.h>
#define	NEWSZ(ptr,sz)	((ptr) = kern_malloc((sz)))
#define	NEWA(ptr,n)	NEWSZ((ptr), (n)*sizeof (*(ptr)))
#define	NEW(ptr)	NEWSZ((ptr), sizeof (*(ptr)))
#define	DEL(ptr)	(kern_free((ptr)))
#endif

#define NIC_VERBOSE 	0

#define LBYTEU(caddr) \
	(u_char) ((*(uint *) ((__psunsigned_t) (caddr) & ~3) << \
	  ((__psunsigned_t) (caddr) & 3) * 8) >> 24)

#define MAX_INFO	2048
#define CRC(reg,val)	(reg) = LBYTEU(&crc8[(reg)^(val)])

/*
 *	 Function Table of Contents
 */

static void		crc16(uchar_t, unsigned short *);

#ifdef SN0
__uint64_t		nic_get_phase_bits();
#endif

static int		nic_presence(nic_state_t *);
static int		nic_write(nic_state_t *, uchar_t);
static int		nic_read(nic_state_t *);
static int		nic_write_byte(nic_state_t *, uchar_t);
static int		nic_read_byte(nic_state_t *);
static int		nic_next_scan(nic_state_t *);

int			nic_setup(nic_state_t *, nic_access_t, nic_data_t);
int			nic_next(nic_state_t *, char *, char *, char *);

static int		nic_match_rom(nic_state_t *, char *, char *, char *);

int			nic_read_one_page(nic_state_t *, char *, char *, char *, int, uchar_t *, uchar_t *);
int			nic_read_mfg(nic_state_t *, char *, char *, char *, uchar_t *, uchar_t *);

static void		char2hex(char *, uchar_t);
static void		chars2hex(char *, uchar_t *, int);
static void		chars2print(char *, uchar_t *, int);

int			nic_eaddr(nic_access_t, nic_data_t, char *);
int			nic_access_mcr32(nic_data_t, int, int, int);
int			nic_get_eaddr(__int32_t *, __int32_t *, char *);

static int		nic_mfg_next(nic_state_t *, char *);

int			nic_info_get(nic_access_t, nic_data_t, char *);

#ifndef _STANDALONE

char		       *nic_vertex_info_get(vertex_hdl_t);
char		       *nic_vertex_info_set(nic_access_t, nic_data_t, vertex_hdl_t);
int			nic_vertex_info_match(vertex_hdl_t, char *);

#ifdef SN0
static int		nic_access_hub(nic_data_t, int, int, int);
char		       *nic_hub_vertex_info(vertex_hdl_t);
#endif	/* SN0 */

char		       *nic_bridge_vertex_info(vertex_hdl_t, nic_data_t);
char		       *nic_ioc3_vertex_info(vertex_hdl_t, nic_data_t, __int32_t *);

nic_vmce_t		nic_vmc_add(char *, nic_vmc_func *);
void			nic_vmc_del(nic_vmce_t);
static void		nic_vmc_check(vertex_hdl_t, char *);

#else	/* _STANDALONE */

#if SN0
int			klcfg_get_nic_info(nic_data_t,char *);
#else
int			cfg_get_nic_info(nic_data_t,char *);
#endif

#endif	/* _STANDALONE */

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

#if NIC_VERBOSE
static void
dump_hex(unsigned char *c, int n)
{
	int i;
	
	for (i=0; i<n; i++)
	  printf("%02x ",c[i]);
	printf("    ");
	for (i=0; i<n; i++)
	  printf("%c",c[i]<32 ? '.' : (c[i]>127? '.' : c[i]));
	printf("\n");
}
#endif

static int oddparity[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

/*
 * crc16() computes a  CRC16-polynomial function X^16+x^15+x^2+1
 * first arg is the input data byte
 * second arg goes in as the current CRC and is returned as the result CRC
 */
 
static void
crc16(uchar_t in_data, unsigned short *crc16_data)
{
   unsigned short data = in_data;

   data = (data ^ (*crc16_data & 0xff)) & 0xff;
   (*crc16_data) >>= 8;

   if (oddparity[data & 0xf] ^ oddparity[data >> 4])
      (*crc16_data) ^= 0xc001;

   data <<= 6;
   (*crc16_data)   ^= data;
   data <<= 1;
   (*crc16_data)   ^= data;
}

#ifdef SN0

__uint64_t
nic_get_phase_bits(void)
{
    __uint64_t	divisor, parm;

    divisor = IP27CONFIG.freq_hub / IP27C_MHZ(1);
    parm = divisor / 2 - 1;

    return (parm << 27 | parm << 20);
}

#endif /* SN0 */

/*
 * nic_presence() sends a reset-and-presence pulse and then checks
 * for device(s) present.  If any devices are present, it will read
 * a low value and return 0.  If no devices are attached, the line
 * will float high and this routine will return 1.
 */

static int
nic_presence(nic_state_t *ns)
{
    return ns->access(ns->data, 520, 65, 500);
}

/*
 * nic_write() writes a single bit on the NIC line.  c should be 0 or 1.
 */

static int
nic_write(nic_state_t *ns, uchar_t c)
{
    return ns->access(ns->data, c ? 6 : 80, c ? 110 : 30, 0);
}

/*
 * nic_read() reads a single bit from the NIC line.
 * The return value is the bit read, 0 or 1.
 */

static int
nic_read(nic_state_t *ns)
{
    return ns->access(ns->data, 6, 13, 100);
}

/*
 * nic_write_byte() and nic_read_byte() are used to send commands
 * and to read pages of data.
 */

static int
nic_write_byte(nic_state_t *ns, uchar_t b)
{
	int i, r;

	for (i=0; i<8; i++)
	  if ((r = nic_write(ns, b & 1 << i)) < 0)
	    return r;

	return NIC_OK;
}

static int
nic_read_byte(nic_state_t *ns)
{
	int i, r, b = 0;

	for (i=0; i<8; i++)
	  if ((r = nic_read(ns)) < 0)
	    return r;
	  else
	    b |= r << i;

	return b;
}

/*
 * nic_next_scan() can return:
 *
 * 0 == part found, bits[] hold 8 bytes of data
 * 1 == no more ports to find (success)
 * 2 == problem detecting parts (failure)
 * Or negative error code from access routine
 *
 * The algorithm is from page 54 of the Dallas
 * "Book of DS19xx Touch Memory Standards"
 */

static int
nic_next_scan(nic_state_t *ns)
{
	int bit_a, bit_b, r;

	if (ns->done)
	  return NIC_DONE;

	if ((r = nic_presence(ns)) < 0)
	  return r;

	if (r)
	  return NIC_NOT_PRESENT;

	ns->bit_index = 1;
	ns->disc_marker = 0;
	if ((r = nic_write_byte(ns, 0xf0)) < 0)
	  return r;
	while (1) {
	    if ((bit_a = nic_read(ns)) < 0)
	      return bit_a;
	    if ((bit_b = nic_read(ns)) < 0)
	      return bit_b;
	  if ((bit_a == 1) && (bit_b == 1)) {
	    return NIC_FAIL;
	  }
	  if ((bit_a == 0) && (bit_b == 0)) {
	    if (ns->bit_index == ns->last_disc) {
	      ns->bits[ns->bit_index-1] = 1;
	    } else
	    if (ns->bit_index > ns->last_disc) {
	      ns->bits[ns->bit_index-1] = 0;
	      ns->disc_marker = ns->bit_index;
	    } else
	    if (ns->bits[ns->bit_index-1] == 0) {
	      ns->disc_marker = ns->bit_index;
	    }
	    if ((r = nic_write(ns, ns->bits[ns->bit_index-1])) < 0)
	      return r;
	    ns->bit_index++;
	    if (ns->bit_index>64) {
	      ns->last_disc = ns->disc_marker;
	      if (ns->last_disc == 0)
		ns->done = 1;
	      return NIC_OK;
	    }
	    continue;
	  }
	  ns->bits[ns->bit_index-1] = bit_a;
	  if ((r = nic_write(ns, ns->bits[ns->bit_index-1])) < 0)
	    return r;
	  ns->bit_index++;
	  if (ns->bit_index>64) {
	    ns->last_disc = ns->disc_marker;
	    if (ns->last_disc == 0)
	      ns->done = 1;
	    return NIC_OK;
	  }
	  continue;
	}
}

/*
 * nic_setup() initializes for a part search.  Returns 0 on success,
 * NIC_* on NIC failure, or a negative error code from the access routine.
 * NIC failure means a missing or broken NIC part.
 */

int
nic_setup(nic_state_t *ns,
	  nic_access_t access,
	  nic_data_t data)
{
	ns->access = access;
	ns->data = data;
	ns->last_disc = 0;
	ns->done = 0;

	return nic_presence(ns);
}

/*
 * nic_next() looks for a part and if found writes the serial number and family
 * code into areas specified by the caller.  Each time it is called it will
 * return the next part, or 1 == all done (no more parts).
 *
 * if serial is non-NULL, it must point to 6 bytes to hold the serial number
 *
 * if family is non-NULL, it must point to 1 byte to hold the family code
 *
 * rc: NIC_OK == success, returning family, serial & crc
 *     NIC_DONE == no more parts
 *     NIC_NOT_PRESENT == couldn't find any nic parts
 *     NIC_FAIL == impossible result, hw failure or delay not in microseconds
 *     Or a negative error code from access routine.
 */

int
nic_next(nic_state_t *ns, char *serial, char *family, char *crc)
{
	unsigned char byte[8];
	unsigned char t_crc = 0;
	int rc;
	int i;

	rc = nic_next_scan(ns);
	if (rc)
	  return rc;
	for (i=0; i<8; i++)
	  byte[i] = 0;
	for (i=0; i<64; i++) {
	  byte[i/8] >>= 1;
	  byte[i/8] |= 0x80*ns->bits[i];
	}
	for (i=0; i<8; i++)	    /* verify part serial number is valid */
	  CRC(t_crc,byte[i]);
	if (t_crc)
	  return NIC_BAD_CRC;
	if (serial){
	  serial[0] = byte[6];
	  serial[1] = byte[5];
	  serial[2] = byte[4];
	  serial[3] = byte[3];
	  serial[4] = byte[2];
	  serial[5] = byte[1];
	}
	if (family)
	  family[0] = byte[0];
	if (crc)
	  crc[0] = byte[7];
	return NIC_OK;
}

static int
nic_match_rom(nic_state_t *ns, char *family, char *serial, char *crc)
{
	int r;
	if ((r = nic_presence(ns)) < 0)
	  return r;
	if (r) {
	  printf("nic_match_rom: couldn't see presence pulse "
		 "during match rom command\n");
	  return NIC_NOT_PRESENT;
	}
	if ((r = nic_write_byte(ns, 0x55)) < 0)	/* match rom command */
	    return r;
	if ((r = nic_write_byte(ns, family[0])) < 0)
	    return r;
	if ((r = nic_write_byte(ns, serial[5])) < 0)
	    return r;
	if ((r = nic_write_byte(ns, serial[4])) < 0)
	    return r;
	if ((r = nic_write_byte(ns, serial[3])) < 0)
	    return r;
	if ((r = nic_write_byte(ns, serial[2])) < 0)
	    return r;
	if ((r = nic_write_byte(ns, serial[1])) < 0)
	    return r;
	if ((r = nic_write_byte(ns, serial[0])) < 0)
	    return r;
	if ((r = nic_write_byte(ns, crc[0])) < 0)
	    return r;
	return NIC_OK;
}

int
nic_read_one_page(nic_state_t *ns, char *family, char *serial, char *crc,
                  int start, uchar_t *redirect, uchar_t *byte)
{
	int pass = 64;
        int page = start;
	int rc;
	int i, r;
	unsigned short crc16_data;
	
	while (redirect[page] != 255) {
#if NIC_VERBOSE
	  printf("Redirection: page %d --> %d\n",page,0xff&(~redirect[page]));
#endif
	  page = 0xff&(~redirect[page]);
	  pass--;
	  if (!pass) {
            printf("nic_read_one_page: loop in NIC redirection map\n");
            return NIC_REDIR_LOOP;
	  }
	}

	if ((rc = nic_match_rom(ns, family, serial, crc)) != 0)
	  return rc;
        if ((r = nic_write_byte(ns, 0xf0)) < 0)
	  return r;         	/* write f0, read memory command*/
	if ((r = nic_write_byte(ns, (page*32)&0xff)) < 0)
	  return r;		/* TA1 low */
	if ((r = nic_write_byte(ns, (page*32)>>8)) < 0)
	  return r;		/* TA2 high */

	if (family[0] == 0x09)			/* DS1982 for Module Serial */
	  if ((r = nic_read_byte(ns)) < 0)
	    return r;

        for (i=0; i<32; i++) {			/* read the data page */
	  if ((r = nic_read_byte(ns)) < 0)
	    return r;
          byte[i] = r;
        }
	  
#if NIC_VERBOSE
        printf("nic_read_one_page: Page %02d:\n",page);
        dump_hex(&byte[0],16);
        dump_hex(&byte[16],16);
#endif
        crc16_data = 0;
        for (i=0; i<32; i++)
          crc16(byte[i],&crc16_data);
        if (crc16_data != 0xb001) {
#if NIC_VERBOSE
	  printf("nic_read_one_page: bad crc16 on data page %d\n",page);
#endif
	  return NIC_BAD_CRC;
	}
	return NIC_OK;				/* success */
}

int
nic_read_mfg(nic_state_t *ns, char *family, char *serial, char *crc,
             uchar_t *pageA, uchar_t *pageB)
{
	int page;
	int rc;
	int i, r;
	unsigned short crc16_data;
	unsigned char redirect[64];		/* for the DS2505 */
	unsigned char byte[32];
	
        if (family[0] == 0x09) {		/* module serial number DS1982 */
          for (i=0; i<64; i++)
            redirect[i] = 0xff;			/* redirection not used */
          if ((r = nic_read_one_page(ns, family, serial,
                                     crc, 0, redirect, pageA)) != NIC_OK)
              return r;
          if ((r = nic_read_one_page(ns, family, serial,
                                     crc, 1, redirect, pageB)) != NIC_OK)
              return r;
          return NIC_OK;
        }
        
	if ((rc = nic_match_rom(ns, family, serial, crc)) != 0)
	  return rc;
        if ((r = nic_write_byte(ns, 0xaa)) < 0)
	    return r;         	/* write f0, read memory command*/
	if ((r = nic_write_byte(ns, 0x00)) < 0)
	    return r;		/* TA1 */
	if ((r = nic_write_byte(ns, 0x01)) < 0)
	    return r;		/* TA2 */

        for (page=0; page<8; page++) {		/* have to read status 8 bytes at a time */
          for (i=0; i<10; i++) {
	    if ((r = nic_read_byte(ns)) < 0)
		return r;
            byte[i] = r;			/* 8 data + 2 crc16 */
	  }
#if NIC_VERBOSE
        printf("nic_read_mfg: Page %02d redirection map:\n",page);
        dump_hex(&byte[0],10);
#endif
          crc16_data = 0;
          if (page == 0) {
            crc16(0xaa,&crc16_data);		/* first page crc16 is different */
            crc16(0x00,&crc16_data);
            crc16(0x01,&crc16_data);
          }
          for (i=0; i<10; i++)			/* compute crc16 on redirection bytes */
            crc16(byte[i],&crc16_data);
          if (crc16_data != 0xb001) {		/* success? */
	    printf("nic_read_mfg: invalid crc16 reading redirection map page %d\n",page);
	    return NIC_BAD_CRC;
	  }
          for (i=0; i<8; i++)			/* save redirection bytes */
            redirect[page*8+i] = byte[i];
	}
	if ((r = nic_read_one_page(ns, family, serial,
				   crc, 0, redirect, pageA)) != NIC_OK)
	    return r;
	if ((r = nic_read_one_page(ns, family, serial,
				   crc, 1, redirect, pageB)) != NIC_OK)
	    return r;
	return NIC_OK;
}

/*
 * char2hex() converts a character into a 2-byte printable string.
 */
 
static void
char2hex(char *h, uchar_t c)
{
	uchar_t table[] = "0123456789abcdef";
	
	h[0] = LBYTEU(&table[(c>>4)&0x0f]);
	h[1] = LBYTEU(&table[c&0x0f]);
}

/*
 * chars2hex() converts a binary string into a hex string with
 * terminating NULL
 */
 
static void
chars2hex(char *h, uchar_t *s, int n)
{
	int i;
	
	h += strlen((const char *)h);
	for (i=0; i<n; i++)
	  char2hex(&(h[i*2]), s[i]);
	h[n*2] = 0;
}

/*
 * chars2print() copies a max of n characters from a string, striping out
 * blanks and converting unprintable characters into periods.  In addition,
 * the ':' and ';' characters are turned into '?' to simplify parsing.
 */
 
static void
chars2print(char *out, uchar_t *in, int n)
{
	int i;
        char *start;
        
	out += strlen((const char *)out);
	start = out;
	for (i=0; i<n; i++) {
	  if (in[i] == ':' || in[i] == ';')	/* : and ; are separators */
	    in[i] = '?';
	  if (in[i] == 0xff)			/* 0xff defined as 'unused' */
	    in[i] = ' ';                        /* so we turn it into a space */
	  if (in[i] > ' ' && in[i] <= 0x7f)	/* copy if printable */
	    *out++ = in[i];
	  if (in[i] < ' ' || in[i] > 0x7f)	/* convert unprintable to . */
	    *out++ = '.';
	}
	if (start == out) {			/* return at least one space */
	  *out++ = ' ';
	}
	*out = 0;				/* add terminating NULL */
}

/*
 * nic_eaddr() is the general case for reading a MAC format part on any
 * nic controller
 *
 * Here is the format of enet MAC address NIC parts
 *
 *   XX - 1 byte command CRC
 *   XX - 1 byte data length
 *
 *   XX
 *   XX - 4 bytes assigned by Dallas
 *   XX   (currently padded with zeros for protos)
 *   XX
 *
 *   XX
 *   XX
 *   XX - 6 bytes Mac Address
 *   XX   LSB to MSB
 *   XX
 *   XX
 *
 *   XX - 16 bits of data CRC
 *   XX
 *
 * I'm currently checking the command CRC, the data length, and
 * the 16 bit CRC on the 10 bytes of data.  I'm ignoring the 4
 * bytes assigned by Dallas (they are zero in the samples).
 */
 
int
nic_eaddr(nic_access_t access, nic_data_t data, char *eaddr)
{
	nic_state_t ns;
	int rc;
	int i;
	unsigned short crc16_data;
        unsigned char byte[32];
	char serial[6];
	char family[1];
	char crc[1];

	if (!eaddr) {
	  printf("nic_eaddr: Called with NULL eaddr!\n");
	  return NIC_PARAM;
	}

	if ((rc = nic_setup(&ns, access, data)) != 0) {
	  if (rc > 0) {
	    printf("nic_eaddr: No presence pulse from NIC\n");
	    printf("nic_eaddr: (check for an empty NIC socket)\n");
	    rc = NIC_NOT_PRESENT;
	  }
	  return rc;
	}

	/* 
	 * This code is intended to allow for multiple NIC parts on the IOC3,
	 * as in the case of the Sn00 motherboard.  It should keep looking
	 * past other parts until it finds a readable MAC part.
	 */
	while (1) {
          rc = nic_next(&ns, serial, family, crc);
          if (rc != NIC_OK) {
            return rc;
          }
          switch (*family) {
            case 0x0b:				/* DS2505 normal mfg. */
                  continue;
            case 0x89:				/* DS1982U */
            case 0x91:				/* DS1981U */
            case 0x09:				/* DS1982 normal MAC */
                  break;
            case 0x08:                          /* DS1992 */
            case 0x06:                          /* DS1993 */
            case 0x04:                          /* DS1994 */
            case 0x0a:                          /* DS1995 */
            case 0x0c:                          /* DS1996 */
                  break;                        /* programmable part */
            default:
                  continue;
          }
          if ((rc = nic_write_byte(&ns, 0xf0)) < 0)
	      return rc;	               /* write f0, read memory command*/
          if ((rc = nic_write_byte(&ns, 0x00)) < 0)
	      return rc;	                /* TA1 */
          if ((rc = nic_write_byte(&ns, 0x00)) < 0)
	      return rc; 	                /* TA2 */

          for (i=0; i<14; i++) {
	    if ((rc = nic_read_byte(&ns)) < 0)
	      return rc;
            byte[i] = rc;			/* partial page to minimize read time */
	  }

          crc16_data = 0;                       /* verify crc16 */
          for (i=1; i<14; i++) {
            crc16(byte[i],&crc16_data);
          }
          if (byte[0] != 0x8d) {
#if NIC_VERBOSE
            printf("nic_eaddr: Invalid enet MAC command CRC byte 0x%02x, s/b 0x8d\n",byte[0]);
#endif
            continue;
          }
          if (crc16_data != 0xb001) {
#if NIC_VERBOSE
            printf("nic_eaddr: Invalid enet crc16 0x%04x, s/b 0xb001\n",crc16_data);
#endif
            continue;
          }

#ifdef NIC_COOKER
#include "nic_cooker.i"
#endif
          if (byte[1] != 0x0a) {
#if NIC_VERBOSE
            printf("nic_eaddr: Invalid enet length byte 0x%02x found in NIC, s/b 0x0a\n",byte[1]);
#endif
            continue;
          }

          crc16_data = 0;                       /* verify crc16 */
          for (i=1; i<14; i++) {
            crc16(byte[i],&crc16_data);
          }
          if (crc16_data != 0xb001) {
#if NIC_VERBOSE
            printf("nic_eaddr: Invalid enet MAC crc16 0x%04x, s/b 0xb001\n",crc16_data);
#endif
            continue;
          }
          break;				/* success */
        }
        for (i=0; i<6; i++)                   	/* byte[6] == LSB, byte[11] == MSB */
          eaddr[i] = byte[11-i];

	return NIC_OK;				/* success */
}

/*
 * 32-bit MCR access routine
 *
 *   This should be moved to a hardware-dependent module, except that
 *   it's common to so many of them.
 */

int
nic_access_mcr32(nic_data_t data,
		 int pulse, int sample, int usec)
{
    __uint32_t		value;

    *(volatile __uint32_t *) data = MCR_PACK(pulse, sample);

    do
	value = *(volatile __uint32_t *) data;
    while (! MCR_DONE(value));

    DELAY(usec);

    return MCR_DATA(value);
}

/*
 * nic_get_eaddr() knows how to read the IOC3 MAC address part in particular
 * external NIC interface for bsd/sgi/if_ef.c
 *
 * inputs are mcr and gpcr_s addresses for the IOC3 that if_ef is looking at
 * output is the 6 byte eaddr found in the MAC NIC wired to that IOC3
 *
 * returns 0 on success, and fills in the 6 bytes with a properly verified
 * (via crc-8 and crc-16 checks) MAC address.
 *
 * returns non-zero on failure, and leaves the eaddr alone.
 */

int
nic_get_eaddr(__int32_t *mcr, __int32_t *gpcr_s, char *eaddr)
{
	int rc;
	int tries = 2;	/* the ds1992 battery part might need 2 tries */
	
	if (gpcr_s)
	  *(volatile __uint32_t *)gpcr_s = 0x00200000;	/* enable nic bit */
	else
	  printf("nic_get_eaddr: bypassing gpcr_s enable in ioc3\n");

	while (tries--) {
	  rc = nic_eaddr(nic_access_mcr32, (nic_data_t) mcr, eaddr);
	  if (rc == NIC_OK)
	    return rc;
	}
        printf("nic_eaddr: Failed to find readable MAC NIC, rc %d\n",rc);
	return rc;
}

/*
 *	nic_mfg_next() takes in a nic_state_t, and fills in a char string, zero == success
 */
 
static int
nic_mfg_next(nic_state_t *ns, char *info)
{
	int rc;
	uchar_t pageA[32];
	uchar_t pageB[32];
	char serial[6];
	char family[1];
	char crc[1];
        
        if (!info)			/* just in case */
          return NIC_PARAM;
	info += strlen((const char *)info);
        while (1) {
          rc = nic_next(ns, serial, family, crc);
          switch (rc) {
            case NIC_OK:
#if NIC_VERBOSE
              printf("Serial: %02x %02x %02x %02x %02x %02x Family: %02x\n",
                serial[0], serial[1], serial[2], serial[3],
                serial[4], serial[5], family[0]);
#endif
              if ((family[0] != 0x0b) &&	/* usually accept DS2505 */
		  (family[0] != 0x09))		/* or DS1982 for MODULEID */
                break;
#if IP30
	      if (family[0] == 0x09)		/* except for IP30 */
	        break;				/* where 09 isn't wanted */
#endif
              if ((rc = nic_read_mfg(ns,family,serial,crc,pageA,pageB)) != 0) {
		if (rc < 0)
		  return rc;
                strcpy(info, "NIC: data not available;");
	      } else {
                if (pageA[0] != 0x01) {
                  strcpy(info, "NIC: page A data not available");
                } else {
                  strcpy(info, "Part:");
                  chars2print(info, &pageA[11], 19);
                  chars2print(info, &pageB[0], 6);
                  strcat(info, ";Name:");
                  chars2print(info, &pageB[16], 14);
                  strcat(info, ";Serial:");
                  chars2print(info, &pageA[1], 10);
                  strcat(info, ";Revision:");
                  chars2print(info, &pageB[6], 4);
                  strcat(info, ";Group:");
                  chars2hex(info, &pageB[10], 1);
                  strcat(info, ";Capability:");
                  chars2hex(info, &pageB[11], 4);
                  strcat(info, ";Variety:");
                  chars2hex(info, &pageB[15], 1);
                }
              }
              strcat(info, ";Laser:");
              chars2hex(info, (uchar_t *)serial, 6);
              strcat(info, ";");
#if NIC_VERBOSE
	      printf("nic_mfg_next: %s\n",info);
#endif
              return NIC_OK;
            case NIC_DONE:
              return rc;
            case NIC_NOT_PRESENT:
	      strcpy(info, "NIC:Not present on interface;Laser:unreadable;");
              return rc;
	    case NIC_FAIL:
              strcpy(info, "NIC:Failure reading interface;Laser:unreadable;");
              return rc;
            case NIC_BAD_CRC:
              strcpy(info, "NIC:CRC failure reading interface;Laser:unreadable;");
              return rc;
            case NIC_REDIR_LOOP:
              strcpy(info, "NIC:Mfg part with redirection loop;Laser:unreadable");
              return rc;
            case NIC_PARAM:
              strcpy(info, "NIC:Called with invalid parameter;Laser:unreadable");
              return rc;
	  default:
	      strcpy(info, "NIC:Can't access part;Laser:unreadable;");
	      return rc;	/* Negative error code from access routine */
          }
        }
}

/*
 *	nic_info_get
 *
 *	given hardware-specific access routine and data, reads the nic(s)
 *	and fills in info with a self-describing string and laser with the
 *	48-bit laser ID.  set info or laser to NULL if not needed
 */

int
nic_info_get(nic_access_t access, nic_data_t data, char *info)
{
        nic_state_t ns;
        int rc;
        
        *info = 0;
        if ((rc = nic_setup(&ns, access, data)) < 0)
	    return rc;
        while (1) {
          rc = nic_mfg_next(&ns, info);
          if (rc != NIC_OK)
            break;
        }

	return rc;
}

/*
 *	nic_item_info_get
 *
 *	given an info buffer returned by nic_info_get
 *	set begin of item string and end of item string.
 *	returns length of item string;
 */
int
nic_item_info_get(char *buf, char *item, char **item_info)
{
	int n = strlen(item);

	while (*buf != '\0') {
	    if (strncmp(buf, item, n) == 0) {
		buf += n;
		*item_info = buf;

		n = 0;
		while (*buf != ';' && *buf != '\0') {
		    n++;
		    buf++;
		}
		return(n);
	    }
	    buf++;
	}
	return(0);
}
	       	
#ifndef _STANDALONE

/*
 *	nic_vertex_info_get
 *
 *	return the decoded NIC data from the specified vertex.
 *	or NULL if there is no attached NIC string.
 */

char *
nic_vertex_info_get(vertex_hdl_t v)
{
	arbitrary_info_t	ainfo = 0;
	(void)hwgraph_info_get_LBL(v, INFO_LBL_NIC, &ainfo);
	return (char *)ainfo;
}

/*
 *	nic_vertex_info_set
 *
 *	given hardware-specific access routine and a vertex,
 *	reads the nic(s) and attaches a self-describing string to the vertex
 *	If the vertex already has the string, just returns the string.
 */

char *
nic_vertex_info_set(nic_access_t access, nic_data_t data, vertex_hdl_t v)
{
        char *info_tmp;
	int info_len;
	char *info;

	/* see if this vertex is already marked */
	info_tmp = nic_vertex_info_get(v);
	if (info_tmp) return info_tmp;
	
	/* get a temporary place for the data */
	NEWSZ(info_tmp, MAX_INFO);
        if (!info_tmp) return NULL;

	/* read the NIC */
	/* NOTE: we ignore the return error code
	 * from nic_info_get, since if it did have
	 * a problem, it stuck reasonable information
	 * into info_tmp.
	 */
	(void)nic_info_get(access, data, info_tmp);

	/* allocate a smaller final place */
	info_len = strlen(info_tmp)+1;
	NEWSZ(info, info_len);
        if (info) {
		strcpy(info, info_tmp);
		DEL(info_tmp);
	} else {
		info = info_tmp;
	}

	/* add info to the vertex */
        hwgraph_info_add_LBL(v, INFO_LBL_NIC,
			     (arbitrary_info_t) info);

	/* see if someone else got there first */
	info_tmp = nic_vertex_info_get(v);
	if (info != info_tmp) {
	    DEL(info);
	    return info_tmp;
	}

	/* export the data */
        hwgraph_info_export_LBL(v, INFO_LBL_NIC, info_len);

	/* trigger all matching callbacks */
	nic_vmc_check(v, info);

	return info;
}

/*
 * nic_vertex_info_match
 *
 *  given a vertex and part identifier string, searches NIC(s) on that vertex 
 *  for a matching field. Return TRUE if found, FALSE if not.
 *
 *  Examples:
 *
 *  nic_vertex_info_match(v, "Part:030-0880-001;Rev:A")
 *
 *	returns TRUE if there is a NIC on the vertex with the specified
 *	part number and rev code letter
 *
 *  nic_vertex_info_match(v, "Part:030-0880-001")
 *
 *	returns TRUE if there is a NIC on the vertex with the specified
 *      part number
 *
 *  nic_vertex_info_match(v, "Part:030-0880-")
 *
 *	return TRUE if there is a 0880 board regardless of the -xxx or
 *      rev code
 */

int
nic_vertex_info_match(vertex_hdl_t v, char *s)
{
	char *info;

	info = nic_vertex_info_get(v);
	if (!info) return 0;
	return strstr(info, s) != (char *)NULL;
}
#endif

#if defined(SN0)

/*
 * Kernel's access routine for Hub
 *
 *	For SN0 Hub.  Knows how to read & write the hub nic register,
 *	given a vertex fills in the vertex info from each NIC found.
 *
 *	XXX Note that there is a workaround that attaches the midplane
 *	serial number and manufacturing nics to the hub in SN0 slot 0.
 */

/*ARGSUSED*/
static int nic_access_hub(nic_data_t data,
			  int pulse, int sample, int usec)
{
    __uint64_t		value;

#ifdef _STANDALONE
	HUB_S(HUBREG_CAST data, (nic_get_phase_bits() | MCR_PACK(pulse, sample))) ;
#else
    SET_MY_HUB_NIC(nic_get_phase_bits() | MCR_PACK(pulse, sample));
#endif

    do
#ifdef _STANDALONE
	value = HUB_L(HUBREG_CAST data) ;
#else
	value = GET_MY_HUB_NIC();
#endif
    while (! MCR_DONE(value));

    DELAY(usec);

    return MCR_DATA(value);
}

#ifndef _STANDALONE

char *
nic_hub_vertex_info(vertex_hdl_t v)
{
  char *nicinfo;
  nicinfo = nic_vertex_info_set(nic_access_hub, 0, v);
#if DEBUG && NIC_DEBUG
  cmn_err(CE_CONT, "%v HUB NIC:\n\t%s\n", v, nicinfo);
#endif
  return nicinfo;
}
#endif 

#endif	/* SN0 && !STANDALONE*/

/*
 *	For SN0 & SpeedRacer Bridges.  Given a pointer to a bridge 
 *      and bridge NIC port, fills in the vertex info from each NIC found.
 */

#if !defined(_STANDALONE)
char *
nic_bridge_vertex_info(vertex_hdl_t v, nic_data_t mcr)
{
  char *nicinfo;
  nicinfo = nic_vertex_info_set(nic_access_mcr32, mcr, v);
#if DEBUG && NIC_DEBUG
  cmn_err(CE_CONT, "%v Bridge NIC:\n\t%s\n", v, nicinfo);
#endif
  return nicinfo;
}
#endif

/*
 *	For Speedo IOC3.  Given a pointers to IOC3 control & nic registers
 *      fills in the vertex info from the manufacturing NIC found.  Note
 *      that on Speedo both the MAC and mfg. nic are on the IOC3.
 */

#if !defined(_STANDALONE)
char *
nic_ioc3_vertex_info(vertex_hdl_t v, nic_data_t mcr, __int32_t *gpcr_s)
{
	char *nicinfo;

	if (gpcr_s)
	  *(volatile __uint32_t *)gpcr_s = 0x00200000;	/* enable nic bit */

	nicinfo = nic_vertex_info_set(nic_access_mcr32, (nic_data_t)mcr, v);
#if DEBUG && NIC_DEBUG
	cmn_err(CE_CONT, "%v IOC3 NIC:\n\t%s\n", v, nicinfo);
#endif
	return nicinfo;
}
#endif

#if !defined(_STANDALONE)
/* =====================================================================
 *		NIC Callback Code
 */

/*
 *	SEMANTICS:
 *
 *	nic_vertex_match_callback enters its parameters in

 *	a list of callback functions.
 *
 *	When the contents of a NIC are decoded into a string
 *	and attached to a vertex, the list is scanned, and
 *	the callback functions are called for each entry where
 *	the specified pattern appears in the decoded NIC string.
 *
 *	NOTE: at this time, we assume that all NIC callbacks
 *	are registered *before* the particular vertexes they
 *	are interested in are decoded.
 *
 *	INTENDED USAGE:
 *
 *	Before we start calling driver attach() routines, various
 *	support codes and device drivers have the opportunity to
 *	register interest in certain classes of boards, based on
 *	the contents of their NIC; the most common use will be to
 *	specify a pattern that matches a particular 7- or 10-digit
 *	part number.
 *
 *	Drivers for devices that may have NICs attached should
 *	call into the NIC support code to have their data decoded
 *	and applied to their connection point vertex; as a side
 *	effect, before that call returns, any NIC callbacks will
 *	be executed. Thus, the driver should check any configuration
 *	hints attached to the connection point after the NIC call
 *	has been made.
 */

struct nic_vmce_s {
	nic_vmce_t		next;
	char		       *pat;
	nic_vmc_func	       *func;
};

static nic_vmce_t	nic_vmce_first = 0;
static nic_vmce_t      *nic_vmce_lastp = &nic_vmce_first;

/* nic_vmc_add:
 * in the future, whenever a new NIC is decoded
 * whose decoded string matches "pat", call the
 * specified function. If multiple entries match
 * a given NIC, they are called in the order that
 * they were processed by nic_vmc_add.
 *
 * returns an opaque (pointer-sized) handle that
 * can be used later to deactivate the callback.
 */
nic_vmce_t
nic_vmc_add(char *pat, nic_vmc_func *func)
{
	nic_vmce_t		vmce;
	nic_vmce_t	       *tail;

#if DEBUG && NIC_DEBUG
cmn_err(CE_CONT, "nic_vmc_add: '%s' gets you 0x%X\n", pat, func);
#endif
	NEW(vmce);
#if DEBUG
	if (vmce == NULL) {
cmn_err(CE_CONT, "nic_vmc_add: kern_malloc failed?!\n");
		return vmce;
	}
#endif
	vmce->next = 0;
	vmce->pat = pat;
	vmce->func = func;

	tail = swap_ptr((void **)&nic_vmce_lastp, (void *)&(vmce->next));
#if DEBUG
	if (tail == NULL)
		cmn_err(CE_PANIC, "nic_vmc_add: null tail pointer locn?!\n");
	if (*tail != NULL)
		cmn_err(CE_PANIC, "nic_vmc_add: null tail pointer contents?!\n");
#endif
	*tail = vmce;
#if DEBUG && NIC_DEBUG
cmn_err(CE_CONT, "nic_vmc_add: returning cookie 0x%X\n", vmce);
#endif
	return vmce;
}

/* nic_vmc_del:
 * deactivate the specified callback so that
 * future observations of new NICs will not
 * attempt to make this call.
 *
 * NOTE: due to the lockless algorithm used
 * to optimize the "add" and "check" operations
 * on this list, we can not really delete an
 * entry from the list, all we can do is
 * deactivate it; also, if a check is in
 * progress, that check may call the callback
 * function after nic_vmc_del has returned;
 * this is probably not a problem since we
 * only see "new" NICs at specific times.
 *
 * If the amount of storage space being taken
 * up by the VMC list becomes a problem, we
 * can identify a time after which we will
 * see no new NICs of interest to callbacks
 * and clear out the list completely.
 */
void
nic_vmc_del(nic_vmce_t vmce)
{
#if DEBUG && NIC_DEBUG
cmn_err(CE_CONT, "nic_vmc_del: deactivating cookie 0x%X\n", vmce);
#endif
	vmce->pat = 0;
	vmce->func = 0;
}

/* nic_vmc_check:
 * internal function
 * called when we decode a new NIC and
 * successfully add the decoded data to
 * its associated vertex.
 */
static void
nic_vmc_check(vertex_hdl_t vhdl, char *nicinfo)
{
	nic_vmce_t		vmce;
	char		       *pat;
	nic_vmc_func	       *func;

#if DEBUG && NIC_DEBUG
cmn_err(CE_CONT, "%v nic_vmc_check: scanning for nicinfo\n\t%s\n", vhdl, nicinfo);
#endif
	for (vmce = nic_vmce_first; vmce != NULL; vmce = vmce->next)
		if (((func = vmce->func) != NULL) &&
		    ((pat = vmce->pat) != NULL)) {
			if ((strstr(nicinfo, pat)) != NULL) {
#if DEBUG && NIC_DEBUG
cmn_err(CE_CONT, "\t0x%X %s MATCH (call 0x%X)\n", vmce, pat, func);
#endif
				func(vhdl);
			} else {
#if DEBUG && NIC_DEBUG
cmn_err(CE_CONT, "\t0x%X %s NOMATCH\n", vmce, pat, func);
#endif
			}
		}
}

#endif	/* !_STANDALONE*/

/*
 *	for IO6prom
 */
 
#if defined(_STANDALONE)
int
#if SN0
klcfg_get_nic_info(nic_data_t nicaddr,char *info)
#else
cfg_get_nic_info(nic_data_t nicaddr,char *info)
#endif	/* SN0 */
{
        return nic_info_get(nic_access_mcr32, (nic_data_t) nicaddr, info);
}

#ifdef SN0
get_hub_nic_info(nic_data_t nicaddr,char *info)
{
        return nic_info_get(nic_access_hub, nicaddr, info);
}

#endif /* SN0 */
#endif /* _STANDALONE */
