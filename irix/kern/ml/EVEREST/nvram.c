/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1986,1992 Silicon Graphics, Inc.                        *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.12 $"

/*
 * nvram.c
 *	Contains the support code for writing to the IO4 nvram
 *	under Everest. 
 */
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/pda.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/nvram.h>
#include <sys/EVEREST/evconfig.h>
#include <string.h>
#include <sys/cmn_err.h>
#include <sys/atomic_ops.h>

/*
 * Useful utility macros
 */
#define EPC_GETBYTE(_x) \
    ( *((unsigned char*) (SWIN_BASE(EPC_REGION, EPC_ADAPTER) + (_x) + 7)))

#define EPC_SETBYTE(_x, _v) \
    EPC_GETBYTE(_x) = (_v);

/*
 * nvram lookup table
 */
static struct nvram_entry nvram_tab[] = {
	{"SystemPartition", 	"", NVOFF_SYSPART, NVLEN_SYSPART},
	{"OSLoadPartition", 	"", NVOFF_OSPART, NVLEN_OSPART},
	{"OSLoader", 		"", NVOFF_OSLOADER, NVLEN_OSLOADER},
	{"OSLoadFilename", 	"", NVOFF_OSFILE, NVLEN_OSFILE},
	{"OSLoadOptions", 	"", NVOFF_OSOPTS, NVLEN_OSOPTS},
	{"AutoLoad", 		"", NVOFF_AUTOLOAD, NVLEN_AUTOLOAD},
	{"dbgtty",   		"", NVOFF_DBGTTY, NVLEN_DBGTTY},
	{"root",     		"", NVOFF_ROOT, NVLEN_ROOT},
	{"TimeZone", 		"", NVOFF_TZ, NVLEN_TZ}, 
	{"console", 		"", NVOFF_CONSOLE, NVLEN_CONSOLE},
	{"diagmode", 		"", NVOFF_DIAGMODE, NVLEN_DIAGMODE},
	{"diskless", 		"", NVOFF_DISKLESS, NVLEN_DISKLESS},
	{"nogfxkbd", 		"", NVOFF_NOKBD, NVLEN_NOKBD},
	{"keybd", 		"", NVOFF_KEYBD, NVLEN_KEYBD},
	{"lang", 		"", NVOFF_LANG, NVLEN_LANG},
	{"scsiretries", 	"", NVOFF_SCSIRT, NVLEN_SCSIRT},
	{"scsihostid", 		"", NVOFF_SCSIHOSTID, NVLEN_SCSIHOSTID},
	{"dbaud", 		"", NVOFF_LBAUD, NVLEN_LBAUD},
	{"rbaud", 		"", NVOFF_RBAUD, NVLEN_RBAUD},
	{"pagecolor", 		"", NVOFF_PGCOLOR, NVLEN_PGCOLOR},
	{"sgilogo", 		"", NVOFF_SGILOGO, NVLEN_SGILOGO},
	{"nogui", 		"", NVOFF_NOGUI, NVLEN_NOGUI},
	{"netaddr", 		"", NVOFF_NETADDR, NVLEN_NETADDR},
	{"passwd_key", 		"", NVOFF_PASSWD_KEY, NVLEN_PASSWD_KEY},
	{"netpasswd_key", 	"", NVOFF_NETPASSWD_KEY, NVLEN_NETPASSWD_KEY},
	{"piggyback_reads",	"", NVOFF_PIGGYBACK, NVLEN_PIGGYBACK},
	{"rebound",		"", NVOFF_REBOUND, NVLEN_REBOUND},
	{"fru",                 "", NVOFF_FRUDATA, NVLEN_FRUDATA},
	{0, 0, 0, 0}
};


/* lock for nvram */
static mutex_t nvram_lock;
int check_checksum(void);

