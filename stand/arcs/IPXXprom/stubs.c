/* stubs need to keep real code out */
#include <arcs/hinv.h>
#include <arcs/errno.h>
#include <stringlist.h>
#include <uif.h>
#include <libsc.h>
#include <pon.h>

#include "ipxx.h"

LONG RegisterDriverStrategy(struct component *, STATUS (*)(void *, void *));
/*ARGSUSED*/
STATUS fake_strat(void *a, void *b)	{return(EIO);}


void _hook_exceptions() {}

static int rlstub;
int *Reportlevel = &rlstub;

int sk_sable = 0;

#define		NVRAM_LEN	256

#define		CONSOLE_ADDR	0
#define		CONSOLE_LEN	3
#define		AUTOLOAD_ADDR	CONSOLE_ADDR+CONSOLE_LEN
#define		AUTOLOAD_LEN	4
#define		DISKLESS_ADDR	AUTOLOAD_ADDR+AUTOLOAD_LEN
#define		DISKLESS_LEN	2
#define		DIAGMODE_ADDR	DISKLESS_ADDR+DISKLESS_LEN
#define		DIAGMODE_LEN	3
#define		OSLOAD_ADDR	DIAGMODE_ADDR+DIAGMODE_LEN
#define		OSLOAD_LEN	50
#define		VERBOSE_ADDR	OSLOAD_ADDR+OSLOAD_LEN
#define		VERBOSE_LEN	2
#define		SGILOGO_ADDR	VERBOSE_ADDR+VERBOSE_LEN
#define		SGILOGO_LEN	2
#define		NETADDR_ADDR	SGILOGO_ADDR+SGILOGO_LEN
#define		NETADDR_LEN	20

struct nvram {
	char *name;
	char *deflt;
	int  addr;
	int  len;
} nvram[] = {
	"console",	"g",		CONSOLE_ADDR,	CONSOLE_LEN,
	"AutoLoad",	"No",		AUTOLOAD_ADDR,	AUTOLOAD_LEN,
	"diskless",	"",		DISKLESS_ADDR,	DISKLESS_LEN,
	"diagmode",	"",		DIAGMODE_ADDR,	DIAGMODE_LEN,
	"OSLoader",	"sash",		OSLOAD_ADDR,	OSLOAD_LEN,
	"VERBOSE",	"",		VERBOSE_ADDR,	VERBOSE_LEN,
	"sgilogo",	"y",		NETADDR_ADDR,	NETADDR_LEN,
	0,		0,		0,		0
};

#define NVFILE	"nvram"
static char nvbuf[NVRAM_LEN];

void
init_env(void)
{
	extern struct string_list environ_str;
	extern int ioserial,autoload,vdiagmode,setdl;
	extern char **environ;
	int reset=0, i, fd;

	init_str(&environ_str);

	environ = environ_str.strptrs;

	if ((fd=l_open(NVFILE,0,0)) < 0)
		reset = 1;
	else if (l_read(fd,nvbuf,NVRAM_LEN) < NVRAM_LEN)
		reset = 1;

	if (reset) {
		printf("NVRAM corrupt, reinitializing\n");

		bzero(nvbuf,NVRAM_LEN);

		if ((fd=l_open(NVFILE,0x101,0666)) < 0) {
			printf("cannot open nvram file!\n");
			l_exit(1);
		}

		for (i=0; nvram[i].name; i++) {
			syssetenv(nvram[i].name,nvram[i].deflt);
			strncpy(nvbuf+nvram[i].addr,nvram[i].deflt,
				nvram[i].len-1);
		}

		l_write(fd,nvbuf,NVRAM_LEN);
	}
	else {
		for (i=0; nvram[i].name; i++) {
			if (nvbuf[nvram[i].addr]) {
				replace_str(nvram[i].name,&nvbuf[nvram[i].addr],
					&environ_str);
			}
		}
	}

	l_close(fd);


	/* command line override of nvram/defaults */
	if (ioserial) replace_str("console","d",&environ_str);
	if (autoload) replace_str("AutoLoad","yes",&environ_str);
	if (setdl) replace_str("diskless","1",&environ_str);
	if (vdiagmode) replace_str("diagmodes","v",&environ_str);

	syssetenv("cpufreq","no");

	init_consenv(0);
}

int
setenv_nvram(char *name, char *new)
{
	int fd, i;

	for (i=0; nvram[i].name; i++) {
		if (!strcasecmp(nvram[i].name,name)) {
			fd=l_open(NVFILE,1,0);
			bcopy(new,nvbuf+nvram[i].addr,
				nvram[i].len-1);
			l_write(fd,nvbuf,NVRAM_LEN);
			l_close(fd);

			return 0;
		}
	}
	return(EINVAL);
}

int nvram_is_protected() {return(0);}

void _z8530_strat() {}			/* sgi_ms.c */
void reset_enet_carrier() {}		/* if_ec2.c */
void enet_carrier_on() {}		/* if_ec2.c */
void kb_translate() {}			/* serialcalls.c */

