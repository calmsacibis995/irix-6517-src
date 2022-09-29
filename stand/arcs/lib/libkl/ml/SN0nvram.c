/**********************************************************************\
*       File:           libkl/ml/SN0nvram.c                             *
*    This file is derived from the libsk's SN0nvram.c.                  *
*    It contains routines that are nvram specific and                   *
*    required by the CPU prom                                           *
*                                                                       *
\***********************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/SN/nvram.h>
#include <libsc.h>
#include <libsk.h>
#include <libkl.h>
#include <kllibc.h>
#include <sys/SN/gda.h> /* For PROMOP definitions */
#include <sys/SN/SN0/klhwinit.h>
#include <sys/PCI/ioc3.h>
#include <sys/SN/addrs.h>

/*
 * The following defines are present here because this
 * file contains the same routines as in libsk/SN0nvram.c.
 * Except that these routines take the nvram_base as an arg.
 */

#define get_nvreg		_get_nvreg
#define nvchecksum 		_nvchecksum

#define cpu_is_nvvalid		_cpu_is_nvvalid
#define cpu_get_nvram_buf	_cpu_get_nvram_buf

/*
 * The following 2 routines are the same as the
 * 'slv_' routines in libsk/SN0nvram.c. libsk does
 * not use these as the size of the io6prom may increase.
 */

static unchar
get_nvreg(__psunsigned_t base, uint offset)
{

    if (base)
        return(*(unchar *)(base+offset)) ;

    return (unchar)0;
}

/*
 * nvchecksum()
 *      Calculates the checksum for the NVRAM attached to the master IO6.
 */
static char
nvchecksum(__psunsigned_t nv_base)
{
    register ulong nv_reg;
    unchar nv_val;
    signed char checksum = 0xa5;

    /*
     * do the checksum on all of the nvram, skip the checksum byte !!
     * Start after the Dallas TOT registers.
     */
    for (nv_reg = NVOFF_REVISION; nv_reg < NVOFF_HIGHFREE; nv_reg++) {
        nv_val = get_nvreg(nv_base, nv_reg);
        if (nv_reg != NVOFF_CHECKSUM &&
            nv_reg != NVOFF_RESTART)
            checksum ^= nv_val;

        /* following is a tricky way to rotate */
        checksum = (checksum << 1) | (checksum < 0);
    }
    return (char)checksum;
}


/*
 * cpu_is_nvvalid()
 *      Returns 1 if it looks like the NVRAM is correct, 0 otherwise.
 */

int
cpu_is_nvvalid(__psunsigned_t nv_base)
{
    int rc = 1;

    /* try twice */
    if (nvchecksum(nv_base) != get_nvreg(nv_base, NVOFF_CHECKSUM) &&
        nvchecksum(nv_base) != get_nvreg(nv_base, NVOFF_CHECKSUM))
    {
        rc = 0;
    }

    if (NV_CURRENT_REV != get_nvreg(nv_base, NVOFF_REVISION))
    {
        rc = 0;
    }
    return rc;
}


void
cpu_get_nvram_buf(__psunsigned_t base, int nv_off, int nv_len, char buf[])
{
    uint i;

    /* Avoid overruns */
    if ((nv_off + nv_len) >= NVLEN_MAX) {
        *buf = '\0';
        return;
    }
 
    /* Transfer the bytes into the array */
    for (i = 0; i < nv_len; i++) {
        buf[i] = get_nvreg(base, i+nv_off);
	delay(200) ;
    }

    /* Null-terminate the string */
    buf[i] = 0;
}


#define MAX_MODULE_NAME_LEN     4
#define MAX_SLOT_NAME_LEN       2
/*
 * check_console_path:
 * Checks the path got from nvram for the env var ConsolePath.
 * Returns : -1 if path is invalid
 *            0 if path is set to CONSOLE_PATH_DEFAULT - "default"
 *            (modid << 16) | slotid - if valid values are found.
 */
check_console_path(char *path)
{
        char *string1 = "/hw/module/" ;
        char *string2 = "/slot/io" ;
	char *string3 = "/slot/MotherBoard" ;
        char tmp[32] ;
        unsigned short  modid = 0, slotid = 0 ;
        int i;

        if (!path)
                return -1 ;

	if (!(*path))      /* variable not found in nvram */
		return 0 ;

	if (!strcmp(path, CONSOLE_PATH_DEFAULT))
		return 0 ;

        *tmp=0;
        if (strncmp(path, string1, strlen(string1)))
                return -1 ;

        path += strlen(string1) ;

        for (i=0; i<(MAX_MODULE_NAME_LEN+1); i++)
            if (*(path+i)){
                if (*(path+i) == '/')
                        break ;
                else if (!isdigit(*(path+i)))
                        return -1 ;
            } else {
                return -1 ;
            }

        if ((i==(MAX_MODULE_NAME_LEN+1)) || (i==0))
                return -1 ;

        strncpy(tmp, path, i) ;
	tmp[i] = 0 ;
        modid = atoi(tmp) ;

        *tmp=0;
        path += i ;
        if (strncmp(path, string2, strlen(string2))) {
		/*
		 * Check if this is baseio on the SN00 motherboard
		 */
		if (strncmp(path, string3, strlen(string3)))
			return -1 ;
		/*
		 * On speedo we look like slot 25
		 */
		return((modid << 16) + 25);
	}
        path += strlen(string2) ;
        for (i=0; i<(MAX_SLOT_NAME_LEN+1); i++)
                if (*(path+i)) {
                        if (!isdigit(*(path+i)))
                                return -1 ;
                }
                else
                        break ;

        if ((i==(MAX_SLOT_NAME_LEN+1)) || (i==0))
                return -1 ;

        strncpy(tmp, path, i) ;
	tmp[i] = 0 ;
        slotid = atoi(tmp) ;
	if (slotid > MAX_IO_SLOT_NUM)
		return -1 ;

        return(((modid<<16)+slotid)) ;

}

int
get_nvram_modid(volatile __psunsigned_t base, nic_t xnic)
{
	return 1 ;
}

char
diag_get_reboot()
{
	int 	ll ;
	char 	rv ;

	ll = diag_get_lladdr() ;
	if ((ll & PROMOP_REBOOT) == PROMOP_REBOOT)
		rv = (SCSI_TO_LL_MAGIC | SCSI_TO_LL_REBOOT) ;
	else
		rv = 0 ;

	return rv ;
}

int check_console(pcfg_hub_t *hub)
{
	partid_t curr, spid;
	uchar magic;
	char buf[NVLEN_STORE_CONSOLE_PATH];
	char check_buf[16];
	console_t *cons;
	__psunsigned_t base;
	cons = KL_CONFIG_CH_CONS_INFO(get_nasid());
	if(cons==NULL)
		return 0;
	base = TO_NODE(get_nasid(),cons->uart_base - IOC3_SIO_UA_BASE + IOC3_NVRAM_OFFSET );
	curr = hub->partition;
	magic = get_nvreg(base,NVOFF_PART_MAGIC);
        if(magic == PART_MAGIC)
                spid    = get_nvreg(base,NVOFF_STORE_PARTID);
	else
		return 0;
	if(curr == spid)
	{
		cpu_get_nvram_buf(base,NVOFF_STORE_CONSOLE_PATH,NVLEN_STORE_CONSOLE_PATH,buf);
		sprintf(check_buf,"module/%d",hub->module);
		if(strstr(buf,check_buf))
			return 1;
		else
			return 0;
	}
	else
		return 0;

}