/*
 * flag for logging to NVRAM, prevents more than one thread from entering.
 * Also, nvram_logfailures is used to log the number of failures we got
 * when writing to the FRU nvram region. This is rare, and has not been 
 * seen yet, but we leave it here for future exploration.
 */
volatile int in_nvram_logger = 0;
int nvram_logfailures = 0;

/*
 * Forward declarations
 */
int set_an_nvram(int, int, char*);
char nvchecksum(void);
unchar get_nvreg(uint);
int set_nvreg(uint, unchar);
void update_checksum(void);
void get_fru_nvram(char* buffer);
void set_fru_nvram(char* error_dumpbuf, int len);
int clr_fru_nvram(void);


/* init for lock*/
void
nvram_init(void)
{
    mutex_init(&nvram_lock, MUTEX_DEFAULT, "nvram_lock");
}


void
set_pwr_fail(uint p)
{
  int i;

  /* write into powerfail offset, which keeps system uptime */
  EPC_SETBYTE(EPC_NVRTC + NVR_XRAMPAGE, XRAM_PAGE(NVOFF_PWRFAILTIME));
  
  for (i = 0; i < sizeof(p); i++)
    EPC_SETBYTE(EPC_NVRTC + XRAM_REG(NVOFF_PWRFAILTIME+i),
		((unsigned char *)&p)[i]);
}


/*
 * set_nvram()
 *	This function looks up the variable in the table of
 *	valid nvram variables and then sets the variable to
 * 	a new value.  It is called by syssgi.
 */

int
set_nvram(char *name, char *value)
{
        register struct nvram_entry *nvt;
        int valuelen = strlen(value);
        char _value[20];
        int _valuelen = 0;

        /* Don't allow setting the password from Unix, only clearing. */
        if (!strcmp(name, "passwd_key") && valuelen)
                return -2;
#ifdef COMMENT
	/* Don't allow setting the fru output from Unix, only clearing */
	if(!strcmp(name,"fru") && valuelen)
	  return -2;
#endif

        /* change the netaddr to the nvram format */
        if (strcmp(name, "netaddr") == 0) {
                char buf[4];
                char *ptr = value;
                int i;

		/* Save user-readable format of netaddr */
                strcpy(_value, value);
                _valuelen = valuelen;

                /* to the end of the string */
                while (*ptr)
                        ptr++;

                /* convert string to number, one at a time */
                for (i = 3; i >= 0; i--) {
			while (*ptr != '.' && ptr >= value)
				ptr--;
			buf[i] = atoi(ptr + 1);
			if (ptr > value)
				*ptr = 0;
			
		}
		value[0] = buf[0];
                value[1] = buf[1];
                value[2] = buf[2];
                value[3] = buf[3];
                valuelen = 4;
        } 

        /* check to see if it is a valid nvram name */
        for(nvt = nvram_tab; nvt->nt_nvlen; nvt++) {
                if(!strcmp(nvt->nt_name, name)) {
                        int status;

                        if(valuelen > nvt->nt_nvlen)
                                return NULL;
                        if(valuelen < nvt->nt_nvlen)
                                ++valuelen;     /* write out NULL */
                        status = set_an_nvram(nvt->nt_nvaddr, valuelen, value);

			/* Restore the original netaddr string so that
			 * kopt_set doesn't get confused.
 			 */
			if (strcmp("netaddr", name) == 0) {
				strcpy(value, _value);
				valuelen = _valuelen;
			}

                        return status;
		}
	}

	return -1;
}

void
get_pwr_fail(char *v)
{
	int i;

	EPC_SETBYTE(EPC_NVRTC + NVR_XRAMPAGE, XRAM_PAGE(NVOFF_PWRFAILTIME));

	for (i = 0; i < 4; i++)
		v[i] = EPC_GETBYTE(EPC_NVRTC + XRAM_REG(NVOFF_PWRFAILTIME+i));
	v[i] = NULL;
}


