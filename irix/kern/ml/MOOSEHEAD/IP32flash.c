#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/IP32flash.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/kopt.h>
#include <sys/invent.h>
#include <sys/PCI/pciio.h>

#ifndef NULL
#define NULL	((void *)0)
#endif

static char *var_tbl[] = {
  "OSLoadOptions",
  "SystemPartition",
  "OSLoadPartition",
  "OSLoader",
  "OSLoadFilename",
  "AutoLoad",
  "rbaud",
  "TimeZone",
  "console",
  "diagmode",
  "diskless",
  "nogfxkbd",
  "keybd",
  "lang",
  "scsiretries",
  "scsihostid",
  "dbaud",
  "pagecolor",
  "volume",
  "sgilogo",
  "nogui",
  "netaddr",
  "passwd_key",
  "netpasswd_key",
  "videotiming", 
  "videooutput",
  "videoinput",
  "videostatus",
  "videogenlock",
  "audiopass",
  0,
};

/* the actual size of the table, including the null entry at the end
	is 29 members as of 8/93 */

#define NVRAMTAB 40
struct nvram_var_s {
    struct nvram_entry nvram_tab[NVRAMTAB];
    int nv_size;
    int nv_good;
    int nv_changed;
    int nv_cnt;
    int nv_structsum;
    int nv_cksum;
};
static struct nvram_var_s nvram_vars;

/*
 * These globals must be set up both in init_flash_rom() and syssgi_ip32(), or anytime
 * the prom could have changed, to insure that they always are in sync with the prom.
 * flash_sync_globals() sets them up appropriately.
 */
FlashSegment*flash_env_seg;	/* points to beginning of environment segment in flash */
static char *flash_env_base;	/* points to beginning of _body_ of env segment in flash */
static char *flash_env_lim;	/* points past end of environment segment in flash	*/ 
static int   flash_ver[2];	/* prom version */

#ifdef RESET_DUMPSYS
int   flash_dump_set = 0;
#endif /* RESET_DUMPSYS */

static int
string_sum(char *s)
{
    int i, sum;

    for (sum = 0, i = 0; i < strlen(s); i++)
	sum += s[i];

    return(sum);
}

/*
 * generate the checksum for the nvram table.  We use two checksums,
 * the first is a sum across only the contents of the nvram_var_s
 * structure (names and string pointer values).  If this is corrupted
 * (does not equal zero) we return a failure indication.  If this
 * succeeds, it is safe to chase down the pointers for the value
 * strings (they are all valid) so we construct the second
 * checksum by summing the bytes in each of the valid value 
 * strings.
 */
static void
gensum(struct nvram_var_s *nv)
{
    int xsum = 0;
    int *xbuf = (int *)nv;
    int *lim  = (int *)&nv->nv_structsum;
    int i;

    /* first sum the contents of the structure */
    for (xsum = 0; xbuf < lim; xbuf++)
	xsum += *xbuf;

    nv->nv_structsum = -xsum;

    for (xsum = 0, i = 0; i < nv->nv_cnt; i++)
	xsum += string_sum(nv->nvram_tab[i].nt_value);

    nv->nv_cksum = -xsum;
}

static int
checksum(struct nvram_var_s *nv)
{
    int xsum = 0;
    int *xbuf = (int *)nv;
    int *lim  = (int *)&nv->nv_cksum;
    int i;

    /* first sum the contents of the structure */
    for (xsum = 0; xbuf < lim; xbuf++)
	xsum += *xbuf;

    /*
     * the structure is corrupt...
     */
    if (xsum)
	return(xsum);

    for (i = 0; i < nv->nv_cnt; i++)
	xsum += string_sum(nv->nvram_tab[i].nt_value);

    return(xsum + nv->nv_cksum);
}

/*
 * simple parsing for environment variable strings.  It assumes that
 * strings are of the form name=value *and* that ev points to the 
 * beginning of a string of this form.
 *
 * If we don't encounter an '=' before a reasonable number of characters
 * have been processed, we decide that the string is malformed and punt.
 */
static int
flash_parsevar(const char *ev, char **value)
{
    char *tmp = (char *)ev;

    while (*tmp != '=' && *tmp != '\0') {
	if (tmp - ev > MAXNVNAMELEN + 1) {
	    *value = NULL;
	    return(0);
	}
	tmp++;
    }
    
    /*
     * did we find a properly formed string?
     */
    if (*tmp) {
	*value = ++tmp;
	return(1);
    } else {
	*value = NULL;
	return(0);
    }
}

