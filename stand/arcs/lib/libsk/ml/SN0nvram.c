/**********************************************************************\
*	File:		SN0nvram.c					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/SN/nvram.h>
#include <libsc.h>
#include <libsk.h>
#include <libkl.h>
#include <kllibc.h>
#include <stringlist.h>
#include <ksys/elsc.h>

int	slvdebug ;

extern int sk_sable;
extern int _prom;

#if defined(DEBUG)
#define DPRINTF(X) printf X 
#else
#define DPRINTF(X)
#endif

/*
 * These set of global variables are needed to access and
 * keep all the nvrams in sync. It is initted in the IO6prom
 * sysinit.c - init_prom_soft. They are re-initted on re-entry
 * into the prom with init. Symmon and other utils just use
 * nvram routines which find the right value in these globals.
 */

__psunsigned_t	nvram_base ; 		/* master nvram's base */
__uint64_t	mstnv_state ;		/* master nvram's state */
__psunsigned_t	slvnv_base[MAX_NVRAM_IN_SYNC] ;
int		nslv_nvram ;		/* num of valid slave nv ptrs */

#define SET_NVREG	0x1
#define UPDATE_CHECKSUM 0x2
#define UPDATE_NVRAM	0x3
#define DUMP_SLVNVRAM	0x4
#define CPU_IS_NVVALID	0x5

extern struct string_list environ_str;

char nvchecksum(void) ;
int  check_slvnvram(int, uint, char) ;
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

     check_slvnvram(SET_NVREG, offset, byte) ;

    return 0;
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

    if (sk_sable)
	return (char)0;

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
#if defined (HUB_ERR_STS_WAR)
    if (do_hub_errsts_war() == 0)
	printf("Force read error for HUB_ERR_STS_WAR failed\n");
#endif

    return (char)checksum;
}

