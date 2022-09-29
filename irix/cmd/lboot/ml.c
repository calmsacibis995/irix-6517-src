/*
 * ml.c - loadable module support functions
 *
 * $Revision: 1.49 $
 */
#include "lboot.h"
#include "boothdr.h"

#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/fcntl.h>

#include <sys/syssgi.h>
#include <sys/param.h>
#include <syslog.h>
#include <sys/conf.h>
#include <sys/fstyp.h>
#include <sys/edt.h>
#include <sys/vmereg.h>
#include <sys/mload.h>
#include <sys/capability.h>

void do_mlist(void);
void do_mloadreg(int, char *[], int);
void do_munloadreg(int, int[], int);
void do_mregister(void);
static long mloadreg(struct driver *, int);
static void prmodule(mod_info_t *);
static char *ml_err(int);
static medt_t *makeedt(struct lboot_edt *);

static int autoreg = 0;		/* doing autoreg or manual reg */
extern char *slash_boot;	/* path to /usr/sysgen/boot */
extern struct driver *driver;	/* head of struct driver linked list */

/*
 * do_mlist - List out all currently loaded and registered modules
 */
void
do_mlist(void)
{
	int		size,
			i,
			modcnt;
	mod_info_t	*mod,
			*modspc;

	if (!(size = (int)syssgi(SGI_MCONFIG, CF_LIST, 0))) {
		fprintf(stderr, "No modules loaded or registered.\n");
		exit (1);
	}

	modspc = (mod_info_t *)malloc(size);
	if (!modspc) {
		fprintf(stderr, "No memory for module list.\n");
		exit (1);
	}

	if (syssgi(SGI_MCONFIG, CF_LIST, modspc, size) != size || errno != 0) {
		fprintf(stderr, "Error copying module list: %s.\n", 
		ml_err(errno));
		exit (1);
	}

	if (!size) {
		fprintf(stderr, "Error reading module list.\n");
		exit(1);
	}
	
	if (modspc->m_cfg_version != M_CFG_CURRENT_VERSION) {
		fprintf(stderr, "Kernel version mismatch.\n");
		exit(1);
	}

	printf ("\nLoaded Modules:\n");
	modcnt = 0;
	for (mod = modspc, i = 0; i < size/sizeof(mod_info_t); ++i, ++mod) {
		if ((mod->m_flags & (M_REGISTERED|M_LOADED)) != M_LOADED)
			continue;
		modcnt++;
		prmodule(mod);
	}
	if (!modcnt)
		printf ("None.\n");

	printf ("\nRegistered Modules:\n");
	modcnt = 0;
	for (mod = modspc, i = 0; i < size/sizeof(mod_info_t); ++i, ++mod) {
		if ((mod->m_flags & (M_REGISTERED|M_LOADED)) != M_REGISTERED)
			continue;
		modcnt++;
		prmodule(mod);
	}
	if (!modcnt)
		printf ("None.\n");

	printf ("\nRegistered and Currently Loaded Modules:\n");
	modcnt = 0;
	for (mod = modspc, i = 0; i < size/sizeof(mod_info_t); ++i, ++mod) {
		if ((mod->m_flags & (M_REGISTERED|M_LOADED))
			!= (M_REGISTERED|M_LOADED))
			continue;
		modcnt++;
		prmodule(mod);
	}
	if (!modcnt)
		printf ("None.\n");
	printf ("\n");

	exit(0);
}

/*
 * do_mloadreg - Load or Register a list of modules
 */
void
do_mloadreg(int modloadc, char *modloadv[], int cmd)
{
	int i;

	/*
	 * Parse the system file.  Quit if it is bad.
	 */
	if (0 != parse_system()) {
		fprintf (stderr, "System file parse bad.\n");
		exit(1);
	}

	for ( i = 0; i < modloadc; i++ ) {
		struct driver *dp;

		if ( !(dp = searchdriver(modloadv[i])) ) {
			fprintf(stderr,"Can't find module %s.\n", modloadv[i]);
			exit(1);
		}
		
		if (!dp->dname) {
			fprintf (stderr,"Can't find object file %s/%s.[oa]\n",
				slash_boot, modloadv[i]);
			continue;
		}

		mloadreg(dp, cmd);
	}
}