/*
 * returns length of string + 1 for the null 
 */
static int
flash_cpyname(char *dest, char *name)
{
    int cnt = 0;

    while (*dest++ = *name++) {
	cnt++;
	if (*name == '=') {
	    *dest = '\0';
	    return cnt;
	}
    }
    return cnt;
}

static int
valid_seg_hdr(FlashSegment *seg)
{
    long *xslim = body(seg);
    long *xsp;
    long xsum;

    if ((seg->magic != FLASH_SEGMENT_MAGIC) ||
	((int)seg & FLASH_PAGE_SIZE - 1) ||
	(seg->segLen & 0x3))
	return(0);

    for (xsum = 0, xsp = (long *)seg; xsp != xslim; xsp++) 
	xsum += *xsp;

    return(xsum == 0);
}

/*
 * next_seg returns a pointer to the next valid segment header
 * in the flash.  It returns NULL if no valid header is found 
 * before reaching the end of the flash address space.
 *
 * NB: if *flash points to a valid header next_seg will return
 * the value of flash.
 */
static FlashSegment *
next_seg(char *flash)
{
    long *magicptr = (long *)((int)flash & ~0x3); /* ensure proper alignment */
    FlashSegment *seg;

next:
    while (*magicptr != FLASH_SEGMENT_MAGICx) {
	if (++magicptr > (long *)(PHYS_TO_K1(FLASH_ROM_BASE) + FLASH_SIZE))
	    return(NULL);
    }

    seg = (FlashSegment *)((int)magicptr - sizeof(long long));
    if (valid_seg_hdr(seg)) {
	if (seg->segLen != 0)
	    return(seg);
	else
	    return(NULL);

    } else
	magicptr++;

    goto next;
}

/*
 * find_named_flash_seg() returns a pointer to the first data byte
 * in the named flash ROM segment.
 */
static FlashSegment *
find_named_flash_seg(char *name)
{
    FlashSegment *seg = (FlashSegment *)PHYS_TO_K1(FLASH_ROM_BASE);

    while (seg = next_seg((char *)seg)) {
	if (!strncmp(seg->name, name, seg->nameLen))
	    return(seg);
	seg = (FlashSegment *)((int)seg + seg->segLen);
    }
    return(NULL);
}

/*
 * copy environment variables from the flash to local nvram variable
 * table.
 */
static void
flash_nvram_tab(void)
{
    char *ev, *value;
    int i;
    struct nvram_entry *nvars = nvram_vars.nvram_tab;

    /*
     * flash does *not* have a valid env segment...
     * something is very wrong.
     */
    if (!flash_env_base)
	return;

    nvram_vars.nv_good = 1;
    for (ev = flash_env_base, i = 0; *ev; ev += strlen(ev) + 1, i++) {

	/*
	 * if we are past the end of the environment segment
	 * something is quite wrong.
	 */
	if (ev > flash_env_lim) {
	    nvram_vars.nv_good = 0;
	    return;
	}

	/*
	 * if we find a malformed variable, we have to assume
	 * that the flash is corrupted.
	 */
	if (!flash_parsevar(ev, &value)) {
	    nvram_vars.nv_good = 0;
	    continue;
	}

	/* 
	 * keep a running count of the number of characters which
	 * will have to be copied back to the flash should the 
	 * environment be changed.
	 */
	nvram_vars.nv_size += flash_cpyname(nvars[i].nt_name, ev);
	nvram_vars.nv_size += strlen(value) + 1; /* +1 for the null */

	nvars[i].nt_value = value;
	nvram_vars.nv_cnt++;
    }
    if (nvram_vars.nv_good)
	gensum(&nvram_vars);
}

#ifdef RESET_DUMPSYS
/*
 * XXX
 * this hack never worked and for whatever reason
 * was never taken out. Lets get back some text/data space
 * by putting these under the RESET_DUMPSYS ...
 */
char flash_1st_page_buf[FLASH_PAGE_SIZE];
char flash_dump_jump_buf[FLASH_PAGE_SIZE];

/*
 * init_flash_dump_vector copies a trampoline function which ends
 * up calling dumpsys into the first few bytes of the PROM.  The
 * original PROM code is saved and is restored upon normal system
 * shutdown or by reset_dump() if a reset exception or an NMI 
 * causes the system to jump to 0xbfc00000.  Ugly, huh?
 */