void wbflush() {}

void gr2_install() {}
void gr2_probe() {}
void lg1_install() {}
void lg1_probe() {}

void play_hello_tune() {}

/* pon stubs */
extern int faildiags, failgdiags, *Reportlevel;
#define msg_printf(X,Y) if (*Reportlevel == X) printf(Y);
pon_graphics()
{
	msg_printf(VRB,"stub pon graphics.\n");
	if (failgdiags)
		return(FAIL_BADGRAPHICS);
	_init_htp();
	return(0);
}
pon_caches()
{
	
	msg_printf(VRB,"stub pon caches.\n");
	l_sginap(50);
	return faildiags;
}
int2_mask()
{
	msg_printf(VRB,"stub pon int2.\n");
	return faildiags;
}
scsi()
{
	msg_printf(VRB,"stub pon scsi.\n");
	l_sginap(50);
	return 0;
}
duart_loopback()
{
	msg_printf(VRB,"stub duart loopback.\n");
	l_sginap(200);
	return faildiags;
}

#ifndef NFAKETAPES
#define NFAKETAPES	1
#endif
#ifndef NFAKECDS
#define NFAKECDS	1
#endif
#ifndef NFAKENET
#define NFAKENET	1
#endif

static COMPONENT netctrl = {
	ControllerClass,
	NetworkController,
	0,
	SGI_ARCS_VERS,
	SGI_ARCS_REV,
	0,
	0x01,
	0,
	0,
	NULL
};
static COMPONENT net = {
	PeripheralClass,
	NetworkPeripheral,
	0,
	SGI_ARCS_VERS,
	SGI_ARCS_REV,
	0,
	0x01,
	0,
	0,
	NULL
};
static COMPONENT cdctrl = {
	ControllerClass,
	CDROMController,
	0,
	SGI_ARCS_VERS,
	SGI_ARCS_REV,
	0,
	0x01,
	0,
	0,
	NULL
};
static COMPONENT cd = {
	PeripheralClass,
	DiskPeripheral,
	0,
	SGI_ARCS_VERS,
	SGI_ARCS_REV,
	0,
	0x01,
	0,
	0,
	NULL
};
static COMPONENT tapectrl = {
	ControllerClass,
	TapeController,
	0,
	SGI_ARCS_VERS,
	SGI_ARCS_REV,
	0,
	0x01,
	0,
	0,
	NULL
};
static COMPONENT tape = {
	PeripheralClass,
	TapePeripheral,
	0,
	SGI_ARCS_VERS,
	SGI_ARCS_REV,
	0,
	0x01,
	0,
	0,
	NULL
};

fake_install(COMPONENT *parent)
{
	COMPONENT *tmp;
	int i;

	for (i=0; i < NFAKETAPES; i++) {
		tapectrl.Key = i;
		tmp = AddChild(parent,&tapectrl,NULL);
		tmp = AddChild(tmp,&tape,NULL);
		RegisterDriverStrategy(tmp,fake_strat);
	}
	for (i=0; i < NFAKECDS; i++) {
		cdctrl.Key = i;
		tmp = AddChild(parent,&cdctrl,NULL);
		tmp = AddChild(tmp,&cd,NULL);
		RegisterDriverStrategy(tmp,fake_strat);
	}
	for (i=0; i < NFAKENET; i++) {
		netctrl.Key = i;
		tmp = AddChild(parent,&netctrl,NULL);
		tmp = AddChild(tmp,&net,NULL);
		RegisterDriverStrategy(tmp,fake_strat);
	}
}

int cpu_get_freq(int cpu) {return 1;}

/* keep out rXkcache.s */
int _icache_size, _dcache_size, cachewrback;
void FlushAllCaches() {}
void clean_dcache() {}
void clean_icache() {}
void flush_cache() {}
void config_cache() {}
void size_cache() {}
void clear_cache() {}

ecc_error_decode() {return 0;}
int load_double() {return 0;}
int is_protected_adrs() {return 0;}
int scuzzy[40];
int scsi_getchanmap() {return 0;}
scsi_freechanmap() {return 0;}
dma_mapalloc() {return 0;}
scsi_reset() {return 0;}
scsidma_flush() {return 0;}
dma_map() {return 0;}
scsi_go() {return 0;}

_ticksper1024inst() {return 0;}
_delayend() {return 0;}
int GetRelativeTime() {static foo; return ++foo;}

long _gio_parerr_save, _cpu_paraddr_save, _gio_paraddr_save;

void us_delay(int x) {/*XXX*/;}
int r4k_getconfig() {return 0;}
void flushbus() {}

void *memset(void *s, int c, size_t n)
{
	char *p = (unsigned char *)s;
	int i;

	for (i=0; i < n; i++) *p++ = (unsigned char)c;

	return (void *)s;
}