/*
 * do_munloadreg - Unload or Unregister a list of modules by id number
 */
void
do_munloadreg(int modunldc, int modunldv[], int cmd)
{
	int i;
	cap_t ocap;
	cap_value_t cap_device_mgt = CAP_DEVICE_MGT;

	for ( i = 0; i < modunldc; i++ ) {
		ocap = cap_acquire(1, &cap_device_mgt);
		if (syssgi(SGI_MCONFIG, cmd, modunldv[i])) {
			fprintf(stderr, "Can not %s module id %d: %s.\n",
				(cmd == CF_UNLOAD) ? "unload" : "unregister",
				modunldv[i], 
				ml_err(errno));
		}
		cap_surrender(ocap);
	}
}

/*
 * do_mregister - Register all loadable kernel modules that have
 * an 'R' in their master file. This is called by loadunix only -
 * if Smart (lboot called from autoconfig), or if Autoreg (lboot -a). 
 */
void
do_mregister (void)
{
	long id;
	register struct driver *dp;
	cap_t ocap;
	cap_value_t cap_device_mgt = CAP_DEVICE_MGT;

	autoreg = 1;
	dp = driver;

	/*
	 * First register all of the modules that have major numbers
	 * assigned to them, then register the others. This is 
	 * necessary so that we don't grab a major number that 
	 * another module requires.
	 */

	while (dp) {

		/* 
		 * For modules that add to the h/w inventory in either
		 * init or edtinit, we must load them, then unload and
		 * register them. This is necessary since MAKEDEV will
		 * not make a device file if it doesn't find the device
		 * in hinv. If the module is just registered only, then
		 * its init and edtinit routines have not been run and
		 * hasn't been added to the hinv. If the master file
		 * contains a 'D', then we load it and then unload it.
		 */

		if ((dp->dname) && (dp->opthdr->flag & DYNAMIC) && 
			(dp->opthdr->flag & DLDREG)) {
			id = mloadreg (dp, CF_LOAD);
			ocap = cap_acquire(1, &cap_device_mgt);
			syssgi(SGI_MCONFIG, CF_UNLOAD, id);
			cap_surrender(ocap);
		}

		/* 
		 * If there's an object file and the master file 
		 * contains a 'd' and an 'R', register it.
		 */
		if ((dp->dname) && (dp->opthdr->flag & DYNAMIC) && 
			(dp->opthdr->flag & DYNREG) &&
			(dp->opthdr->soft[0] != DONTCARE))
			mloadreg (dp, CF_REGISTER);
		dp = dp->next;
	}

	dp = driver;
	while (dp) {

		/* 
		 * If there's an object file and the master file 
		 * contains a 'd' and an 'R', register it.
		 */
		if ((dp->dname) && (dp->opthdr->flag & DYNAMIC) && 
			(dp->opthdr->flag & DYNREG) &&
			(dp->opthdr->soft[0] == DONTCARE))
			mloadreg (dp, CF_REGISTER);
		dp = dp->next;
	}
	autoreg = 0;
}

/*
 * loadreg - load or register a module
 */