void
init_flash_dump_vector(void)
{
    extern int is_r4600_flag;
    extern char flash_jump_vector_end[];
    int i, len;

    /*
     * user hasn't requested this monumental service...
     */
    if (!(is_specified(arg_reset_dump) && is_enabled(arg_reset_dump)))
	return;

    /*
     * we can't do this trick on an R10K because it loses
     * it's register file on soft reset and hard reset, and
     * the consequences of an NMI aren't known.
     */
    if (!is_r4600_flag)
	return;
    

    /*
     * copy the first page of the flash for later restoration
     * bcopy((char *)0xbfc00000, flash_1st_page_buf, FLASH_PAGE_SIZE);
     */
    for (i=0; i < FLASH_PAGE_SIZE; i++)
	flash_1st_page_buf[i] = pciio_pio_read8((char *)(0xbfc00000+i));

    flash_dump_set = 1;

    /*
     * now get our special little buffer ready...
     */
    len = (int)((caddr_t)flash_jump_vector_end - (caddr_t)flash_jump_vector);

    bzero(flash_dump_jump_buf, FLASH_PAGE_SIZE);
    bcopy((char *)flash_jump_vector, flash_dump_jump_buf, len);

    (void)flash_write_sector((char *)0xbfc00000, flash_dump_jump_buf);
}
#endif /* RESET_DUMPSYS */

static flash_initted = 0;
void
init_flash_rom(void)
{
    flash_sync_globals();	/* sync global variables to the prom */
    bzero(&nvram_vars, sizeof(nvram_vars));
    flash_initted = 1;
}

#define isdigit(x) (((x) >= '0') && ((x) <= '9'))
#define toint(x) ((int)(x) - (int)('0'))

static ver_initted = 0;
static void
get_flash_version(void)
{
    FlashSegment *seg = find_named_flash_seg("version");
    char *verno;

    if (ver_initted)
	return;

    ver_initted = 1;

    flash_ver[FLASHPROM_MAJOR] = flash_ver[FLASHPROM_MINOR] = 0;

    if (seg == NULL)
	seg = find_named_flash_seg("firmware");

    if (seg == NULL)
	return;

    if (seg->vsnLen == 0)
	return;

    verno = seg->version;
      
    /*
     * flash version numbers are of the form x.y where x and y
     * are both decimal integers.  Check to ensure that first
     * character of the version string is a decimal digit.
     */
    if (!isdigit(*verno))
	return;

    /*
     * while reading decimal digits calculate the integer version number
     */
    while (isdigit(*verno)) {
	flash_ver[FLASHPROM_MAJOR] = (flash_ver[FLASHPROM_MAJOR] * 10) + toint(*verno);
	verno++;
    }

    /*
     * we should have stopped at a decimal point
     * if not, we don't have a valid version string, reset major to 0
     * and return
     */
    if (*verno++ != '.') {
	flash_ver[FLASHPROM_MAJOR] = 0;
	return;
    }

    /*
     * a digit should follow the decimal point, if not, we don't
     * have a valid version string.
     */
    if (!isdigit(*verno)) {
	flash_ver[FLASHPROM_MAJOR] = 0;
	return;
    }
	
    /*
     * get the minor version number.
     */
    while (isdigit(*verno)) {
	flash_ver[FLASHPROM_MINOR] = (flash_ver[FLASHPROM_MINOR] * 10) + toint(*verno);
	verno++;
    }
}

void
update_flash_version(void)
{
  ver_initted = 0;
  get_flash_version();
}

void
load_nvram_tab(void)
{
    init_flash_rom();
#if 0
    get_flash_version();
#endif
    flash_nvram_tab();
}


int
flash_version(int part)
{
    return(part ? flash_ver[FLASHPROM_MINOR] : flash_ver[FLASHPROM_MAJOR]);
}

static void
flash_write_enable(void)
{
    unsigned char wrenreg;
    
    wrenreg = READ_REG64(FLASH_WENABLE, _crmreg_t);
    wrenreg |= FLASH_WP_OFF;
    WRITE_REG64(wrenreg, FLASH_WENABLE, _crmreg_t);
}

