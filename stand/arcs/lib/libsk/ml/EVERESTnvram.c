/**********************************************************************\
*	File:		EVERESTnvram.c					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/nvram.h>
#include <libsc.h>
#include <libsk.h>

extern int sk_sable;

extern int sysctl_getserial(char *);

/* Forward declarations */
void nvram_setserial(char *);
int  nvram_getserial(char *);

#define EPC_GETBYTE(_x) \
    ( *((unsigned char*) (SWIN_BASE(EPC_REGION, EPC_ADAPTER) + (_x) + 7)))

#define EPC_SETBYTE(_x, _v) \
    EPC_GETBYTE(_x) = (_v);
 
#if defined(DEBUG)
#define DPRINTF(X) printf X 
#else
#define DPRINTF(X)
#endif

/*
 * get_nvreg()
 *	Read a byte from the NVRAM.  If an error occurs, return -1, else
 *	return 0.
 */

unchar
get_nvreg(ulong offset)
{
    if (sk_sable)
	return (unchar)0;

    /* First set up the XRAM Page register */
    EPC_SETBYTE(EPC_NVRTC + NVR_XRAMPAGE, (unsigned char)XRAM_PAGE(offset));
	
    return EPC_GETBYTE(EPC_NVRTC + XRAM_REG(offset));	
}


/*
 * set_nvreg()
 *	Writes a byte into the NVRAM
 */

int
set_nvreg(ulong offset, unchar byte)
{
    unchar value; 

    if (sk_sable)
	return -1;

    /* Write the byte */
    EPC_SETBYTE(EPC_NVRTC + NVR_XRAMPAGE, (unsigned char)XRAM_PAGE(offset));
    EPC_SETBYTE(EPC_NVRTC + XRAM_REG(offset), byte);  

    /* Read it back to make sure it wrote correctly */
    value = EPC_GETBYTE(EPC_NVRTC + XRAM_REG(offset));

    if (value == byte) {
	return 0;
    } else {
	return -1;	
    }
}


/*
 * nvchecksum()
 *	Calculates the checksum for the NVRAM attached to the master
 *	IO4 EPC.
 */

static char 
oldnvchecksum(void)
{
    register ulong nv_reg;
    unchar nv_val;
    signed char checksum = 0xa5;

    /*
     * do the checksum on all of the nvram, skip the checksum byte !!
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

    if (sk_sable)
	return (char)0;

    /*
     * do the checksum on all of the nvram, skip the checksum byte !!
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

void
update_checksum(void)
{
	set_nvreg(NVOFF_OLD_CHECKSUM, oldnvchecksum());
	set_nvreg(NVOFF_NEW_CHECKSUM, nvchecksum());
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

	/*
	 * We now enforce a serial number beginning with 'S'.
	 */
	if (serial[0] == 's') {
	    serial[0] = 'S'; 
	} else if (serial[0] != 'S') {
	    return(0);
	} 
        i = 1;

        /* The remaining characters must be numeric  - and at least 5 of them*/
        while(serial[i] != '\0') {
                if ((serial[i] < '0') || (serial[i] > '9')) {
                        return 0;
		} else if (i++ > 6) {
		        return 0;
		}
        }

        return i >= 6;
}


/*
 * read_serial()
 * 	Reads the serial number from both the system controller and
 * 	NVRAM, reconciles the results, and puts the result in serial.
 */

int
read_serial(char *serial)
{
    int retries = 6;    /* Try reading it three times */
    char nvram_serial[SERIALNUMSIZE];
    int nv_valid, sysctlr_valid;
    int test_system;

    /* Determine whether the serial number in nvram looks good */
    bzero(nvram_serial, SERIALNUMSIZE);
    nv_valid = !nvram_getserial(nvram_serial);
    DPRINTF(("read_serial: nvram_getserial(%s) is %s\n", 
	     nvram_serial, nv_valid ? "valid" : "invalid"));

    /* Try to get the system controller serial number */
    bzero(serial, SERIALNUMSIZE);
    while (retries-- && (sysctlr_getserial(serial) == -1))
        /* LOOP */ ;
    sysctlr_valid = valid_serial(serial);
    DPRINTF(("read_serial: sysctlr_serial(%s) is %s\n", 
	     serial, sysctlr_valid ? "valid" : "invalid"));

    /* In order to avoid problems with passing on test systems' serial 
     * numbers, we won't propagate serial numbers if a certain combination
     * of debug switches is set.  The theory is that all manufacturing oven
     * systems should have this bit set, and so will never fix up a boards
     * serial number.
     */ 
    test_system = (EVCFGINFO->ecfg_debugsw & (VDS_DEBUG_PROM|VDS_MANUMODE));

    /* If the serial number in the nvram and sysctlr are both valid,
     * the system controller should take precedence.
     */
    if (nv_valid && sysctlr_valid && strcmp(nvram_serial, serial)) {
	printf("********************************************************\n");
	printf("* WARNING:  NVRAM and system controller serial numbers *\n");
	printf("*    do not match.  Rewriting NVRAM serial number.     *\n");
	printf("********************************************************\n");
	nv_valid = 0;
    }
  
    /* If both are okay, just return */
    if (nv_valid && sysctlr_valid)
	return 0;

    /* If the system controller serial number is broken, repair it */
    if (nv_valid && !sysctlr_valid) {
	if (!test_system) {
	    DPRINTF(("In read_serial, reprogramming system controller...\n"));
	    sysctlr_setserial(nvram_serial);
	} else
            printf("WARNING: In system test mode, not reprogramming serial "                       "number\n");   
	strcpy(serial, nvram_serial);
	return 0;
    }

    /* If the nvram version is broken, repair it */
    if (!nv_valid && sysctlr_valid) {
	if (!test_system) {
	    DPRINTF(("In read_serial, reprogramming nvram\n"));
	    nvram_setserial(serial);
	} else 
	    printf("WARNING: In system test mode, not reprogramming serial "
		   "number\n");
	return 0;
    }

    /* If we get here, both serial numbers are hosed. */
    DPRINTF(("In read_serial, both nvram and sysctlr were corrupt.\n"));
    return -1;
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
    sysctlr_setserial(serial);
    return 0;
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

    cpu_get_nvram_buf(NVOFF_SERIALNUM, len, serial);
    if (!valid_serial(serial))
	return -1;
    else
	return 0;
} 