static long
mloadreg(struct driver *dp, int cmd)
{
	cfg_desc_t desc;
	union mod_dep m;
	char *fname;
	int error = 0;
	int i;
	cap_t ocap;
	cap_value_t cap_device_mgt = CAP_DEVICE_MGT;


	bzero(&m, sizeof m);
	bzero(&desc, sizeof desc);

	/* create a complete pathname to the module .o for register */
	fname = malloc (strlen(slash_boot) + strlen(dp->dname) + 2);
	strcpy (fname, slash_boot);
	strcat (fname, "/");
	strcat (fname, dp->dname);

	desc.m_cfg_version = M_CFG_CURRENT_VERSION;
	desc.m_fname       = fname;
	desc.m_data        = &m;
	strcpy(desc.m_prefix, dp->opthdr->prefix);

	if (dp->opthdr->flag & (CHAR|BLOCK|FUNDRV)) {

		/* Module type is driver */
		desc.m_type = M_DRIVER;

		/* Driver type is char, block, or stream */
		if ((dp->opthdr->flag & (BLOCK|CHAR)) == (BLOCK|CHAR))
			m.d.d_type = MDRV_BLOCK|MDRV_CHAR;
		else
		if (dp->opthdr->flag & CHAR)
			m.d.d_type = MDRV_CHAR;
		else
		if (dp->opthdr->flag & BLOCK)
			m.d.d_type = MDRV_BLOCK;
		else
		if (dp->opthdr->flag & FUNDRV)
			m.d.d_type = MDRV_STREAM;

		if (dp->edtp) 
			m.d.d_edtp = makeedt (dp->edtp);
		if (!dp->opthdr->nsoft ) {
			m.d.d_nmajors = 1;
			m.d.d_majors[0] = MAJOR_ANY;
		} else {
			for (i=0; i<dp->opthdr->nsoft; i++) {
				if (dp->opthdr->soft[i] == DONTCARE)
					m.d.d_majors[i] = MAJOR_ANY;
				else
					m.d.d_majors[i] = dp->opthdr->soft[i];
			}
			m.d.d_nmajors = dp->opthdr->nsoft;
		}

		/* Locking only takes place if driver's devflag says so */
		m.d.d_cpulock = -1;
		if ((dp->flag & INEDT) || dp->opthdr->sema == S_PROC )
			m.d.d_cpulock = lock_on_cpu(dp->opthdr, dp);
	} else
	if (dp->opthdr->flag & FUNMOD) {

		/* Module type is streams */
		desc.m_type = M_STREAMS;

		strcpy (m.s.s_name, dp->mname);
	} else
	if (dp->opthdr->flag & FSTYP) {
		desc.m_type = M_FILESYS;
		fprintf(stderr,"Dynamic filesystem loading not supported.\n");
		exit(1);
	} else
	if (dp->opthdr->flag & ENET) {
		desc.m_type = M_ENET;
		if (dp->edtp)
			m.e.e_edtp = makeedt (dp->edtp);
	}
	if (!(dp->opthdr->flag & DYNREG) || (dp->opthdr->flag & NOUNLD))
		desc.m_delay = M_NOUNLD;
	else
		desc.m_delay = M_UNLDDEFAULT;

	ocap = cap_acquire(1, &cap_device_mgt);
	error = (int) syssgi(SGI_MCONFIG, cmd, &desc);
	cap_surrender(ocap);
	if (error) {
		if (!autoreg)
			fprintf(stderr, "Module %s %s failed: %s.\n", 
			    fname, 
			    ((cmd == CF_LOAD) ? "load" : "register"),
			    ml_err(errno));
	} else {
		if (!autoreg)
			syslog (LOG_NOTICE, "Module %s dynamically %s.\n",
			    fname,
			    ((cmd == CF_LOAD) ? "loaded" : "registered"));
	}

	free (fname);
	if (!error)
		return (desc.m_id);
	else
		return error;
}

/*
 * makeedt - copy the edt information from lboot's version of the
 *	edt struct to the sys/edt.h version of the struct.
 */
