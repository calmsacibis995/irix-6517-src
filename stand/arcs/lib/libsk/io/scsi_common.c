/*
 * Common scsi functions.  Things that the wd93 (aka scsi), wd95, ql, adp78 do
 * can all be done here.
 *
 * $Ident: $
 */
#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/buf.h>
#include <sys/debug.h>
#include <sys/splock.h>
#include <sys/kopt.h>
#include <sys/cmn_err.h>
#include <sys/dmamap.h>
#include <sys/scsi.h>
#include <sys/scsidev.h>
#include <sys/invent.h>

#include <arcs/hinv.h>
#include <arcs/cfgdata.h>
#include <libsk.h>
#include <libsc.h>

#ifdef MHSIM
#include "../../../IP32/include/DBCsim.h"
#define PRINT		DPRINTF
#else
#define PRINT		printf
#endif

#define splockspl(x,y) 0 
#define spunlockspl(x,y)
#define splock(x) 0
#define spunlock(x,y)
#define initnlock(x,y)
#define spsema(x)
#define svsema(x)

/*
 * Sense key and additional sense messages.  These are used by dksc, tpsc, etc.
 */
char *scsi_key_msgtab[SC_NUMSENSE] = {
        "No sense",                             /* 0x0 */
        "Recovered Error",                      /* 0x1 */
        "Drive not ready",                      /* 0x2 */
        "Media error",                          /* 0x3 */
        "Unrecoverable device error",           /* 0x4 */
        "Illegal request",                      /* 0x5 */
        "Unit Attention",                       /* 0x6 */
        "Data protect error",                   /* 0x7 */
        "Unexpected blank media",               /* 0x8 */
        "Vendor Unique error",                  /* 0x9 */
        "Copy Aborted",                         /* 0xa */
        "Aborted command",                      /* 0xb */
        "Search data successful",               /* 0xc */
        "Volume overflow",                      /* 0xd */
        "Reserved (0xE)",                       /* 0xe */
        "Reserved (0xF)",                       /* 0xf */
};

/*
 * A number of previously unspec'ed errors were spec'ed in SCSI 2; these were
 * added from App I, SCSI-2, Rev 7.  The additional sense qualifier adds more
 * info on some of these...
 */
char *scsi_addit_msgtab[SC_NUMADDSENSE] = {
        NULL,   /* 0x00 */
        "No index/sector signal",               /* 0x01 */
        "No seek complete",                     /* 0x02 */
        "Write fault",                          /* 0x03 */
        "Driver not ready",                     /* 0x04 */
        "Drive not selected",                   /* 0x05 */
        "No track 0",                           /* 0x06 */
        "Multiple drives selected",             /* 0x07 */
        "LUN communication error",              /* 0x08 */
        "Track error",                          /* 0x09 */
        "Error log overflow",                   /* 0x0a */
        NULL,                                   /* 0x0b */
        "Write error",                          /* 0x0c */
        NULL,                                   /* 0x0d */
        NULL,                                   /* 0x0e */
        NULL,                                   /* 0x0f */
        "ID CRC or ECC error",                  /* 0x10  */
        "Unrecovered data block read error",    /* 0x11 */
        "No addr mark found in ID field",       /* 0x12 */
        "No addr mark found in Data field",     /* 0x13 */
        "No record found",                      /* 0x14 */
        "Seek position error",                  /* 0x15 */
        "Data sync mark error",                 /* 0x16 */
        "Read data recovered with retries",     /* 0x17 */
        "Read data recovered with ECC",         /* 0x18 */
        "Defect list error",                    /* 0x19 */
        "Parameter overrun",                    /* 0x1a */
        "Synchronous transfer error",           /* 0x1b */
        "Defect list not found",                /* 0x1c */
        "Compare error",                        /* 0x1d */
        "Recovered ID with ECC",                /* 0x1e */
        NULL,                                   /* 0x1f */
        "Invalid command code",                 /* 0x20 */
        "Illegal logical block address",        /* 0x21 */
        "Illegal function",                     /* 0x22 */
        NULL,                                   /* 0x23 */
        "Illegal field in CDB",                 /* 0x24 */
        "Invalid LUN",                          /* 0x25 */
        "Invalid field in parameter list",      /* 0x26 */
        "Media write protected",                /* 0x27 */
        "Media change",                         /* 0x28 */
        "Device reset",                         /* 0x29 */
        "Log parameters changed",               /* 0x2a */
        "Copy requires disconnect",             /* 0x2b */
        "Command sequence error",               /* 0x2c */
        "Update in place error",                /* 0x2d */
        NULL,                                   /* 0x2e */
        "Tagged commands cleared",              /* 0x2f */
        "Incompatible media",                   /* 0x30 */
        "Media format corrupted",               /* 0x31 */
        "No defect spare location available",   /* 0x32 */
        "Media length error",                   /* 0x33 (tape only) */
        NULL,                                   /* 0x34 */
        NULL,                                   /* 0x35 */
        "Toner/ink error",                      /* 0x36 */
        "Parameter rounded",                    /* 0x37 */
        NULL,                                   /* 0x38 */
        "Saved parameters not supported",       /* 0x39 */
        "Medium not present",                   /* 0x3a */
        "Forms error",                          /* 0x3b */
        NULL,                                   /* 0x3c */
        "Invalid ID msg",                       /* 0x3d */
        "Self config in progress",              /* 0x3e */
        "Device config has changed",            /* 0x3f */
        "RAM failure",                          /* 0x40 */
        "Data path diagnostic failure",         /* 0x41 */
        "Power on diagnostic failure",          /* 0x42 */
        "Message reject error",                 /* 0x43 */
        "Internal controller error",            /* 0x44 */
        "Select/Reselect failed",               /* 0x45 */
        "Soft reset failure",                   /* 0x46 */
        "SCSI interface parity error",          /* 0x47 */
        "Initiator detected error",             /* 0x48 */
        "Inappropriate/Illegal message",        /* 0x49 */
};