void
update_checksum(void)
{
	set_nvreg(NVOFF_CHECKSUM, nvchecksum());
	check_slvnvram(UPDATE_CHECKSUM, 0, 0) ;
#if defined (HUB_ERR_STS_WAR)
    if (do_hub_errsts_war() == 0)
	printf("Force read error for HUB_ERR_STS_WAR failed\n");
#endif
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
	{
	printf("max limit reached\n") ;
	return -1;
	}


    for (i = 0; i < nv_len; i++) {
	if (set_nvreg(i+nv_off, string[i]))
	{
	printf("nv set failed \n") ;
	    return -1;
	}
    }

    update_checksum();
#if defined (HUB_ERR_STS_WAR)
    if (do_hub_errsts_war() == 0)
	printf("Force read error for HUB_ERR_STS_WAR failed\n");
#endif

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
#if defined (HUB_ERR_STS_WAR)
    if (do_hub_errsts_war() == 0)
	printf("Force read error for HUB_ERR_STS_WAR failed\n");
#endif
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
    int rc = 1;

    /* try twice */
    if (nvchecksum() != get_nvreg(NVOFF_CHECKSUM) &&
	nvchecksum() != get_nvreg(NVOFF_CHECKSUM))
    {
	rc = 0;
    }

    if (NV_CURRENT_REV != get_nvreg(NVOFF_REVISION))
    {
	rc = 0;
    }
#if defined (HUB_ERR_STS_WAR)
    if (do_hub_errsts_war() == 0)
	printf("Force read error for HUB_ERR_STS_WAR failed\n");
#endif

    check_slvnvram(CPU_IS_NVVALID, rc, nvchecksum()) ;

    return rc;
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
 * cpu_fix_dbgtty()
 * ssg proms have the default dbgtty env var set to /dev/tty/ioc3tty0
 * The ficus proms have changed this name internally as /dev/tty/ioc30
 * Since default env values are not upgraded for every new prom, 
 * we force it here.
 */

#define DBGTTY_PATH_OLD_DEFAULT	"/dev/tty/ioc3tty0"

void
cpu_fix_dbgtty(void)
{
    char *cp ;

    if (cp = getenv("dbgtty")) {
	if (!strcmp(cp, DBGTTY_PATH_OLD_DEFAULT))
	    setenv("dbgtty", DBGTTY_PATH_DEFAULT) ;
    }
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
#if 1
    printf("fixme: cpu_update_nvram\n"); /* XXX */
#else
    char serial[SERIALNUMSIZE];
    int i;
    extern int read_serial(char *);
    extern void initialize_envstrs(void);

   /* XXX - The "free" area may be in use by PNT. Check inventory.h
    for (i = NVOFF_LOWFREE; i < NVOFF_HIGHFREE; i++)
	set_nvreg(i, 0);
   */

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
#if defined (HUB_ERR_STS_WAR)
    if (do_hub_errsts_war() == 0)
	printf("Force read error for HUB_ERR_STS_WAR failed\n");
#endif

#endif
}

/* Master slave nvram sync support */

/*
 * All key routines for the master nvram are written
 * for the slave nvrams with a 'slv_' prefix.
 */

unchar
slv_get_nvreg(__psunsigned_t base, uint offset)
{
    if (base)
        return(*(unchar *)(base+offset)) ;

    return (unchar)0 ;
}

int
slv_set_nvreg(__psunsigned_t base, uint offset, unchar byte)
{
    if ((offset > NVOFF_LOWFREE) && (offset < NVOFF_HIGHFREE)) {
	return 0 ;
    }

    if (base)
        *((unchar *)(base+offset)) = byte ;

    return 0;
}

char
slv_nvchecksum(__psunsigned_t base)
{
    register ulong nv_reg;
    unchar nv_val;
    signed char checksum = 0xa5;

    /*
     * do the checksum on all of the nvram, skip the checksum byte !!
     */
    for (nv_reg = NVOFF_REVISION; nv_reg < NVOFF_HIGHFREE; nv_reg++) {
        nv_val = slv_get_nvreg(base, nv_reg);
        if (nv_reg != NVOFF_CHECKSUM &&
            nv_reg != NVOFF_RESTART)
            checksum ^= nv_val;

        /* following is a tricky way to rotate */
        checksum = (checksum << 1) | (checksum < 0);
    }

    return (char)checksum;
}

/*
 * copy master nvram to the nvram pointed to by b
 * except checksum byte. re-write checksum.
 */

/*
 * In 6.0+ proms, keep only that portion of the nvram
 * in sync that was present in downrev proms
 */

#define NVOFF_LOWFREE_FOR_SYNC	(NVOFF_SCSI_TO_WAR + NVLEN_SCSI_TO_WAR)

char
slv_nvchecksum_sync(__psunsigned_t base)
{
    register ulong nv_reg;
    unchar nv_val;
    signed char checksum = 0xa5;

    /*
     * do the checksum on all of the nvram, skip the checksum byte !!
     */
    for (nv_reg = NVOFF_REVISION; nv_reg < NVOFF_LOWFREE_FOR_SYNC; nv_reg++) {
        nv_val = slv_get_nvreg(base, nv_reg);
        if (nv_reg != NVOFF_CHECKSUM &&
            nv_reg != NVOFF_RESTART)
            checksum ^= nv_val;

        /* following is a tricky way to rotate */
        checksum = (checksum << 1) | (checksum < 0);
    }

    return (char)checksum;
}

int
slv_copy_nvram(__psunsigned_t b)
{
	int i ;

	for (i=NVOFF_REVISION;i<NVOFF_LAST;i++) {
        	if ((i == NVOFF_CHECKSUM) || (i == NVOFF_RESTART) ||
                    ((i >= NVOFF_LOWFREE_FOR_SYNC) /* && (i < NVOFF_HIGHFREE)*/))
				continue ;
		*((volatile unchar *)(b+i)) = *(unchar *)(nvram_base+i) ;
	}
	return 0 ;
}

/*
 * Update all nvrams to be in sync with the master
 */

int
slv_update_slvnvram(__psunsigned_t b)
{
    slv_copy_nvram(b) ;
    slv_set_nvreg(b, NVOFF_CHECKSUM, slv_nvchecksum(b));
    return 0 ;
}

int
slvnvram(void)
{
	check_slvnvram(DUMP_SLVNVRAM, 0, 0) ;
	return 0 ;
}

/*
 * Performs 'op' on all the slave or non-master nvrams.
 * There is no check for errors as these operations on
 * nvrams are like memory write. TO not keep an nvram
 * in sync, set the addrs of it to 0 in slvnv_base.
 * Global variables used : nslv_nvram, slvnv_base and mstnv_state.
 */

check_slvnvram(int op, uint off, char val)
{
    int i, j ;
    char c ;
    __psunsigned_t	base ;

    if (!SLVNV_PRESENT)
    	return 0 ;

    for (i=0; i<nslv_nvram;i++) {
	base = slvnv_base[i] ;
	if (base) {
	    switch (op) {
		case SET_NVREG:
			slv_set_nvreg(base, off, (unchar)val) ;
		break ;

#if 0
		case UPDATE_NVRAM:
			slv_update_slvnvram(base) ;
		break ;
#endif

		case UPDATE_CHECKSUM:
			slv_set_nvreg(base, NVOFF_CHECKSUM, 
				slv_nvchecksum(base));
		break ;

		case CPU_IS_NVVALID:
#ifdef DEBUG
			printf("base = 0x%lx: Mstcsum = %x, slvcsum = %x\n", 
				base, val, slv_nvchecksum(base)) ;
#endif
			if (slv_nvchecksum(base) != val) {
#ifdef DEBUG
			    printf("Fixing Slave nvram 0x%lx, bad checksum.\n", 
					base) ;
#endif
			    slv_update_slvnvram(base) ;
			    if (slv_nvchecksum_sync(base) == 
				slv_nvchecksum_sync(nvram_base))
			    	mstnv_state |= SLVNV_OK ;
			    else {
				printf("**Warning: Slave nvram chksum are different."
 				        "Continuing ...\n", 
					slv_nvchecksum_sync(nvram_base), 
					slv_nvchecksum_sync(base)) ;
				slvnv_base[i] = 0 ;
			    }
		 	}
		break ;

#ifdef DEBUG
#endif
		case DUMP_SLVNVRAM:
			printf("Base = 0x%lx\n", base) ;
			for (j=0; j<NVLEN_CONSOLE_PATH;j++)
			    if (c = slv_get_nvreg(base, (NVOFF_CONSOLE_PATH+j)))
			      if (isprint(c))
				putchar(c) ;
			      else break ;
			printf("\n") ;
		break ;
	    }
	}
    }
    return 1 ;
}

/* ---------------------- end sync nvram support --------- */

/*
 * The ProbeAllScsi variable has moved in kudzu.
 * The old variable now holds the 2nd byte of the
 * 2 byte scsi host id. This routine changes the
 * old value of ProbeAllScsi from 'y' or 'n'
 * to 0.
 * Also, if the old ProbeAllScsi location holds a
 * 'y' or 'n' it means that it is a old version
 * of the NVRAM, but a new version of the PROM.
 * Transfer that variable to the new location.
 * If the new location already has a y or n
 * do not transfer.
 *
 * The default value of NVOFF_SWAP_ALL_BANKS is set to 'y' because all new
 * versions of the IO prom (i.e. all those which have this variable) do
 * know how to swap all pairs of banks.
 */
#define NVOFF_OLDPROBEALLSCSI (NVOFF_SCSIHOSTID + 1)

struct init_new_env_s {
	int 	off ;
	char 	*val ;
	char 	*name ;
} new_env[] = {
	NVOFF_PROBEALLSCSI, 	"n", 	"ProbeAllScsi",
	NVOFF_PROBEWHICHSCSI, 	"", 	"ProbeWhichScsi",
	NVOFF_DBGNAME, 		"",	"dbgname",
	NVOFF_SWAP, 		"",	"swap",
	NVOFF_RESTORE_ENV, 	"y",  	"RestorePartEnv",
	NVOFF_EA_CNT, 		"",	NULL,
	NVOFF_SWAP_ALL_BANKS,	"y",	NULL,
	0, 			NULL, 	NULL
} ;

void
init_new_env_vars(void)
{
	struct init_new_env_s 	*ne = new_env ;
	char	c ;
	int	changed = 0 ;

	while (ne->off) {
        	c = get_nvreg(ne->off) ;
        	if (!isascii(c)) {
			changed = 1 ;
			if (ne->val)
            			set_nvreg(ne->off, *ne->val) ;
			else
				set_nvreg(ne->off, 0) ;

			if ((ne->name) && (ne->val))
    				replace_str(ne->name, ne->val, &environ_str) ;
        	}
		ne++ ;
	}

	if (changed)
        	update_checksum() ;
}

void
change_probeallscsi()
{
        char c, c1 ;

	/* Transfer the old value of ProbeAllScsi as its location moved. */

        if (((c = get_nvreg(NVOFF_OLDPROBEALLSCSI)) == 'y') ||
            (c == 'n')) {
            set_nvreg(NVOFF_OLDPROBEALLSCSI, 0) ;
	    if (((c1 = get_nvreg(NVOFF_PROBEALLSCSI)) == 'y') ||
		  (c1 == 'n')) {
		/* do nothing */
	    } else
		set_nvreg(NVOFF_PROBEALLSCSI, c) ; /* Transfer the old val */
            update_checksum() ;
        }

	/* 
	 * It looks like all the free area in the NVRAM is NOT
	 * initted to zeros. We need to try and explicitly
	 * init all new space to known values.
 	 * One check is to see if these locations have 0 or
	 * some known ascii value.
	 */

	init_new_env_vars() ;
}

void
cpu_set_sav_cons_path(char *s)
{
    cpu_set_nvram_offset(NVOFF_SAV_CONSOLE_PATH,
			 NVLEN_SAV_CONSOLE_PATH, s) ;
    replace_str("oldConsolePath", s, &environ_str) ;
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

char
get_scsi_war_value(void)
{
	char times;
	
	times = get_nvreg(NVOFF_SCSI_TO_WAR);
	
	return times;
}


void
set_scsi_war_value(char times)
{

	set_nvreg(NVOFF_SCSI_TO_WAR, times);
	update_checksum();
	
}

char
get_scsi_to_ll_value(void)
{
        char ll;

        ll = get_nvreg(NVOFF_SCSI_TO_LL);

        return ll;
}


void
set_scsi_to_ll_value(char ll)
{

        set_nvreg(NVOFF_SCSI_TO_LL, ll);
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

/*
 * Support for remembering old partition's info
 */
partid_t
get_my_partid(void)
{
	return(elsc_partition_get(get_elsc())) ;
}

#define	IS_PARTID_VALID(p)	((p > 0) && (p < MAX_PARTITIONS))

struct swap_nv {
	int	store_off ;
	int	curr_off ;
	int	len ;
	char	*name ;
} ;

struct swap_nv swap_off[] = {
	NVOFF_PREV_DSKPART, NVOFF_SYSPART, NVLEN_SYSPART, "SystemPartition",
	NVOFF_PREV_OSPART, NVOFF_OSPART, NVLEN_OSPART, "OSLoadPartition",
	NVOFF_PREV_ROOT, NVOFF_ROOT, NVLEN_ROOT, "root",
	NVOFF_PREV_NETADDR, NVOFF_NETADDR, NVLEN_NETADDR, "netaddr",
	-1, -1, NULL
} ;

#define MAX_NVVAR_LEN 	128
#define STORE		1
#define RESTORE		2

void
do_part_info(int flag, partid_t curr)
{
	struct swap_nv	*tmp ;
	char		var[MAX_NVVAR_LEN] ;
	int		off ;
	char		na[32] ;

        /* Make stored partid invalid */

        set_nvreg(NVOFF_STORED_PART_ID, 0) ;

        /* store it */
        tmp = swap_off ;
        while (tmp->store_off > 0) {
		if (flag == RESTORE)
			off = tmp->store_off ;
		else if (flag == STORE)
			off = tmp->curr_off  ;
		else
			return ;

                cpu_get_nvram_buf(off, tmp->len, var) ;

		if (flag == RESTORE) {
			if (tmp->curr_off == NVOFF_NETADDR) {
				sprintf(na, "%d.%d.%d.%d\0", 
					var[0], var[1],
					var[2], var[3]) ;
				strcpy(var, na) ;
			}
			setenv(tmp->name, var) ;
		} 
		else if (flag == STORE)
                	cpu_set_nvram_offset(tmp->store_off, tmp->len,
                                        var) ;
                tmp++ ;
        }

        /* Make stored partid valid */

        set_nvreg(NVOFF_STORED_PART_ID, curr) ;
	/* update_checksum() ; */
}

void
check_prev_part_info()
{
	partid_t	prev, curr, stored ;
	partid_t	spid;
	char		magic;
	char		buf[NVLEN_STORE_CONSOLE_PATH];

	/* First, Get 3 part ids */

	prev 	= get_nvreg(NVOFF_PREV_PART_ID) ;
	curr 	= get_my_partid() ;
	stored 	= get_nvreg(NVOFF_STORED_PART_ID) ;
	
	/* Also get the last partid when magic and console_path was written */
	magic 	= get_nvreg(NVOFF_PART_MAGIC);
	if(magic == PART_MAGIC)
		spid	= get_nvreg(NVOFF_STORE_PARTID); 

	/* we got clobbered but there is hope */

	if ((prev != curr)  && (stored == curr)) {
		printf("Restoring environment for partition %d\n", curr) ;
		do_part_info(RESTORE, curr) ;
	}
	else {
		do_part_info(STORE, curr) ;
	}

	/* Lastly, update the prev id to curr id */

	set_nvreg(NVOFF_PREV_PART_ID, curr) ;
        /* Also we may overwrite the console path */
	
	if(magic != PART_MAGIC || curr == spid)
	{
		set_nvreg(NVOFF_STORE_PARTID, curr);
		set_nvreg(NVOFF_PART_MAGIC, PART_MAGIC);
		cpu_get_nvram_buf(NVOFF_CONSOLE_PATH,NVLEN_CONSOLE_PATH,buf);
		cpu_set_nvram_offset(NVOFF_STORE_CONSOLE_PATH,NVLEN_STORE_CONSOLE_PATH,buf);

	}
    	update_checksum();

}