medt_t *
makeedt (struct lboot_edt *ledtp)
{
	int i;
	struct lboot_edt *ep;
	medt_t *medtp, *mep;
	edt_t *edtp;
	struct vme_intrs *vme;
	struct iospace *iosp;
	struct lboot_iospace *liosp;

        /*
	 * If there are multiple ctrls, then lboot produces a list
	 * of edt structs for each driver. For each edt, copy the
	 * lboot info into the edt struct to be sent to mload.
	 */

	ep = ledtp;
	medtp = mep = malloc (sizeof (medt_t));
	while (ep) {
		mep->edtp = edtp = malloc (sizeof (edt_t));
		mep->medt_next = (medt_t *)0;
		edtp->e_bus_type = ep->e_bus_type;
		edtp->v_cpuintr = ep->v_cpuintr;
		edtp->e_adap = ep->e_adap;
		edtp->e_ctlr = ep->e_ctlr;
		if (ep->v_next) {
			vme = edtp->e_bus_info = (void *) malloc (sizeof(struct vme_intrs));
			vme->v_vintr = (int (*)(int)) ep->v_next->v_vname;
			vme->v_vec = ep->v_next->v_vec;
			vme->v_brl = ep->v_next->v_brl;
			vme->v_unit = ep->v_next->v_unit;
			
		} else
			edtp->e_bus_info = (void *)0;
		edtp->e_init = (int (*)(struct edt *))ep->e_init;
		iosp = &edtp->e_space[0];
		liosp = &ep->e_space[0];
		for (i=0; i<NBASE; i++, iosp++, liosp++) {
			iosp->ios_type = liosp->ios_type;
			iosp->ios_iopaddr = liosp->ios_iopaddr;
			iosp->ios_size = liosp->ios_size;
			/* Only called on live system, so not an issue
			 * with 32-bit lboot cross-lbooting a 64-bit kernel
			 * (64-bit kernels get a 64-bit lboot).
			 */
			iosp->ios_vaddr = (caddr_t)liosp->ios_vaddr;
		}
		if (ep = ep->e_next) {
			mep->medt_next = malloc (sizeof (medt_t));
			mep = mep->medt_next;
		}
	}
	return (medtp);
}

/*
 * prmodule - print information about the module
 */
static void 
prmodule(mod_info_t *mod)
{
	int i;

	printf ("Id: %d\t", mod->m_id);
	switch (mod->m_type) {
	case M_DRIVER:
		if (mod->m_driver.d_type == MDRV_CHAR)
			printf ("Character ");
		else if (mod->m_driver.d_type == MDRV_BLOCK)
			printf ("Block ");
		else if (mod->m_driver.d_type == (MDRV_BLOCK|MDRV_CHAR))
			printf ("Character/Block ");
		else if (mod->m_driver.d_type == MDRV_STREAM)
			printf ("Streams ");
		else
			printf ("Unknown ");

		printf ("device driver: prefix %s, ", mod->m_prefix);
		for (i=0; i<mod->m_driver.d_nmajors; i++)
			printf ("major %d, ", mod->m_driver.d_majors[i]);
		if (mod->m_delay != M_NOUNLD) {
			printf ("unload delay %d minutes, ", mod->m_delay);
		} else
			printf ("no auto-unload, ");
		printf ("filename %s\n", mod->m_fname);
		break;
	case M_STREAMS:
		printf ("Streams module: ");
		printf ("prefix %s, fmodsw name %s, ",
			mod->m_prefix, mod->m_stream.s_name);
		if (mod->m_delay != M_NOUNLD) {
			printf ("unload delay %d minutes, ", mod->m_delay);
		} else
			printf ("no auto-unload, ");
 		printf ("filename %s\n", mod->m_fname);
		break;
	case M_IDBG:
		printf ("Idbg module: filename %s\n", mod->m_fname);
		break;
	case M_LIB:
		printf ("Library module: filename %s\n", mod->m_fname);
		break;
	case M_SYMTAB:
		printf ("Symbol Table module: ");
		if (mod->m_delay != M_NOUNLD) {
			printf ("unload delay %d minutes, ", mod->m_delay);
		} else
			printf ("no auto-unload, ");
		printf ("filename %s\n", mod->m_fname);
		break;
	case M_FILESYS:
		printf ("File system: filename %s \n", mod->m_fname);
		break;
	case M_ENET:
		printf ("Ethernet module: ");
		printf ("prefix %s, filename %s\n",
			mod->m_prefix, mod->m_fname);
		break;
	default:
		printf ("Unknown module type\n");
		break;
	}
}

