#ident	"lib/libsc/cmd/hinv_cmd.c:  $Revision: 1.95 $"

/*
 * Hardware inventory command for prom/standalone programs
 *
 */
#include <sys/types.h>
#include <arcs/io.h>
#include <arcs/hinv.h>
#include <arcs/cfgdata.h>
#include <arcs/errno.h>
#include <arcs/pvector.h>
#include <libsc.h>
#include <libsc_internal.h>

static int is_MP;	/* Flag indicating whether system is multiprocessor */

static void hinv_walk(COMPONENT *, int, int);
static void prcomponent(COMPONENT *, int, int);
static void irixprcomponent(COMPONENT *, int);

#define HINV_VERBOSE	0x01
#define HINV_PATHS	0x02
#define HINV_ARCS	0x04
#define HINV_BUSINFO	0x08

#define HINVCENTER	25			/* where : is centered */

/*ARGSUSED*/
int
hinv(int argc, char **argv, char **bunk1, struct cmd_table *bunk2)
{
	int flags = 0;

	if (GetChild(NULL) == NULL) { /* must be a SN0 system */
		kl_hinv(argc, argv) ;
		return 0 ;
	}

	while (--argc) {
		++argv;
		if (!strcmp(*argv,"-v")) 
			flags |= HINV_VERBOSE;
		else if (!strcmp(*argv,"-t"))
			flags |= HINV_ARCS;
		else if (!strcmp(*argv,"-p"))
			flags |= HINV_PATHS;
		else if (!strcmp(*argv,"-b"))
			flags |= HINV_BUSINFO;
		else
			return 1;
	}

	if (flags & HINV_BUSINFO) {
		businfo(flags & HINV_VERBOSE);
	        return 0;
	}

	is_MP = (_get_numcpus() > 1);

	hinv_walk(GetChild(NULL),0,flags);


	return 0;
}

static char *_class[] = {
	"system",
	"processor",
	"cache",
#ifndef	_NT_PROM
	"memory",
	"adapter",
	"controller",
	"peripheral"
#else
	"adapter",
	"controller",
	"peripheral",
	"memory"
#endif
};

#define	_CLASS_SIZE	(sizeof _class / sizeof _class[0])

static char *_type[] = {
	"ARC",
	"CPU",
	"FPU",
	"primary icache",
	"primary dcache",
	"secondary icache",
	"secondary dcache",
	"secondary cache",
#ifndef	_NT_PROM
	"main",
#endif
	"EISA",
	"TC",
	"SCSI",
	"DTI",
	"multi function",
	"disk",
	"tape",
	"CDROM",
	"WORM",
	"serial",
	"network",
	"display",
	"parallel",
	"pointer",
	"keyboard",
	"audio",
	"other",
	"disk",
	"floppy disk",
	"tape",
	"modem",
	"monitor",
	"printer",
	"pointer",
	"keyboard",
	"terminal",
	"line",
	"network",
#ifdef	_NT_PROM
	"main",
#endif
	"other",

	"XTalk",		/* xtalk adapters like heart, xbow, hub */
	"PCI",			/* pci adapters like bridge */
	"GIO",			/* gio adapters like bridge */
	"TPU",			/* TPU adapter */
};
#define	_TYPE_SIZE	(sizeof _type / sizeof _type[0])

/* high byte class, bottom triplet class */
#define ctmunge(class,type)	((class<<24)|type)
#define MEM_MM			ctmunge(MemoryClass,Memory)
#define DEV_DISK		ctmunge(PeripheralClass,DiskPeripheral)
#define DEV_OTHER		ctmunge(ControllerClass,OtherController)
#define DEV_TAPE		ctmunge(PeripheralClass,TapePeripheral)
#define DEV_FLOPPY		ctmunge(PeripheralClass,FloppyDiskPeripheral)
#define DEV_NETWORK		ctmunge(PeripheralClass,NetworkPeripheral)
#define PROC_CPU		ctmunge(ProcessorClass,CPU)
#define PROC_FPU		ctmunge(ProcessorClass,FPU)
#define CACHE_SCACHE		ctmunge(CacheClass,SecondaryCache)
#define CACHE_PICACHE		ctmunge(CacheClass,PrimaryICache)
#define CACHE_PDCACHE		ctmunge(CacheClass,PrimaryDCache)
#define CACHE_SICACHE		ctmunge(CacheClass,SecondaryICache)
#define CACHE_SDCACHE		ctmunge(CacheClass,SecondaryDCache)
#define CTRL_DISP		ctmunge(ControllerClass,DisplayController)
#define CTRL_AUDIO		ctmunge(ControllerClass,AudioController)
#define CTRL_NETWORK		ctmunge(ControllerClass,NetworkController)
#define SYS_ARC			ctmunge(SystemClass,ARC)

#define	BUS_XTALK		ctmunge(AdapterClass, XTalkAdapter)
#define	BUS_PCI			ctmunge(AdapterClass, PCIAdapter)
#define	BUS_GIO			ctmunge(AdapterClass, GIOAdapter)
#define BUS_TPU			ctmunge(AdapterClass, TPUAdapter)
#define	PERIPH_OTHER		ctmunge(PeripheralClass,OtherPeripheral)