static void
flash_write_disable(void)
{
    unsigned char wrenreg;
    
    wrenreg = READ_REG64(FLASH_WENABLE, _crmreg_t);
    wrenreg &= FLASH_WP_ON;
    WRITE_REG64(wrenreg, FLASH_WENABLE, _crmreg_t);
}

/*
 * the write is complete when bit 7 of the data in the last byte
 * a flash sector is equal to bit 7 of the original data.
 */
#ifdef P0_WAR
#define WRITE_NOT_COMPLETE(d, s) \
	((pciio_pio_read8((volatile uint8_t *)(d) + (FLASH_PAGE_SIZE - 1)) & 0xff) != \
         (*((s) + (FLASH_PAGE_SIZE - 1)) & 0xff))
#else
#define WRITE_NOT_COMPLETE(d, s) \
	((pciio_pio_read8((volatile uint8_t *)(d) + (FLASH_PAGE_SIZE - 1)) & 0x80) != \
         (*((s) + (FLASH_PAGE_SIZE - 1)) & 0x80))
#endif
/*
 * writes a FLASH_PAGE_SIZE byte sector.  Must be FLASH_PAGE_SIZE 
 * byte aligned address in the flash address space.  buf must point 
 * to FLASH_PAGE_SIZE bytes of data.
 *
 * returns 0 if successful and 1 if it fails.  
 */
int
flash_write_sector(volatile char *addr, char *buf)
{
    int s, i;

    /* check address alignment */
    if ((__psint_t)addr & (FLASH_PAGE_SIZE - 1)) {
	cmn_err(CE_WARN, "flash address alignment error 0x%x\n",
		addr);
	return(1);
    }

    s = splecc();
    flash_write_enable();
    for (i = 0; i < FLASH_PAGE_SIZE; i++)
	/* *(addr + i) = *(buf + i); */
	pciio_pio_write8(*(buf + i), (volatile uint8_t *)(addr + i));
    flash_write_disable();
    splxecc(s);

    /* 
     * they don't tell you, but it takes around 7ms(!) to complete
     * a write to the flash ram.
     */
    us_delay(7000);
	
    /* check to make sure the write took place */
    i = 1000;
    while (WRITE_NOT_COMPLETE(addr, buf)) {
	us_delay(1000);
	if (!i) { 
	    cmn_err(CE_WARN, "flash write failure, not completing at 0x%x\n",
	    	    addr);
	    return(1);
	}
	i--;
    }

    /* 
     * at this point, we should be able to read 
     * the same data we wrote 
     */
    ASSERT(pciio_pio_read8((volatile uint8_t *)addr) == *buf);
    return(0);
}

/*
 * reads a FLASH_PAGE_SIZE byte sector.  Must be FLASH_PAGE_SIZE 
 * byte aligned address in the flash address space.  buf must point 
 * to FLASH_PAGE_SIZE bytes of data.
 *
 * returns 0 if successful and 1 if it fails.  
 */
flash_read_sector(char *addr, char *buf)
{
    int i;

    if ((__psint_t)addr & (FLASH_PAGE_SIZE - 1))
	return(1);

    for (i = 0; i < FLASH_PAGE_SIZE; i++)
	/* *(buf + i) = *(addr + i); */
	*(buf + i) = pciio_pio_read8((volatile uint8_t *)(addr + i));

    return(0);
}

static int
set_nv_value(char **value, char *newval, int new)
{
    char *t = NULL;
    int  oldsize = 0;
#ifdef FLASH_DEBUG
    void flash_write_env(void);
#endif

    if (*value && !new)
	oldsize = strlen(*value) + 1;

    if ((*value > flash_env_base && *value < flash_env_lim) || new) {
	if ((t = (char *)kmem_alloc(strlen(newval) + 1, KM_SLEEP)) == NULL) {
	    cmn_err(CE_WARN, "Unable to allocate %d bytes for nvram var\n", 
		    strlen(newval) + 1);
	    return(-1);
	}
    } else {
	if ((t = (char *)kmem_alloc(strlen(newval) + 1, KM_SLEEP)) == NULL) {
	    cmn_err(CE_WARN, "Unable to allocate %d bytes for nvram var\n", 
		    strlen(newval) + 1);
	    return(-1);
	}
	if (*value) kmem_free(*value, strlen(*value) + 1);
    }

    if (oldsize)
	nvram_vars.nv_size -= oldsize;

    if (t)
	*value = t;
    strcpy(*value, newval);
    nvram_vars.nv_size += strlen(*value) + 1;
    nvram_vars.nv_changed = 1;
    gensum(&nvram_vars);
#ifdef FLASH_DEBUG
    flash_write_env();
#endif
    return(0);
}

