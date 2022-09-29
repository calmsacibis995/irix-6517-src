/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <string.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/SN/nvram.h>
#include <sys/SN/klconfig.h>
#include "sn_private.h"
#include <sys/debug.h>

#if defined (SN0)
#include <sys/SN/SN0/klhwinit.h>		/* IOC3_NVRAM_OFFSET 	*/
#include <sys/SN/SN0/ip27log.h>
#endif

/* Forward declarations */
void nvram_setserial(char *);
int  nvram_getserial(char *);

static char nvchecksum(void) ;
static void get_nvram_buf(int, int, char *);

/*
 * Standard nvram lookup table
 */
static struct nvram_entry nvram_tab[] = {
        {"SystemPartition",     "", NVOFF_SYSPART, NVLEN_SYSPART},
        {"OSLoadPartition",     "", NVOFF_OSPART, NVLEN_OSPART},
        {"OSLoader",            "", NVOFF_OSLOADER, NVLEN_OSLOADER},
        {"OSLoadFilename",      "", NVOFF_OSFILE, NVLEN_OSFILE},
        {"OSLoadOptions",       "", NVOFF_OSOPTS, NVLEN_OSOPTS},
        {"AutoLoad",            "", NVOFF_AUTOLOAD, NVLEN_AUTOLOAD},
        {"dbgtty",              "", NVOFF_DBGTTY, NVLEN_DBGTTY},
        {"root",                "", NVOFF_ROOT, NVLEN_ROOT},
        {"swap",                "", NVOFF_SWAP, NVLEN_SWAP},
        {"TimeZone",            "", NVOFF_TZ, NVLEN_TZ},
        {"console",             "", NVOFF_CONSOLE, NVLEN_CONSOLE},
        {"diagmode",            "", NVOFF_DIAGMODE, NVLEN_DIAGMODE},
        {"diskless",            "", NVOFF_DISKLESS, NVLEN_DISKLESS},
        {"nogfxkbd",            "", NVOFF_NOKBD, NVLEN_NOKBD},
        {"keybd",               "", NVOFF_KEYBD, NVLEN_KEYBD},
        {"lang",                "", NVOFF_LANG, NVLEN_LANG},
    	{"volume", 		"", NVOFF_VOLUME, NVLEN_VOLUME},
        {"scsiretries",         "", NVOFF_SCSIRT, NVLEN_SCSIRT},
        {"scsihostid",          "", NVOFF_SCSIHOSTID, NVLEN_SCSIHOSTID},
        {"dbaud",               "", NVOFF_LBAUD, NVLEN_LBAUD},
        {"rbaud",               "", NVOFF_RBAUD, NVLEN_RBAUD},
        {"pagecolor",           "", NVOFF_PGCOLOR, NVLEN_PGCOLOR},
        {"sgilogo",             "", NVOFF_SGILOGO, NVLEN_SGILOGO},
        {"nogui",               "", NVOFF_NOGUI, NVLEN_NOGUI},
        {"netaddr",             "", NVOFF_NETADDR, NVLEN_NETADDR},
        {"passwd_key",          "", NVOFF_PASSWD_KEY, NVLEN_PASSWD_KEY},
        {"netpasswd_key",       "", NVOFF_NETPASSWD_KEY, NVLEN_NETPASSWD_KEY},
        {"rebound",             "", NVOFF_REBOUND, NVLEN_REBOUND},
        {"ProbeAllScsi",        "", NVOFF_PROBEALLSCSI, NVLEN_PROBEALLSCSI},
        {"ProbeWhichScsi",      "", NVOFF_PROBEWHICHSCSI, NVLEN_PROBEWHICHSCSI},
        {0, 0, 0, 0}
};

#define	SERIALNUMSIZE	12

static int set_nvram_offset(int, int, char*);
__psunsigned_t	nvram_base ;

/*
 * Initialize the nvram_base that will be used for all other
 * nvram activities 
 */
