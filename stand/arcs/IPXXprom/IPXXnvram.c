/* IPXX nvram code
 */

#ident "$Revision: 1.3 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/IP20nvram.h>
#include <libsc.h>
#include <libsk.h>
#include <arcs/folder.h>

/* Exports */
char *		cpu_get_nvram_offset(int, int);
void		cpu_get_nvram_buf(int,int,char buf[]);
int		cpu_set_nvram_offset(int, int, char *);

/* Private */
static ushort	get_nvreg(int);
static int	set_nvreg(int, unsigned short);

static ushort nvram[128];

/*
 * nvchecksum -- calculate new checksum for non-volatile RAM
 */
static char
nvchecksum(void)
{
    register int nv_reg;
    unsigned short nv_val;
    /*
     * Seed the checksum so all-zeroes (all-ones) nvram doesn't have a zero
     * (all-ones) checksum.
     */
    register signed char checksum = 0xa5;

    /*
     * do the checksum on all of the nvram, skip the checksum byte !!
     */
    for (nv_reg = 0; nv_reg < NVLEN_MAX / 2; nv_reg++)
    {
	nv_val = get_nvreg(nv_reg);
	if (nv_reg == (NVOFF_CHECKSUM / 2))
	   if (NVOFF_CHECKSUM & 0x01)
	      checksum ^= nv_val >> 8;
	   else
	      checksum ^= nv_val & 0xff;
	else
	   checksum ^= (nv_val >> 8) ^ (nv_val & 0xff);
	/* following is a tricky way to rotate */
	checksum = (checksum << 1) | (checksum < 0);
    }

    return (char)checksum;
}


/*
   find out number of address bits in the nvram. since this routine will be 
   called from unix, any reference to prom BSS MUST be avoided 
*/

static int
size_nvram ()
{
	return(256);
}

/*
 * get_nvreg -- read a 16 bit register from non-volatile memory.  Bytes
 *  are stored in this string in big-endian order in each 16 bit word.
 */
static ushort
get_nvreg(int nv_regnum)
{
	ushort data;

	return(nvram[nv_regnum]);
}


/*
 * set_nvreg -- writes a 16 bit word into non-volatile memory.  Bytes
 *  are stored in this register in big-endian order in each 16 bit word.
 */
static int
set_nvreg(int nv_regnum, unsigned short val)
{
	nvram[nv_regnum] = val;
	return(0);
}

/*
 * cpu_get_nvram -- read string from nvram at an offset
 */
char *
cpu_get_nvram_offset(int nv_off, int nv_len)
{
	static char buf[128];
	char *p = (char *)nvram;

	bcopy(p+nv_off,buf,nv_len);

	return buf;
}

/*
 * cpu_get_nvram_buf -- the same as cpu_get_nvram, but put it in a buffer
 *                      of our choice
 */
void
cpu_get_nvram_buf(int nv_off, int nv_len, char buf[])
{
	char *p = (char *)nvram;

	bcopy(p+nv_off,buf,nv_len);

	return;
}

/*
 * cpu_set_nvram_offset -- write a string to nvram at an offset
 */
int
cpu_set_nvram_offset(int nv_off, int nv_len, char *string)
{
	char *p = (char *)nvram;

	bcopy(string,p+nv_off,nv_len);
	return(0);
}

/*
 * cpu_set_nvram - set nvram string to value string
 *	XXX reset of nvram code needs to move out of prom area
 *	to someplace reasonable (here?)
 */
int
cpu_set_nvram(char *match, char *newstring)
{
    extern int _prom;

    if (_prom)
	return setenv_nvram (match, newstring);
    else
	return 0;
}

int
cpu_is_nvvalid(void)
{
    /* try twice */
    if (nvchecksum() != *cpu_get_nvram_offset(NVOFF_CHECKSUM,NVLEN_CHECKSUM) &&
	nvchecksum() != *cpu_get_nvram_offset(NVOFF_CHECKSUM,NVLEN_CHECKSUM) )
	return 0;
    

    if (NV_CURRENT_REV !=*cpu_get_nvram_offset(NVOFF_REVISION, NVLEN_REVISION))
	return 0;

    return 1;
}

/*ARGSUSED*/
void
cpu_set_nvvalid(void(*a)(char *,struct string_list *),struct string_list *b)
{
    char csum = NV_CURRENT_REV;
    cpu_set_nvram_offset(NVOFF_REVISION, NVLEN_REVISION, &csum);

    csum = nvchecksum();
    cpu_set_nvram_offset(NVOFF_CHECKSUM, NVLEN_CHECKSUM, &csum);
}

/* NVRAM users */

/* the PROM uses this routine for the PROM_EADDR entry point */
void
cpu_get_eaddr(char eaddr[])
{
	cpu_get_nvram_buf(NVOFF_ENET, NVLEN_ENET, eaddr);
}