/* 
 * maximum size of the environment variable strings is the body size
 * of the environment buffer less 4 bytes for the environment body checksum 
 * less 4 bytes to leave room for 2 nulls at the end of the last string.
 */
#define FLASH_MAX_ENV_SIZE (bodySize(flash_env_seg) - 8)

#define TBL_OVERFLOW(size, oldval, newval) \
    (((size) + (strlen((oldval)) - strlen((newval)))) > FLASH_MAX_ENV_SIZE)

static int
set_env_var(char *name, char *value)
{
    register struct nvram_entry *nvt;
    int rv;

    /*
     * see if the structure is intact.
     */
    if (checksum(&nvram_vars))
	    return -1;

    for (nvt = nvram_vars.nvram_tab; nvt->nt_name[0]; nvt++) {
	if(!strcmp(nvt->nt_name, name)) {
	    if (!TBL_OVERFLOW(nvram_vars.nv_size, nvt->nt_value, value))
		return(set_nv_value(&nvt->nt_value, value, 0));
	    else {
		cmn_err(CE_ALERT, "nvram table overflow, failed to set %s=%s failed\n",
			name, value);
		return(-1);
	    }
	}
    }

    /* 
     * not currently in table, create new entry 
     * first, we must check to see if the table is full.
     */
    if (nvram_vars.nv_cnt >= NVRAMTAB ||
	nvram_vars.nv_size + strlen(name) + strlen(value) + 2 > FLASH_MAX_ENV_SIZE)
	return(-1);           

    nvt = &nvram_vars.nvram_tab[nvram_vars.nv_cnt++];
    strcpy(nvt->nt_name, name);
    nvram_vars.nv_size += strlen(name) + 1;   /* +1 for '=' to be written */
    if (rv = set_nv_value(&nvt->nt_value, value, 1)) {
	nvram_vars.nv_cnt--;
	nvram_vars.nv_size -= (strlen(name) + 1);
	bzero(nvt->nt_name, strlen(name) + 1);
	gensum(&nvram_vars);
    }
    return(rv);
}

/* Set nv ram variable name to value
 *	return  0 - success, -1 on failure
 * All the real work is done in set_an_nvram().
 * There is no get_nvram because the code that would use
 * it always gets it from kopt_find instead.
 * This is called from syssgi().
 */
int
set_nvram(char *name, char *value)
{
    register struct nvram_entry *nvt;
    int valuelen = strlen(value);
    int i;
    char *var;

    /* Don't allow setting the password from Unix, only clearing. */
    if (!strcmp(name, "passwd_key") && valuelen)
	return -2;

    if (!nvram_vars.nv_good)
	return -1;

    /* check to see if it is a valid nvram name in the flash table */
    for(nvt = nvram_vars.nvram_tab; nvt->nt_name[0]; nvt++) {
	if(!strcmp(nvt->nt_name, name)) {
	    return set_env_var(nvt->nt_name, value);
	}
    }

    /* check to see if it is a known nvram variable */
    for (i = 1, var = var_tbl[0]; var; var = var_tbl[i++]) {
	if(!strcmp(var, name)) {
	    return set_env_var(var, value);
	}
    }
    return -1;
}

/*
 * body_len parameter should be total size of body (i.e., don't include header).
 */
static void
flash_create_env_hdr(FlashSegment *hp, int body_len)
{
    char *seg_name = "env";
    char *vsn_data = "1.0";
    long *fp = (long *)hp;
    long xsum = 0;

    /* Initialize the header */
    bzero((caddr_t)hp, sizeof(FlashSegment));
    hp->magic = FLASH_SEGMENT_MAGIC;
    hp->nameLen = strlen(seg_name);
    hp->vsnLen = strlen(vsn_data);
    hp->segLen = hdrSize(hp) + __RUP(body_len, long);
    bcopy(seg_name, hp->name, hp->nameLen);
    bcopy(vsn_data, version(hp), hp->vsnLen);

    while (fp != chksum(hp)) {
	xsum += *fp++;
    }

    hp->chksum = -xsum;
}