static void
prcfgdata(COMPONENT *c, int n)
{
	struct cfgdatahdr *p;
	char *dsh = "-";

	if (!c->ConfigurationDataSize)
		return;

	if (!(p=(struct cfgdatahdr *)malloc(c->ConfigurationDataSize)))
		panic("cannot malloc space for cfgdata\n");

	if (GetConfigurationData(p,c) != ESUCCESS) {
		printf("GetConfigurationData FAILED!\n");
		goto done;
	}

	printf("%*sv: ",2*n,"");

	printf("%s %s %s %s ",
		p->Type ? p->Type : dsh,
		p->Vendor ? p->Vendor : dsh,
		p->ProductName ? p->ProductName : dsh,
		p->SerialNumber ? p->SerialNumber : dsh);

	switch(c->Type) {
	case NetworkController:
		{
			struct net_data *nd = (struct net_data *)p;
			ulong i;

			printf("type: %s MTU: %u HW: ",
				nd->NetType, nd->NetMaxlength);
			for (i = 0; i < nd->NetAddressSize; i++)
				printf("%s%x", !i?"":":",
				       nd->NetAddress[i]);
		}
		break;
	case DiskPeripheral:
			if (c->Type && !strcmp(p->Type,SCSIDISK_TYPE)) {
				struct scsidisk_data *d =
					(struct scsidisk_data *)p;
				printf("blocksize: %u maxblocks: %u",
					d->BlockSize, d->MaxBlocks);
			}
		break;
	}
	printf("\n");
done:
	free(p);
	return;
}