void
nvram_baseinit(void)
{ 

	/*
	 * For now, assume the nvram we use is on the Same base io
	 * as the consoel. So, we tweak the console base
	 * to get to nvram base..
	 */
	nvram_base = KL_CONFIG_CH_CONS_INFO(master_nasid)->memory_base + 
				IOC3_NVRAM_OFFSET;
}

/*
 * get_nvreg()
 *	Read a byte from the NVRAM.  If an error occurs, return -1, else
 *	return 0.
 */

unchar
get_nvreg(uint offset)
{
    if (nvram_base)
	return(*(unchar *)(nvram_base+offset)) ;

    return (unchar)0;
}


/*
 * set_nvreg()
 *	Writes a byte into the NVRAM
 */

int
set_nvreg(uint offset, unchar byte)
{
    if (nvram_base)
	*((unchar *)(nvram_base+offset)) = byte ;

    return 0;
}

/*
 * get_nvram_buf()
 *	Read a string of given length from nvram starting
 *	at a specified offset.  The characters read are written
 *	into the given buffer.  The buffer is null-terminated.
 */

void
get_nvram_buf(int nv_off, int nv_len, char buf[])
{
    uint i;
    
    /* Avoid overruns */
    if ((nv_off + nv_len) >= NVLEN_MAX) {
	*buf = '\0';
	return;
    }
  
    /* Transfer the bytes into the array */ 
    for (i = 0; i < nv_len; i++)
       	buf[i] = get_nvreg(i+nv_off);

    /* Null-terminate the string */
    buf[i] = 0;
}


/*
 * get_nvram_offset()
 *	Read a string of given length from nvram starting at
 *	a specified offset.  The routine returns a pointer to
 *	a static buffer containing the string.
 */
char *
get_nvram_offset(int nv_off, int nv_len)
{
    static char buf[128];

    get_nvram_buf(nv_off, nv_len, buf);
    return buf;
}

/*
 * nvchecksum()
 *	Calculates the checksum for the NVRAM attached to the master IO6.
 */
char 
nvchecksum(void)
{
    register ulong nv_reg;
    unchar nv_val;
    signed char checksum = 0xa5;

    /*
     * do the checksum on all of the nvram, skip the checksum byte !!
     */
    for (nv_reg = NVOFF_REVISION; nv_reg < NVOFF_HIGHFREE; nv_reg++) {
	nv_val = get_nvreg(nv_reg);
	if (nv_reg != NVOFF_CHECKSUM &&
	    nv_reg != NVOFF_RESTART)
	    checksum ^= nv_val;

	/* following is a tricky way to rotate */
        checksum = (checksum << 1) | (checksum < 0);
    }

    return (char)checksum;
}

void
update_checksum(void)
{
	set_nvreg(NVOFF_CHECKSUM, nvchecksum());
}

/*
 * set_nvram_offset
 *	Write the given string to nvram at an offset.  If operation
 *	fails, return -1, otherwise return 0.
 */

int
set_nvram_offset(int nv_off, int nv_len, char *string)
{
    uint i;

    if ((nv_off + nv_len) >= NVLEN_MAX)
	return -1;


    for (i = 0; i < nv_len; i++) {
	if (set_nvreg(i+nv_off, string[i]))
	    return -1;
    }

    update_checksum();

    return 0;
} 


/*
 * set_nvram
 *	Set the NVRAM RAM string whose name is passed as a parameter
 *	to the given value.  This just calls back to the set_nvram
 *	routine.
 */