/*
 * nvram_setserial(char *serial)
 *	Write the given serial number string into nvram.
 */

#define MIN(_x, _y) (((_x) < (_x)) ? (_x) : (_y))
void
nvram_setserial(char *serial)
{
    unchar len = (unchar)MIN(SERIALNUMSIZE, strlen(serial));

    cpu_set_nvram_offset(NVOFF_SERIALNUM, len, serial);
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


/*
 * cpu_set_nvram_offset
 *	Write the given string to nvram at an offset.  If operation
 *	fails, return -1, otherwise return 0.
 */

int
cpu_set_nvram_offset(int nv_off, int nv_len, char *string)
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
 * cpu_set_nvram
 *	Set the NVRAM RAM string whose name is passed as a parameter
 *	to the given value.  This just calls back to the setenv_nvram
 *	routine.
 */

int
cpu_set_nvram(char *match, char *newstring)
{
    extern int _prom;

    if (_prom)
	return setenv_nvram(match, newstring);
    else
	return 0;
}


/*
 * cpu_get_nvram_buf()
 *	Read a string of given length from nvram starting
 *	at a specified offset.  The characters read are written
 *	into the given buffer.  The buffer is null-terminated.
 */

void
cpu_get_nvram_buf(int nv_off, int nv_len, char buf[])
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
 * cpu_get_nvram_offset()
 *	Read a string of given length from nvram starting at
 *	a specified offset.  The routine returns a pointer to
 *	a static buffer containing the string.
 */
char *
cpu_get_nvram_offset(int nv_off, int nv_len)
{
    static char buf[128];

    cpu_get_nvram_buf(nv_off, nv_len, buf);
    return buf;
}


/*
 * cpu_is_nvvalid()
 *	Returns 1 if it looks like the NVRAM is correct, 0 otherwise.
 */

int
cpu_is_nvvalid(void)
{
    /* try twice */
    if (nvchecksum() != get_nvreg(NVOFF_NEW_CHECKSUM) &&
	nvchecksum() != get_nvreg(NVOFF_NEW_CHECKSUM))
	return 0;
 

    if (NV_CURRENT_REV != get_nvreg(NVOFF_REVISION))
	return 0;

    return 1;
}


/*
 * cpu_set_nvvalid()
 *	Sets the valid bit in the nvram.
 */

/*ARGSUSED*/
void
cpu_set_nvvalid(void (*delstr)(char *str, struct string_list *),
	        struct string_list *env)
{
    char csum = NV_CURRENT_REV;
    cpu_set_nvram_offset(NVOFF_REVISION, NVLEN_REVISION, &csum);

    update_checksum();
}


/*
 * cpu_is_oldnvram()
 *	Checks to see if this looks like an old but valid nvram.
 */

int
cpu_is_oldnvram(void)
{
    unchar version = get_nvreg(NVOFF_REVISION);
    char cksum;

    /* Make sure the checksums look reasonable */
    if (version <= NV_BROKEN_REV) 
	cksum = oldnvchecksum(); 
    else
	cksum = nvchecksum();
    
    if (cksum != get_nvreg(NVOFF_OLD_CHECKSUM))
	return 0;

    /* Now check the versions */
    if (version < NV_CURRENT_REV)
	return 1;

    return 0;
}


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
    char serial[SERIALNUMSIZE];
    int i;
    extern int read_serial(char *);
    extern void initialize_envstrs(void);
   
    for (i = NVOFF_LOWFREE; i < NVOFF_HIGHFREE; i++)
	set_nvreg(i, 0);
 
   cpu_set_nvram_offset(NVOFF_REBOUND, NVLEN_REBOUND, REBOUND_DEFAULT);

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
}



/* 
 * The following is crap required to keep the other proms working. 
 */

/*ARGSUSED*/
void 
cpu_nv_unlock(void) { }

/*ARGSUSED*/
void 
cpu_nv_lock(int lock) { }

/*ARGSUSED*/
void
cpu_get_nvram_persist(int (*putstr)(char *,char *,struct string_list *),
		      struct string_list *env) { }

/*ARGSUSED*/
void
cpu_set_nvram_persist(char *a,char *b) { }

void
cpu_nvram_init(void) { }