static char *
flash_copy_entry(char *buf, struct nvram_entry *nvp)
{
    strcpy(buf, nvp->nt_name);
    buf += strlen(nvp->nt_name);
    *buf++ = '=';
    strcpy(buf, nvp->nt_value);
    buf += strlen(nvp->nt_value);
    *buf++ = '\0';
    return(buf);
}

long
flash_body_cksum(long *body, long *lim)
{
    long xsum;

    for (xsum = 0; body < lim; body++)
	xsum += *body;

    return(-xsum);
}

void
flash_write_env(void)
{
    char *env_buf;	/* allocated buffer to hold entire segment before write		*/
    int env_buf_size;	/* size of buffer; determined from env header in prom		*/
    FlashSegment *seg; 	/* points to env_buf (used to find field locations)		*/
    
    struct nvram_entry *nvt = nvram_vars.nvram_tab;
    char *bufptr;	/* env_buf iterator	*/
    char *bufendptr;	/* bufptr limit */
    char *flash_addr; 	/* points to actual flash env segment, and used as iterator	*/ 
    int i;

    /*
     * allocate temporary buffer to hold name=value pairs before writing;
     * we can't do this with a static buffer since the size of the
     * environment section of the prom could change out from under us
     * (when the prom is upgraded, for instance).
     * Also round up a FLASH_PAGE_SIZE multiple, since flash is programmed
     * in integral number of FLASH_PAGE_SIZE byte chunks. 
     */
    env_buf_size = roundup(hdrSize(flash_env_seg) + bodySize(flash_env_seg),
                                   FLASH_PAGE_SIZE);
    env_buf = kmem_alloc(env_buf_size, KM_SLEEP);
    if (!env_buf) {
	cmn_err(CE_ALERT, "could not allocate environment buffer\n");
	return;
    }

    seg 	= (FlashSegment *)env_buf;
    flash_addr 	= (char *)flash_env_seg;
    bufptr 	= env_buf;

#ifdef RESET_DUMPSYS
    /*
     * check to see if we should restore the first page of flash
     * memory.
     */
    if (flash_dump_set) {
	if (flash_write_sector((char *)0xbfc00000, flash_1st_page_buf))
	    cmn_err(CE_ALERT, "could not restore flash contents\n");
	else 
	    flash_dump_set = 0;
    }
#endif /* RESET_DUMPSYS */

    /* 
     * zero out the environemnt buffer since we don't know whether
     * we got here through a normal shutdown or a panic of some kind.
     */
    bzero(env_buf, env_buf_size);


    /*
     * if the environment variables were corrupted in some way
     * when read from the nvram, we write out an empty segment
     * and let the prom create a new one one at the next boot.
     */
    if (!nvram_vars.nv_good) {
	for (i = env_buf_size/FLASH_PAGE_SIZE; i ; i--) {
	    if (flash_write_sector(flash_addr, bufptr)) {
		cmn_err(CE_ALERT, "Environment segment invalid!  Unable to zero FLASH RAM\n");
		kmem_free( env_buf, env_buf_size );
		return;
	    }
	    flash_addr += FLASH_PAGE_SIZE; bufptr += FLASH_PAGE_SIZE;
	}
	kmem_free( env_buf, env_buf_size );
	return;
    }

    if (checksum(&nvram_vars) || !nvram_vars.nv_changed ) {
	kmem_free( env_buf, env_buf_size );
	return;
    }

    /* create the header using the existing body size.  header chksum is generated for us. */
    flash_create_env_hdr(seg, bodySize(flash_env_seg) );

    /* 
     * copy the table entries to the environment buffer 
     */
    bufptr = (char *)body(seg);		/* skip to the body of the buffer */
    bufendptr = (char *)segChksum(seg);
    for(nvt = nvram_vars.nvram_tab; nvt->nt_name[0]; nvt++) {
	int entlen;
	
	entlen = strlen(nvt->nt_name) + strlen(nvt->nt_value) + 2; /* '=' & '\0' also */
	if (bufptr + entlen >= bufendptr) {
	    /* 
	     * this should not have happend, the TBL_OVERFLOW should have
	     * prevented nvram table overflow. Possible data corruption
	     * we may lose nvram variable set but avoid flashing the
	     * the envSeg since we may really hose the nvram.
	     */ 
	    cmn_err(CE_ALERT, "nvram table overflow, skip programming FLASH RAM\n");
	    kmem_free( env_buf, env_buf_size );
	    return;
	}

	bufptr = flash_copy_entry(bufptr, nvt);
    }

    /* calculate and assign new body checksum */
    *segChksum(seg) = flash_body_cksum( body(seg)	/* base */, 
				    	segChksum(seg)	/* limit */ ); 

    /*
     * do the actual write out to prom
     */
    bufptr = env_buf;	/* read pointer */
    for (i = env_buf_size/FLASH_PAGE_SIZE; i ; i--) {
	if (flash_write_sector(flash_addr, bufptr)) {
	    cmn_err(CE_ALERT, "Environment segment invalid! Unable to program FLASH RAM\n");
	    return;
	}
	flash_addr += FLASH_PAGE_SIZE; bufptr += FLASH_PAGE_SIZE;
    }
    
    nvram_vars.nv_changed = 0;
    gensum(&nvram_vars);
    
    kmem_free( env_buf, env_buf_size );
    return;
}