int
set_nvram(char *name, char *value)
{
	struct nvram_entry	*nvt;
	int	valuelen = strlen(value);
	char	_value[20];
	int	_valuelen = 0;
	int	i;
	/*
	 * Reject setting passws_key from unix.
	 */
	
	if (!strcmp(name, "passwd_key") && valuelen)
		return -2;

	/* If netaddr, convert it to nvram format */
	if (strcmp(name, "netaddr") == 0) {
		char	buf[4];
		char	*ptr = value;

		strcpy(_value, value);
		_valuelen = valuelen;
		/* 
		 * Start from end of string.
		 */
		while (*ptr)
			*ptr++;
		
		/* Convert string to a number. */
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

	/* Check to see if it is a valid nvram name */
	for (nvt = nvram_tab; nvt->nt_nvlen; nvt++) {
                if(!strcmp(nvt->nt_name, name)) {
                        int status;

                        if(valuelen > nvt->nt_nvlen)
                                return NULL;
                        if(valuelen < nvt->nt_nvlen)
                                ++valuelen;     /* write out NULL */
                        status = set_nvram_offset(nvt->nt_nvaddr, valuelen, value);

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

/*
 * cpu_is_nvvalid()
 *	Returns 1 if it looks like the NVRAM is correct, 0 otherwise.
 */

int
cpu_is_nvvalid(void)
{
    char csum;

    csum = nvchecksum();
#ifdef	DEBUG
    printf("csum: 0x%x get_nvreg(NVOFF_CHECKSUM): 0x%x\n",
		csum, get_nvreg(NVOFF_CHECKSUM));
#endif

    if (csum != get_nvreg(NVOFF_CHECKSUM))  {
	csum = nvchecksum();
	printf("csum: 0x%x get_nvreg(NVOFF_CHECKSUM): 0x%x\n",
		csum, get_nvreg(NVOFF_CHECKSUM));
	if (csum != get_nvreg(NVOFF_CHECKSUM))
		return 0;
    }

    printf("NV_CURRENT_REV: 0x%x get_nvreg(NVOFF_REVISION): 0x%x\n",
	NV_CURRENT_REV, get_nvreg(NVOFF_REVISION));


#if 0
    /* try twice */
    if (nvchecksum() != get_nvreg(NVOFF_CHECKSUM) &&
	nvchecksum() != get_nvreg(NVOFF_CHECKSUM))
	return 0;
 
#endif
    if (NV_CURRENT_REV != get_nvreg(NVOFF_REVISION))
	return 0;

    return 1;
}

#if NOT_NEEDED_YET
/*
 * cpu_update_nvram()
 * 	Update the old nvram versions to the current level.  This is only
 * 	called if it seems like the old version of the prom is otherwise
 * 	valid.  All we need to do is initialize the new variables to 
 * 	reasonable values and reset the checksum.
 */

void
cpu_update_nvram(void)
{
#if 1
    printf("fixme: cpu_update_nvram\n"); /* XXX */
#else
    char serial[SERIALNUMSIZE];
    int i;
    extern int read_serial(char *);
    extern void initialize_envstrs(void);
   
    for (i = NVOFF_LOWFREE; i < NVOFF_HIGHFREE; i++)
	set_nvreg(i, 0);
 
   set_nvram_offset(NVOFF_REBOUND, NVLEN_REBOUND, REBOUND_DEFAULT);

    /* Update the revision number */
    set_nvreg(NVOFF_REVISION, NV_CURRENT_REV);

    /* Retrieve the serial number from the system controller. 
     * Since read_serial() will repair the nvram serial # if
     * it appears to be bogus, this will effectively update 
     * copy the serial number into the nvram.
     */
     if (read_serial(serial) == -1)
	nvram_invalid_serial();

    update_checksum();

    initialize_envstrs();
#endif
}

/*
 * valid_serial()
 *      Checks to see whether the serial number looks valid.
 *      If it does, return 1.  If it looks like the serial number
 *      has been trashed, return 0.  Note that this routine
 *      disallows serial numbers > 8 bytes in length.
 */

int
valid_serial(char *serial)
{
        int     i;

        if (!serial || !strlen(serial))
                return 0;

        /* The first character of the serial number must be alphabetic */
        if (serial[0] >= 'a' && serial[0] <= 'z')
                serial[0] -= 0x20;
        if (serial[0] < 'A' || serial[0] > 'Z')
             return 0;

        i = 1;

        /* The remaining characters must be numeric */
        while(serial[i] != '\0') {
                if ((serial[i] < '0') || (serial[i] > '9') || (i >= 8)) {
                        return 0;
                }
                i++;
        }

        return 1;
}


/*
 * read_serial()
 * 	Reads the serial number from both the system controller and
 * 	NVRAM, reconciles the results, and puts the result in serial.
 */

/*ARGSUSED*/
int
read_serial(char *serial)
{
    cmn_err(CE_NOTE, "fixme: read_serial\n"); /* XXX */
    return 0;
}


/*
 * write_serial()
 *	Update the system controller and NVRAM versions of the
 *	serial number.
 */

int
write_serial(char *serial)
{
    if (!valid_serial(serial)) {
	printf("ERROR: write_serial received invalid serial number\n");
	return -1;
    }

    nvram_setserial(serial);
#if 0
    sysctlr_setserial(serial);
#endif
    return 1 ;
}

/*
 * nvram_getserial
 *	Read the serial number from the nvram.
 */

int
nvram_getserial(char *serial)
{
    unchar len;

    if (get_nvreg(NVOFF_SNUMVALID) != NVRAM_SNUMVALID) {
	return -1;
    }
 
    if ((len = get_nvreg(NVOFF_SNUMSIZE)) > NVLEN_SERIALNUM) {
	return -1;
    }

    get_nvram_buf(NVOFF_SERIALNUM, len, serial);
    if (!valid_serial(serial))
	return -1;
    else
	return 0;
} 


/*
 * nvram_setserial(char *serial)
 *	Write the given serial number string into nvram.
 */

void
nvram_setserial(char *serial)
{
    unchar	len;
    printf("fixme: nvram_setserial\n"); /* XXX */

    len = MIN(SERIALNUMSIZE, strlen(serial));

    set_nvram_offset(NVOFF_SERIALNUM, len, serial);
    set_nvreg(NVOFF_SNUMSIZE, len);
    set_nvreg(NVOFF_SNUMVALID, NVRAM_SNUMVALID);
    update_checksum();
}


/*
 * nvram_invalidate_serial()
 *	Invalidate the serial number in the nvram.
 */

void
nvram_invalid_serial(void)
{
    set_nvreg(NVOFF_SNUMVALID, 0x0);
    set_nvreg(NVOFF_SNUMSIZE, 0x0);
}


#endif	/* NOT_NEEDED_YET */

#ifdef	DEBUG
/*
 * Routine which reads from nvram and dumps the data..
 */

void
read_nvram()
{
	struct 	nvram_entry	*nvt;
	char	*nvname;

	for (nvt = nvram_tab; nvt->nt_nvlen != 0; nvt++) {
		nvname = get_nvram_offset(nvt->nt_nvaddr, nvt->nt_nvlen);

		printf("offset %d length %d nvname: %s table name : %s\n", 
			nvt->nt_nvaddr, nvt->nt_nvlen, nvname, nvt->nt_name);
	}
}

#endif	/* DEBUG */

/* Initialize the nvram table for disabled io components */
void
nvram_dict_init(void)
{
	unchar index = 0;
	/* Set the magic number for this table */
	nvram_dict_magic_set();
	/* Set the table size to 0 (empty) */
	nvram_dict_size_set(0);
	/* Initialize all the entries to being empty */
	for(index = 0; index < MAX_DISABLEDIO_COMPS ; index++)
		free_index_set(index);
	update_checksum();
}

/* Get the current # of disabled io components from nvram */
unchar
nvram_dict_size(void)
{
	/* Make sure that the disable io component table has
	 * been initialized
	 */
	if (!nvram_dict_magic_check())
		nvram_dict_init();
	return(nvram_dict_size_get());
}
/* Get the first available free slot for storing a (mod,slot,comp) triple
 * for the disabled io component.
 * On failure returns (nvram_dict_invalid_value);
 */
unchar
nvram_dict_first_free_index_get(void)
{
	unchar	index = 0;

	/* Make sure that the disable io component table has
	 * been initialized
	 */
	if (!nvram_dict_magic_check())
		nvram_dict_init();

	while (valid_index(index) && !free_index_check(index))
		index++;
	/* Make sure that the index is valid. */
	if (!valid_index(index))
		return(nvram_dict_invalid_value);
	return(index);
	
}
/* 
 * Get the next filled  entry in the table starting from index.
 * On failure returns (nvram_dict_invalid_value);
 */
unchar
nvram_dict_next_index_get(unchar index)
{

	/* Make sure that the disable io component table has
	 * been initialized
	 */
	if (!nvram_dict_magic_check())
		nvram_dict_init();

	while(valid_index(index) && free_index_check(index))
		index++;
	/* Make sure that the index is valid */
	if (!valid_index(index))
		return(nvram_dict_invalid_value);
	return(index);
}
/* Store the information in the disable io component table into the
 * nvram.
 */
int
nvram_dict_index_set(dict_entry_t *dict,unchar index)
{
	unchar	       	current_size;

	/* Make sure that the disable io component table has
	 * been initialized
	 */
	if (!nvram_dict_magic_check())
		nvram_dict_init();

	/* Get the # of components that have already been disabled
	 * (stored in the nvram)
	 */
	current_size = nvram_dict_size_get();
	/* check that 0 <= index <= current_size . NOTE the +1 is due to 
	 * the fact that caller might be trying to write a completely
	 * new nvram entry 
	 */
	if (!valid_index(index))
		return(-1);
	/* If this is a free index then we are adding a new triple
	 * so increment the number of components.
	 */
	if (free_index_check(index)) {
		current_size++;
		/* Store the new size into the nvram */
		nvram_dict_size_set(current_size);
	}
	/* Store the <module,slot,component>  byte triple in the
	 * nvram for this new disabled io component
	 */
	nvram_disable_io_component(index,
				   dict->module,
				   dict->slot,
				   dict->comp);
	update_checksum();
	return(0);
}

/* Read the information from the nvram about disabled io components 
 * and put it into the disabled io component table
 */
int	
nvram_dict_index_get(dict_entry_t *dict,unchar index)
{
	/* Make sure that the disable io component table has
	 * been initialized
	 */
	if (!nvram_dict_magic_check())
		nvram_dict_init();

	if (!valid_index(index)||
	    !dict)
		return(-1);		/* Got an invalid paramters*/

	dict->module 	= nvram_dict_module_get(index);
	dict->slot 	= nvram_dict_slot_get(index);
	dict->comp 	= nvram_dict_comp_get(index);

	return(0);
}

/* Remove the information about a disabled component(usually when it is
 * enabled). Also called when initialzing this nvram table
 */
void
nvram_dict_index_remove(unchar index)
{
	unchar	size;
	/* Make sure that the disable io component table has
	 * been initialized
	 */
	if (!nvram_dict_magic_check())
		return;
	size = nvram_dict_size_get();
	/* Table is empty !!*/
	if (size == 0)
		return;
	free_index_set(index);
	size--;		
	nvram_dict_size_set(size);
	update_checksum();
}

/* Routines to get & set the master baseio prom version number
 * stored in the nvram 
 */
void
nvram_prom_version_set(unsigned char version,unsigned char revision)
{
	set_nvreg(NVOFF_VERSION,version);
	set_nvreg(NVOFF_VERSION+1,revision);
	update_checksum();

}

void
nvram_prom_version_get(unsigned char *version,unsigned char *revision)
{
	*version = get_nvreg(NVOFF_VERSION);
	*revision = get_nvreg(NVOFF_VERSION+1);
}


#if defined (SN0)
/*
 * nvram_enable_cpu_set
 * 	Set the cpu number given to have the enable/disable given
 */ 
int nvram_enable_cpu_set(cpuid_t cpu_num, int enable) 
{
    nasid_t nasid;
    int slice;
    char *key_string;

    nasid = cputonasid(cpu_num);
    slice = cputoslice(cpu_num);
    if(!slice) key_string = DISABLE_CPU_A;
    else key_string = DISABLE_CPU_B;
    
    /* ip27log_setenv already has a lock in it */
    if(enable) return(ip27log_unsetenv(nasid, key_string, 0));
    return(ip27log_setenv(nasid, key_string,
			  DISABLE_CPU_IP27LOG, 0));
}
#endif

void
set_part_reboot_nvram(char *val) 
{
	set_nvram_offset(NVOFF_RESTART, 1, val) ;
}