/* NVRAM error logging. See pv #519323.
 * Loads a reserved region of NVRAM with up to NVLEN_FRUDATA bytes
 * of error information. This is used by the fru to save the analysis 
 * done by the analyzer before we are dumping core. We also save the 
 * panic string in this region as well. Only *one* thread can enter this
 * routine and write data to the FRU nvram.
 */
void
set_fru_nvram(char* bufp, int len)
{
  int i,s;

  /* Under debug kernels, in assertion failure code,
   * we make a call to debug_stop_all_cpus() which will inevitably 
   * interrupt this code before it completes either the loop below or
   * the call to update_checksum() which needs to happen in order to avoid
   * having the prom scrub the NVRAM. See PV #525192. To avoid this problem
   * we execute all NVRAM logging code under spl7(). 
   */

  s = spl7();

  /* The first thread which gets to this code will get the initial value
   * of the flag (0), and set the value to it's cpuid. Subsequent calls 
   * to this routine (after the flag has been set) will return the first
   * threads cpuid in which case the code should return.When done, we 
   * reset the flag to 0. This way,we don't have multiple messages from
   * each CPU and guarantee mutex -there can be only one.
   */
  if(test_and_set_int((int*)&in_nvram_logger, (0x100 | cpuid())) !=0){
    splx(s);
    return ;
  }

  /* On large systems, when PANIC string output is large, writing more
   * than NVLEN_FRUDATA -1 bytes to the FRU NVRAM will have the effect
   * of writing to the first byte (where the checksum is stored). Since
   * the address-space "wraps-around", we stop writing at NVLEN_FRUDATA -2
   * (need space for null byte) and simply don't log all the panic output.
   */
  for (i = 0; (i < len) && (bufp[i] !='\0') && (i < NVLEN_FRUDATA -2); i++){
    if(set_nvreg(NVOFF_FRUDATA+i, bufp[i]))
      nvram_logfailures++;
  }

  /* Write null byte */
  if(set_nvreg(NVOFF_FRUDATA+i, '\0'))
      nvram_logfailures++;

  /* Always update */
  update_checksum();

  /* 
   * Reset so someone else can come along.
   */
  in_nvram_logger = 0;

  splx(s);
}

/*
 * Get whatever is in the FRU NVRAM and store it in the buffer.
 * Note: buffer HAS TO BE AT LEAST NVLEN_FRUDATA bytes in size.
 */
void
get_fru_nvram(char* buffer)
{
  int i =0;
  char c = 'j';
  do{
    c = get_nvreg(NVOFF_FRUDATA+i);
    buffer[i++] = c ;
  }  while((i < NVLEN_FRUDATA -2) && (c != '\0'));
  buffer[i] = '\0';
}


/*
 * set_an_nvram
 *	Writes a string of a specified length to a specied
 *	offset in the NVRAM.
 */

int
set_an_nvram(int nv_off, int nv_len, char *string)
{
	uint i;
	int ret = 0; 

	if ((nv_off + nv_len) >= NVLEN_MAX)
		return -1;

	if(!check_checksum()) return -1;
	mutex_lock(&nvram_lock, 0);

	for (i = 0; i < nv_len; i++) {
	    if (set_nvreg(i+nv_off, string[i])) {
		ret = -1;
		break;
	    }
	}
	if(ret != -1) update_checksum();
	mutex_unlock(&nvram_lock);

	return ret;
}

/*
 * nvchecksum()
 *	Calculates the checksum for the NVRAM attached to the master
 *	IO4 EPC.
 */
char
oldnvchecksum(void)
{
	register ulong nv_reg;
	unchar nv_val;
	signed char checksum = 0xa5;

	/*
	 * do the checksum on all of the nvram, skip the checksum byte.
	 */
	for (nv_reg = 0; nv_reg < NVLEN_MAX; nv_reg++) {
		nv_val = get_nvreg(nv_reg & 0x3e0);
		if (nv_val != NVOFF_OLD_CHECKSUM)
		       checksum ^= nv_val;

		/* following is a tricky way to rotate */
		checksum = (checksum << 1) | (checksum < 0);
	}

	return (char)checksum;
}