extern int scsi_ha_id[];
extern void adp_cdrom_modeselect(int, int, int);

union scsicfg {
	struct scsidisk_data disk;
	struct floppy_data fd;
};

/* template for scsi adapter */
static COMPONENT scsi_adapter_tmpl = {
	AdapterClass,			/* Class */
	SCSIAdapter,			/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	"\0"				/* Identifier */
};

/* template for scsi controller. */
static COMPONENT scsictrltmpl = {
	ControllerClass,		/* Class */
	0,				/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	"\0"				/* Identifier */
};

/* template for scsi device */
static COMPONENT scsitmpl = {
	PeripheralClass,		/* Class */
	0,				/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	"\0"				/* Identifier */
};


/*
 * Run configuration for the scsi adpater "name" at controller "key",
 * registering all child devices.  Parent is the parent of the adapter.
 */
void
scsiinstall_adapter(struct component *parent, char *name, int key)
{
	COMPONENT *adapter_node;
	char *adapter_name;
	int len;

/*	DPRINTF("scsiinstall_adapter: parent component %s at 0x%x\n",
	       parent->Identifier, parent); */

	len = strlen(name) + 1;
	adapter_name = malloc(len);
	strcpy(adapter_name, name);
	scsi_adapter_tmpl.IdentifierLength = len;
	scsi_adapter_tmpl.Identifier = adapter_name;
	scsi_adapter_tmpl.Key = key;
	adapter_node = AddChild(parent, &scsi_adapter_tmpl, (void *) NULL);

	scsiinstall(adapter_node);
}


/*
 * Run configuration for the scsi adpater c registering all child devices.
 */