static void
prcomponent(COMPONENT *c, int n, int f)
{
	long classtype = ctmunge(c->Class,c->Type);

	printf("%*s",2*n,"");
	printf("%s %s",
	       c->Class < _CLASS_SIZE ? _class[c->Class] : "invalid class",
	       c->Type < _TYPE_SIZE ? _type[c->Type] : "invalid type");
	if (c->Identifier)
		printf(" %s",c->Identifier);

	switch (classtype) {
	case DEV_DISK:
	case DEV_TAPE:
		printf(" unit %u",c->Key);
		break;
	case CACHE_SCACHE:
	case CACHE_PICACHE:
	case CACHE_PDCACHE:
	case CACHE_SICACHE:
	case CACHE_SDCACHE:
	{
		union key_u key;

		key.FullKey = c->Key;

		printf(" %d Kbytes (block %d lines, line %d bytes)",
		       (1<<key.cache.c_size)*4,
		       key.cache.c_bsize,
		       1<<key.cache.c_lsize);
		break;
	}

	case MEM_MM:
		printf(" %u Mbytes",c->Key/256);
		break;

	default:
		printf(" key %u",c->Key);
		break;
	}
	if ((f & HINV_VERBOSE) && c->Flags)
		printf(" (%s%s%s%s%s%s%s )",
		       c->Flags & Failed ? " Failed" : "",
		       c->Flags & ReadOnly ? " ReadOnly" : "",
		       c->Flags & Removable ? " Removable" : "",
		       c->Flags & ConsoleIn ? " ConsoleIn" : "",
		       c->Flags & ConsoleOut ? " ConsoleOut" : "",
		       c->Flags & Input ? " Input" : "",
		       c->Flags & Output ? " Output" : "");
	printf("\n");
	if (f & HINV_VERBOSE)
		prcfgdata(c,n);

	if (f & HINV_PATHS) {
		char *p = (char *)getpath(c);
		if (p) printf("%*sp: %s\n",2*n,"",getpath(c));
	}
}
/*ARGSUSED*/
static void
irixprcomponent(COMPONENT *c, int f)
{
	long classtype = ctmunge(c->Class,c->Type);
	COMPONENT *pp, *p = GetParent(c);
	char *str;

	switch(classtype) {
	case DEV_OTHER:
		if (p->Type == SCSIAdapter) {
			if (c->Type == OtherController) {
                               printf("%*s: Controller %lu ID %lu\n",HINVCENTER,
                                       "SCSI Device",p->Key,c->Key);
			}
		}
		break;
	case DEV_DISK:
		pp = GetParent(p);
		if (pp->Type == SCSIAdapter) {
			if (p->Type == CDROMController)
				printf("%*s: scsi(%u)cdrom(%u)\n",
				       HINVCENTER,"SCSI CDROM",
				       pp->Key, p->Key);
			else
				printf("%*s: scsi(%u)disk(%u)\n",HINVCENTER,
				       "SCSI Disk",pp->Key,p->Key);
		}
		/* only support SCSI disk so far */
		break;
	case DEV_TAPE:
		pp = GetParent(p);
		if (pp->Type == SCSIAdapter) {
			printf("%*s: scsi(%u)tape(%u)\n",HINVCENTER,"SCSI Tape",
			       pp->Key,p->Key);
		}
		/* only support SCSI tape so far */
		break;
	case DEV_FLOPPY:
		pp = GetParent(p);
		if (pp->Type == SCSIAdapter) {
			printf("%*s: Controller %u, ID %u, removable media\n",
			       HINVCENTER,"SCSI Disk", pp->Key, p->Key);
		}
		break;
#ifndef _NT_PROM
	case MEM_MM:
		printf("%*s: %u Mbytes\n",HINVCENTER,"Memory size",c->Key/256);
		break;
#endif
	case SYS_ARC:			/* root of tree */
		printf("%*s: %s\n",HINVCENTER,"System",c->Identifier+4);
		break;
	case PROC_CPU:			/* handle cpu/fpu */
		printf("%*s: %d Mhz %s",HINVCENTER,"Processor",
		       cpufreq((int)c->Key), c->Identifier+5);
	
	        /* Display scache information */
		if (is_MP && (pp = find_type(c, SecondaryCache)) && 
		    (pp->Type == SecondaryCache)) {
		    union key_u key;

		    key.FullKey = pp->Key;
		    printf(", %dM secondary cache", (1<<key.cache.c_size)/256);
		}
 
		if (c->Key != 0)
			printf(", (cpu %u)", c->Key);
		if ((pp = GetChild(c)) && (pp->Type == FPU) && !is_MP)
			printf(", with FPU");
		printf("\n");
		break;
	case CACHE_SCACHE:
		str = "Secondary cache size";
		goto commoncache;
	case CACHE_PICACHE:
		str = "Primary I-cache size";
		goto commoncache;
	case CACHE_PDCACHE:
		str = "Primary D-cache size";
		goto commoncache;
	case CACHE_SICACHE:
		str = "Secondary I-cache size";
		goto commoncache;
	case CACHE_SDCACHE:
		str = "Secondary D-cache size";
commoncache:
		{
			union key_u key;

			key.FullKey = c->Key;

			if (!is_MP || (f & HINV_VERBOSE)) {
				/* Caches 2MB and bigger report as MB.  Pick
				 * this number so old R4k systems do not
				 * change.
				 */
				if (key.cache.c_size >= 9)
					printf("%*s: %d Mbytes\n",HINVCENTER,
						str,1<<(key.cache.c_size-8));
				else
					printf("%*s: %d Kbytes\n",HINVCENTER,	
						str,(1<<key.cache.c_size)*4);
			}
		}
		break;
	case CTRL_DISP:
		printf("%*s: %s\n",HINVCENTER,"Graphics",c->Identifier+4);
		break;
	case CTRL_AUDIO:
		if (!strcmp("HAL2",c->Identifier)) {
			printf("%*s: Iris Audio Processor: version A2 revision %d.%d.%d\n",HINVCENTER,
				"Audio",
				(c->Key>>12)&0x7,	/* board */
				(c->Key>>4)&0xf,	/* chip major */
				c->Key&0xf);		/* chip minor */
		}
		else {
			printf("%*s: %s",HINVCENTER,"Audio",c->Identifier);
			if (c->Key) printf(" revision %d\n",c->Key);
			putchar('\n');
		}
		break;
	case CTRL_NETWORK:
		if (!strncmp("MACE",c->Identifier,4)) {
			printf("%*s: %s", HINVCENTER, "Network",
				c->Identifier+5);
			printf(" revision %d\n", c->Key);
		}
		break;

	case BUS_XTALK:
		if (c->Key != 0) /* don't report primary (built-in) XTalk */
			printf("%*s: %s %d\n",HINVCENTER,
			       "XIO Bus",c->Identifier,c->Key);
		break;

	case BUS_PCI:
		/* Don't report any PCI buses that want to hide (mainly
		 * built in PC buses now.
		 */
		if (c->Identifier && c->Identifier[0] != '\0')
			printf("%*s: %s (%d)\n",HINVCENTER,
			       "PCI Bus",c->Identifier,c->Key);
		break;

	case BUS_GIO:
		printf("%*s: %s (%d)\n",HINVCENTER,
		       "GIO Bus",c->Identifier,c->Key);
		break;

	case BUS_TPU:
		printf("%*s: %s\n", HINVCENTER,
		       "TPU",c->Identifier);
		break;

	case PERIPH_OTHER:
		/* Print Unknown devices if bus code adds info about it */
		if (c->Identifier) {
			if (p->Type == PCIAdapter) {
			    printf("%*s: %s\n",HINVCENTER,
				    "PCI Device",c->Identifier);
			} else {
			    printf("%*s: %s key %u\n",HINVCENTER,
				    "Unknown Device",c->Identifier,c->Key);
			}
		}
		break;
	case DEV_NETWORK:
		/* For the builtin network device */
		if (c->IdentifierLength && c->Identifier && c->Identifier[0] != '\0')
		    printf("%*s: %s\n",HINVCENTER,
			"Network", c->Identifier);
		break;
	}
}

static void
hinv_walk(COMPONENT *c, int n, int f)
{
	if (!c) return;

	if (f&HINV_ARCS)
		prcomponent(c,n,f);
	else
		irixprcomponent(c,f);
	hinv_walk(GetChild(c),n+1,f);
	hinv_walk(GetPeer(c),n,f);
	return;
}