void
flash_set_nvram_changed()
{
    struct nvram_entry *nvt;

    /* copy the nvram table to RAM (unless it's already there) */
    nvt = nvram_vars.nvram_tab;
    if (nvt->nt_value > flash_env_base && nvt->nt_value < flash_env_lim) {
	for( ; nvt->nt_name[0]; nvt++)
		set_nv_value(&nvt->nt_value, nvt->nt_value, 0);
    }
    nvram_vars.nv_changed = 1;
    gensum(&nvram_vars);
}


/*
 * prom started taking care of memory ecc
 * initialization in version 2.0.
 */
#define PROM_ECCINIT_MAJ	2
#define PROM_ECCINIT_MIN	0

int
prom_does_eccinit(void)
{
    static int ver_initted = 0;

    if (!ver_initted) {
	ver_initted = 1;
	get_flash_version();
    }
	
#if 0
    if (flash_ver[FLASHPROM_MAJOR] >= PROM_ECCINIT_MAJ)
	return 1;

    if (flash_ver[FLASHPROM_MAJOR] == PROM_ECCINIT_MAJ &&
	flash_ver[FLASHPROM_MINOR] >= PROM_ECCINIT_MIN)
	return 1;
#endif

    /*
     * It appears as though the version 2 PROM DOES NOT
     * properly zero memory, so we'll let the kernel do
     * it all until the PROM problems are resolved.
     */
    return 0;
}

/*
 * this routine should be called both at initialization time, and any time after
 * which the prom version may have changed (for example, after syssgi(SGI_SET_IP32_FLASH, ))
 * It syncs up global pointers and version information with the prom.
 */
void
flash_sync_globals(void)
{
    inventory_t *finv;
    /*
     * find the environment segment of the flash, and set up related pointers
     * appropriately.
     */
    flash_env_seg = find_named_flash_seg("env");
    if (flash_env_seg) {
	flash_env_base = (char *)body(flash_env_seg);
	flash_env_lim = (char *)(body(flash_env_seg) + bodySize(flash_env_seg));
    } else {
	/* this is a Bad Thing */
	cmn_err(CE_ALERT, "could not locate environment segment of PROM.\n");
	flash_env_base = (char *)NULL;
	flash_env_lim = (char *)NULL;
    }

    /*
     * force a reread of the version information
     */
    update_flash_version();

    /*
     * Update Flash PROM info in the inventory
     * We may have reprogammed the flash PROM
     */
    if (flash_initted) {
	finv = find_inventory(0, INV_MEMORY, INV_PROM, -1, -1, 0);
	if (finv)
	    replace_in_inventory(finv, INV_MEMORY, INV_PROM,
				 flash_version(FLASHPROM_MAJOR),
				 flash_version(FLASHPROM_MINOR), 0);
	else
	    add_to_inventory(INV_MEMORY, INV_PROM,
				 flash_version(FLASHPROM_MAJOR),
				 flash_version(FLASHPROM_MINOR), 0);
    }

}

#if DEBUG
void
flash_dump_globals(void)
{
    printf("FLASH POINTERS:\n");
    printf("  environment segment   : %Xh\n", flash_env_seg);
    printf("  environment body base : %Xh\n", flash_env_base);
    printf("  environment body limit: %Xh\n", flash_env_lim);
}
#endif /* DEBUG  */


/* END of NVRAM stuff */