static char *
ml_err(int error)
{
	switch (error) {

	case	EPERM:
		return ("Must be super-user to use this command");

	case	MERR_ENOENT:
		return ("No such file or directory");

	case	MERR_ENAMETOOLONG:
		return ("Path name too long");
		
	case	MERR_VREG:
		return ("File not a regular file");

	case	EIO:
		return ("I/O error");

	case	ENXIO:
		return ("No such device or address");

	case	MERR_VOP_OPEN:
		return ("VOP_OPEN failed");

	case	MERR_VOP_CLOSE:
		return ("VOP_CLOSE failed");

	case	MERR_COPY:
		return ("Copyin/copyout failed");

	case	MERR_BADMAGIC:
		return ("Object file format not ELF");

	case	MERR_BADFH:
		return ("Error reading file header");

	case	MERR_BADAH: 
		return ("Error reading a.out header");

	case	MERR_BADSH:
		return ("Error reading section header");

	case	MERR_BADTEXT:
		return ("Error reading text section");

	case	EBUSY:
		return ("Device busy");

	case	MERR_BADDATA:
		return ("Error reading data section");

	case	MERR_BADREL:
		return ("Error reading relocation records");

	case	MERR_BADSYMHEAD:
		return ("Error reading symbol header");

	case	MERR_BADEXSTR:
		return ("Error reading external string table");

	case	MERR_BADEXSYM:
		return ("Error reading external symbol table");

	case	MERR_NOSEC:
		return ("Error reading file header sections");

	case	MERR_NOSTRTAB:
		return ("No string table found for object");

	case	MERR_NOSYMS:
		return ("No symbols found for object");

	case	MERR_BADARCH:
		return ("Bad EF_MIPS_ARCH arch type");

	case	MERR_SHSTR:
		return ("Error reading section header string table");

	case	MERR_SYMTAB:
		return ("Error reading section symbol table");

	case	MERR_STRTAB:
		return ("Error reading section string table");

	case	MERR_NOTEXT:
		return ("No text or textrel section found in object");

	case	MERR_SHNDX:
		return ("Bad section index");

	case	MERR_UNKNOWN_CFCMD:
		return ("Unknown syssgi command");

	case	MERR_UNKNOWN_SYMCMD:
		return ("Unknown syssgi sym command");

	case	MERR_FINDADDR:
		return ("Address not found in symbol tables");

	case	MERR_FINDNAME:
		return ("Name not found in symbol tables");

	case	MERR_SYMEFAULT:
		return ("Address fault on sym command");

	case	MERR_NOELF:
		return ("Dynamically loadable ELF modules not supported");

	case	MERR_UNSUPPORTED:
		return ("Feature unsupported");

	case	MERR_LOADOFF:
		return ("Dynamic loading turned off");

	case	MERR_BADID:
		return ("Module id not found");

	case	MERR_NOTLOADED:
		return ("Module not loaded");

	case	MERR_NOTREGED:
		return ("Module not registered");

	case	MERR_OPENBUSY:
		return ("Can not open, device busy");

	case	MERR_UNLOADBUSY:
		return ("Can not unload module, device busy");

	case	MERR_UNREGBUSY:
		return ("Can not unregister module, device busy");

	case	MERR_ILLDRVTYPE:
		return ("Illegal driver type");

	case	MERR_NOMAJOR:
		return ("No major passed in cfg information");

	case	MERR_NOMAJORS:
		return ("Out of major numbers");

	case	MERR_ILLMAJOR:
		return ("Illegal major number");

	case	MERR_MAJORUSED:
		return ("Major number already in use");

	case	MERR_SWTCHFULL:
		return ("Switch table is full");

	case	MERR_UNLDFAIL:
		return ("Unload function failed");

	case	MERR_DOLD:
		return ("D_OLD drivers not supported as loadable modules");

	case	MERR_NODEVFLAG:
		return ("No xxxdevflag found");

	case	MERR_NOINFO:
		return ("No xxxinfo found");

	case	MERR_NOOPEN:
		return ("No xxxopen found");

	case	MERR_NOCLOSE:
		return ("No xxxclose found");

	case	MERR_NOMMAP:
		return ("No xxxmmap found");

	case	MERR_NOSTRAT:
		return ("No xxxstrategy found");

	case	MERR_NOSIZE:
		return ("No xxxsize found");

	case	MERR_NOUNLD:
		return ("No xxxunload found");

	case	MERR_NOVFSOPS:
		return ("No xxxvfsops found");

	case	MERR_NOVNODEOPS:
		return ("No xxxvnodops found");

	case	MERR_NOINIT:
		return ("No xxxinit found");

	case	MERR_NOINTR:
		return ("No xxxintr found");

	case	MERR_NOEDTINIT:
		return ("No xxxedtinit found");

	case	MERR_NOSTRNAME:
		return ("Stream name missing");

	case	MERR_STRDUP:
		return ("Duplicate streams name");

	case	MERR_NOPREFIX:
		return ("No prefix sent in cfg information");

	case	MERR_NOMODTYPE:
		return ("No module type sent in descriptor");

	case	MERR_BADMODTYPE:
		return ("Bad module type sent in descriptor");

	case	MERR_BADCFGVERSION:
		return ("Lboot version out of date with kernel version");

	case	MERR_BADVERSION:
		return ("Module version out of date with kernel version");

	case	MERR_NOVERSION:
		return ("Module version string is missing");

	case	MERR_BADLINK:
		return ("Can't resolve all symbols in object");

	case	MERR_BADJMP:
		return ("Address outside of jal range - use jalr");

	case	MERR_BADRTYPE:
		return ("Bad relocation type");

	case	MERR_GP:
		return ("GP section unsupported - compile with -G0");

	case	MERR_BADSC:
		return ("Unexpected storage class encountered");

	case	MERR_REFHI:
		return ("Unexpected REFHI relocation type");

	case	MERR_NORRECS:
		return ("No relocation records found in object");

	case	MERR_SCNDATA:
		return ("Bad data section encountered in object");

	case	MERR_COMMON:
		return ("Common storage class in object not supported");

	case	MERR_JMP256:
		return ("Can't relocate j in a 256MB boundary on an R4000");

	case	MERR_LIBUNLD:
		return ("Library modules can not be unloaded");

	case	MERR_LIBREG:
		return ("Library modules can not be registered/unregistered");

	case	MERR_NOLIBIDS:
		return ("Out of library module ids");

	case	MERR_IDBG:
		return ("Error loading idbg.o module");

	case	MERR_IDBGREG:
		return ("The idbg.o module can not be registered/unregistered");

	case	MERR_NOENETIDS:
		return ("Out of ethernet module ids");

	case	MERR_ENETREG:
		return ("Ethernet modules can not be registered");

	case	MERR_ENETUNREG:
		return ("Ethernet modules can not be unregistered");

	case	MERR_ENETUNLOAD:
		return ("Ethernet modules can not be unloaded");

	case	MERR_NOFSYSNAME:
		return ("No filesystem name specified");

	case	MERR_DUPFSYS:
		return ("filesystem already exists in vfssw table");

	case	MERR_VECINUSE:
		return ("Interrupt vector in use");

	case	MERR_BADADAP:
		return ("No adapter found");

	case	MERR_NOEDTDATA:
		return ("No edt data for ethernet driver");

	case	MERR_ELF64:
		return ("A 64bit kernel requires a 64bit loadable module");

	case	MERR_ELF32:
		return ("A 32bit kernel requires a 32bit loadable module");

	case	MERR_ELFN32:
		return ("An N32 kernel requires an N32 loadable module");

	case	MERR_ELF64COFF:
		return ("A 64bit kernel requires a 64bit ELF loadable module");

	case	MERR_ET_REL:
		return ("Loadable modules must be built as relocatable");

	case	MERR_SYMTABREG:
		return ("Can not reg/unreg a symbol table module");

	case	MERR_NOSYMTABIDS:
		return ("Out of symbol table module ids");

	case	MERR_NOSYMTAB:
		return ("No symbol table found in object");

	case	MERR_DUPSYMTAB:
		return ("Symbol table for this filename already exists");

	case	MERR_NOSYMTABAVAIL: 
		return ("No runtime symbol table available for running kernel");

	case	MERR_SYMTABMISMATCH:
		return ("Symbol table doesn't not match running kernel");

	default:
		return ("Error unknown");
	}
}