char 
nvchecksum(void)
{
        register ulong nv_reg;
        unchar nv_val;
        signed char checksum = 0xa5;

	/* Support old broken nvchecksum for awhile */
	if (get_nvreg(NVOFF_REVISION) <= NV_BROKEN_REV)
	    return oldnvchecksum();

        /*
         * do the checksum on all of the nvram, skip the checksum byte.
         */
        for (nv_reg = 0; nv_reg < NVOFF_HIGHFREE; nv_reg++) {
                nv_val = get_nvreg(nv_reg);
                if (nv_reg != NVOFF_NEW_CHECKSUM &&
		    nv_reg != NVOFF_RESTART)
                       checksum ^= nv_val;

                /* following is a tricky way to rotate */
                checksum = (checksum << 1) | (checksum < 0);
        }

        return (char)checksum;
}


/*
 *  check_checksum
 *	Checks the old and new checksum values 
 */

int
check_checksum(void)
{
    return(get_nvreg(NVOFF_OLD_CHECKSUM) == (unchar) oldnvchecksum() &&
	   get_nvreg(NVOFF_NEW_CHECKSUM) == (unchar)  nvchecksum());
}

/*
 * update_checksum
 *	Updates the old and new checksum values 
 */

void
update_checksum(void)
{
	set_nvreg(NVOFF_OLD_CHECKSUM, oldnvchecksum());
	set_nvreg(NVOFF_NEW_CHECKSUM, nvchecksum());
}

/*
 * get_nvreg()
 *      Read a byte from the NVRAM.  If an error occurs, return -1, else
 *      return 0.
 */

unchar
get_nvreg(uint offset)
{
	/* First set up the XRAM Page register */
	EPC_SETBYTE(EPC_NVRTC + NVR_XRAMPAGE, XRAM_PAGE(offset));

	return EPC_GETBYTE(EPC_NVRTC + XRAM_REG(offset));
}


/*
 * set_nvreg()
 *      Writes a byte into the NVRAM
 */

int
set_nvreg(uint offset, unchar byte)
{
	unchar value;

	/* Write the byte */
	EPC_SETBYTE(EPC_NVRTC + NVR_XRAMPAGE, XRAM_PAGE(offset));
	EPC_SETBYTE(EPC_NVRTC + XRAM_REG(offset), byte);

	/* Read it back to make sure it wrote correctly */
	value = EPC_GETBYTE(EPC_NVRTC + XRAM_REG(offset));

	if (value == byte)
		return 0;
	else
		return -1;
}

/* local defines to get to nvram location of cpu enable/disable */
#define INV_OFFBOARD(_b) (NVOFF_INVENTORY + ((_b) * INV_SIZE))
#define INV_OFFUNIT(_b, _u) \
    (INV_OFFBOARD(_b) + INV_UNITOFF + ((_u) * INV_UNITSIZE))

/*
 * nvram_enable_cpu_set
 * 	Set the cpu number given to have the enable/disable given
 */ 
int nvram_enable_cpu_set(cpuid_t cpu_num, int enable) 
{
    int slot, unit, ret;
    
    slot = cpuid_to_slot[cpu_num];
    unit = cpuid_to_cpu[cpu_num];

    if(!check_checksum()) return -1;
    mutex_lock(&nvram_lock, 0);

    ret = set_nvreg((INV_OFFUNIT(slot, unit) + INVU_ENABLE), 
		    (unsigned char) enable);
    if(ret != -1) ret = set_nvreg((INV_OFFUNIT(slot, unit) + INVU_DIAGVAL), 
				  (unsigned char) EVDIAG_FPU);
    if(ret != -1) update_checksum();
    
    mutex_unlock(&nvram_lock);
    return ret;
}