void
scsiinstall(struct component *c)
{
	int adap = c->Key;
	char *which, *extra, *inv;
	u_char targ, lu=0, mintarg=0;
	COMPONENT *tmp, *tmp2;
	union scsicfg cfgd;
	char id[26];		/* 8 + ' ' + 16 + null */
	int type;
	char *shared_disk_string;
	int shared_disk_num;
	int maxtarg;

/*	DPRINTF("scsiinstall (devices): parent component %s at 0x%x\n",
	       c->Identifier, c); */

        scsi_seltimeout(adap, 1);	/* shorten selection timeout */

	/* now add all the scsi devices for this adapter to the inventory.
	 * It is now back in scsi.c, rather than scattered in the different
	 * SCSI drivers.  This speeds things up a bit, since we scan once
	 * per adapter, rather once for all adapters in each SCSI driver.
	 * rather than once per driver, and reduces code size a bit also.
	 * For kernel, it means we always inventory all the devices,
	 * even if the driver isn't present.  This will allow us to
	 * add 'probing' for SCSI drivers to lboot.  For now, do NOT probe
	 * for LUN's other than 0; it turns out that some devices (including
	 * the Tandberg QIC tape drive) do not handle it correctly) ...*/

	if (adap == 1) {
		shared_disk_string = getenv("shared_disk_hack");
		if (shared_disk_string != NULL) {
			printf("shared_disk_hack = %s\n",
			       shared_disk_string);
			shared_disk_num = atoi(shared_disk_string);
			if (shared_disk_num) {
				mintarg=3;
				printf("setting adap %d mintarg to %d\n",
				       adap, mintarg);
			}
		}
	}


#ifdef IP32
	/*
	 * Moosehead adapter 0 has only targs 1-4
	 */
	if (adap == 0)
		maxtarg=5;
	else
		maxtarg=SC_MAXTARG;
#else
	maxtarg=SC_MAXTARG;
#endif


	for(targ=mintarg; targ < maxtarg; targ++) {
		if(scsi_ha_id[adap] == targ)
			continue;

		if(!(inv = (char *)scsi_inq(adap, targ, lu)) ||
			(inv[0]&0x10) == 0x10) 
			continue; /* LUN not supported, or not attached */

		/* ARCS Identifier is "Vendor Product" */
		strncpy(id,inv+8,8); id[8] = '\0';
		for (extra=id+strlen(id)-1; *extra == ' '; *extra-- = '\0') ;
		strcat(id," ");
		strncat(id,inv+16,16); id[25] = '\0';
		for (extra=id+strlen(id)-1; *extra == ' '; *extra-- = '\0') ;
		scsictrltmpl.IdentifierLength = strlen(id);
		scsictrltmpl.Identifier = id;

		switch(inv[0]) {
		case 0:	/* hard disk */
		    extra = "disk";
		    which = (inv[1] & 0x80) ? "removable " : "";

		    scsictrltmpl.Type = DiskController;
		    scsictrltmpl.Key = targ;

		    tmp = AddChild(c,&scsictrltmpl,(void *)NULL);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("disk ctrl");

		    if (inv[1]&0x80) {
			    scsitmpl.Type = FloppyDiskPeripheral;
			    scsitmpl.Flags = Removable|Input|Output;
			    scsitmpl.ConfigurationDataSize =
				    sizeof(struct floppy_data);
			    bzero(&cfgd.fd,sizeof(struct floppy_data));
			    cfgd.fd.Version = SGI_ARCS_VERS;
			    cfgd.fd.Revision = SGI_ARCS_REV;
			    cfgd.fd.Type = FLOPPY_TYPE;
		    }
		    else {
			    scsitmpl.Type = DiskPeripheral;
			    scsitmpl.Flags = Input|Output;
			    scsitmpl.ConfigurationDataSize =
				    sizeof(struct scsidisk_data);
			    bzero(&cfgd.disk,sizeof(struct scsidisk_data));
			    /* sends out another inquiry command, but why? */
/*			    scsi_rc(adap,targ,lu,&cfgd.disk); */
			    cfgd.disk.Version = SGI_ARCS_VERS;
			    cfgd.disk.Revision = SGI_ARCS_REV;
			    cfgd.disk.Type = SCSIDISK_TYPE;
		    }
		    scsitmpl.Key = lu;

		    tmp = AddChild(tmp,&scsitmpl,(void *)&cfgd);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("scsi disk");
		    RegisterDriverStrategy(tmp,dksc_strat);
		    break;

		case 1:	/* sequential == tape */
		    type = tpsctapetype(inv, 0);
		    which = "tape";
		    extra = inv;
		    sprintf(inv, " type %d", type);

		    scsictrltmpl.Type = TapeController;
		    scsictrltmpl.Key = targ;
		    tmp = AddChild(c,&scsictrltmpl,(void *)NULL);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("tape ctrl");

		    scsitmpl.Type = TapePeripheral;
		    scsitmpl.Flags = Removable|Input|Output;
		    scsitmpl.ConfigurationDataSize = 0;
		    scsitmpl.Key = lu;

		    tmp = AddChild(tmp,&scsitmpl,(void *)NULL);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("scsi tape");
		    RegisterDriverStrategy(tmp,tpsc_strat);
		    break;
		default:
		    type = inv[1] & INV_REMOVE;
		    /* ANSI level: SCSI 1, 2, etc.*/
		    type |= inv[2] & INV_SCSI_MASK;

		    if ((inv[0]&0x1f) == INV_CDROM) {
			    scsictrltmpl.Type = CDROMController;
			    scsitmpl.Type = DiskPeripheral;
			    scsitmpl.Flags = ReadOnly|Removable|Input;
			    which = "CDROM";
			    adp_cdrom_modeselect(adap, targ, 0);
		    }
		    else {
			    scsictrltmpl.Type = OtherController;
			    scsitmpl.Type = OtherPeripheral;
			    scsitmpl.Flags = 0;
			    sprintf(which=inv, " device type %u",
					inv[0]&0x1f);
		    }

		    scsitmpl.ConfigurationDataSize = 0;
		    scsictrltmpl.Key = targ;
		    tmp = AddChild(c,&scsictrltmpl,(void *)NULL);
		    if (tmp == (COMPONENT *)NULL) cpu_acpanic("other ctrl");

		    scsitmpl.Key = lu;

		    tmp2 = AddChild(tmp,&scsitmpl,(void *)NULL);
		    if (tmp2 == (COMPONENT *)NULL) cpu_acpanic("scsi device");

		    if (tmp->Type == CDROMController)
			RegisterDriverStrategy(tmp2,dksc_strat);

		    extra = "";
		    break;
		}
		if(showconfig) {
		    cmn_err(CE_CONT,"SCSI %s%s (%d,%d", which,
			extra?extra:"", adap, targ);
		    if(lu)
			cmn_err(CE_CONT, ",%d", lu);
		    cmn_err(CE_CONT, ")\n");
		}
	}

	scsi_seltimeout(adap, 0);       /* standard selection timeout */

	/* revert the Toshiba CDROM to claiming to be a CDROM.  This is so
	 * newer proms correctly show it in the hardware inventory, and also
	 * so sash can do auto-installation from it.  This requires the
	 * SCSI driver, but is not a driver in and of itself. */
/*	revertcdrom(); */
}
