/* ARCS style hardware inventory/configuration
 */

#ident "$Revision: 1.45 $"

#include <sys/types.h>
#include <saio.h>
#include <arcs/errno.h>
#include <arcs/hinv.h>
#include <arcs/cfgdata.h>
#include <arcs/cfgtree.h>
#include <arcs/spb.h>
#include <standcfg.h>
#include <libsc.h>
#include <libsk.h>
#ifdef SN
#include <sys/SN/klconfig.h>
#endif

struct cfgchain {
	cfgnode_t *cfg;
	struct cfgchain *next;
	void (*install)(COMPONENT*);
};

struct {
	struct cfgchain *head;
	struct cfgchain *tail;
} cfgnext, cfglist;

static void runstandcfg(struct standcfg *, COMPONENT *c);

extern COMPONENT cpu_cfgroot;		/* root data of ARCS config tree */
extern struct standcfg standcfg[];	/* configuration tree template */
static cfgnode_t *cfgroot;

#ifdef SLOTCONFIG
static void drvrinfo_install(struct standcfg *cfg);
static void slotconfig_init(void);
static void slotconfig_done(void);
#endif

/* Initialize the config tree
 */
void
init_cfgtree(void)
{
	struct cfgchain *run;
	
	cfgnext.head = cfgnext.tail = (struct cfgchain *)NULL;

	cfgroot = cpu_makecfgroot();

#ifdef SLOTCONFIG
	slotconfig_init();
#endif

	/* go through standcfg[] and actually install each driver */
	runstandcfg((struct standcfg *)(__psint_t)SCFG_ROOTPARENT,
		    (COMPONENT *)cfgroot);

	/* Now pick up any RegisterInstall() calls from above work */
	do {
		cfglist = cfgnext;
		cfgnext.head = cfgnext.tail = (struct cfgchain *)NULL;
		while (run=cfglist.head) {
			run->install((COMPONENT *)run->cfg);
			cfglist.head = run->next;
			free(run);
		}
	} while(cfgnext.head) ;

#ifdef SLOTCONFIG
	slotconfig_done();		/* clean-up slotconfig memory */
#endif
}

#ifdef SN0
LONG
kl_reg_drv_strat(prom_dev_info_t  *dev_info,
		 STATUS (*dstrat)(klinfo_t  *self, IOBLOCK *iob))
{
	dev_info->driver = dstrat ;
	return (0) ;
}
#endif

LONG
RegisterDriverStrategy(COMPONENT *c, STATUS (*d)(COMPONENT *self, IOBLOCK *iob))
{
	((cfgnode_t *)c)->driver = d;
	return(0);
}

LONG
RegisterInstall(COMPONENT *comp, void (*d)(COMPONENT *))
{
	struct cfgchain *p;

	if (!(p = (struct cfgchain *)malloc(sizeof(*p))))
		panic("cannont malloc space for RegisterInstall node\n");

	p->next = (struct cfgchain *)NULL;
	p->cfg = (cfgnode_t *)comp;
	p->install = d;

	if (!cfgnext.head) {
		cfgnext.head = cfgnext.tail = p;
	}
	else {
		cfgnext.tail->next = p;
		cfgnext.tail = p;
	}
	return(0);
}

COMPONENT *
GetPeer(COMPONENT *c)
{
#ifdef SN0
      panic("GetPeer does not work on SN0.\n") ;
#endif

	return((COMPONENT *)(((cfgnode_t *)c)->peer));
}

COMPONENT *
GetChild(COMPONENT *c)
{
	if (c == (COMPONENT *)NULL)
		return((COMPONENT *)cfgroot);
	return((COMPONENT *)(((cfgnode_t *)c)->child));
}

COMPONENT *
GetParent(COMPONENT *c)
{
#ifdef SN0
      panic("GetParent does not work on SN0.\n") ;
#endif
	return((COMPONENT *)(((cfgnode_t *)c)->parent));
}

LONG
GetConfigurationData(void *p, COMPONENT *c)
{
	cfgnode_t *n = (cfgnode_t *)c;

#ifdef SN0
      panic("GetConfigurationData does not work on SN0.\n") ;
#endif

	if (!c->ConfigurationDataSize || !n->cfgdata)
		return(EINVAL);

	bcopy(n->cfgdata,p,(int)c->ConfigurationDataSize);
	return(ESUCCESS);
}

COMPONENT *
AddChild(COMPONENT *p, COMPONENT *temp, void *cfgdata)
{
	struct cfgdatahdr *cfgd = (struct cfgdatahdr *)cfgdata;
	cfgnode_t *parent = (cfgnode_t *)p;
	cfgnode_t *n;
#if !_K64PROM32
	char *str = "WARNING: AddChild %s(%d) different from system %s(%d).\n";

	char *vers = "version";
	char *rev = "revision";

	if (SPB->Version) {		/* for symmon w/o a PROM */
		if (temp->Version != SPB->Version)
			printf(str,vers,temp->Version,vers,SPB->Version);
		if (temp->Revision != SPB->Revision)
			printf(str,rev,temp->Revision,rev,SPB->Revision);
	}
#endif

#ifdef SN0
      panic("AddChild does not work on SN0.\n") ;
#endif

	n = (cfgnode_t *)malloc(sizeof(cfgnode_t));
	if (n == (cfgnode_t *)NULL)
		return((COMPONENT *)NULL);

	bcopy(temp,n,sizeof(COMPONENT));
	if (temp->IdentifierLength) {
		n->comp.Identifier = (char *)malloc(temp->IdentifierLength+1);
		if (n->comp.Identifier == (char *)NULL)
			goto err;
		strncpy(n->comp.Identifier,temp->Identifier,
			temp->IdentifierLength+1); 
	}
	else
		n->comp.Identifier = (char *)NULL;

	if (n->comp.ConfigurationDataSize && cfgdata) {
		n->cfgdata = (struct cfgdatahdr *)
				malloc(n->comp.ConfigurationDataSize);
		if (n->cfgdata == (void *)NULL)
			goto err;
		bcopy(cfgdata,n->cfgdata,(int)n->comp.ConfigurationDataSize);

		/* Copy strings in standard configuration data header */
		if (cfgd->Type &&
		    !(n->cfgdata->Type = strdup(cfgd->Type)))
			goto err;
		if (cfgd->Vendor &&
		    !(n->cfgdata->Vendor = strdup(cfgd->Vendor)))
			goto err;
		if (cfgd->ProductName &&
		    !(n->cfgdata->ProductName = strdup(cfgd->ProductName)))
			goto err;
		if (cfgd->SerialNumber &&
		    !(n->cfgdata->SerialNumber = strdup(cfgd->SerialNumber)))
			goto err;
	}
	else
		n->cfgdata = (void *)NULL;

	/* set up rest of cfgnode_t */
	n->parent = parent;
	n->peer = (cfgnode_t *)NULL;
	n->child = (cfgnode_t *)NULL;
	n->driver = (STATUS (*)())NULL;

	/* now link up as the last child of the parent */
	if (parent) {
		cfgnode_t **pp;

		for (pp = &parent->child; *pp ;pp = &((*pp)->peer)) ;
		*pp = n;
	}

	return((COMPONENT *)n);
err:
	/* clean-up from a malloc error */
	if (n) {
		free(n->comp.Identifier);
		if (n->cfgdata) {
			free(n->cfgdata->Type);
			free(n->cfgdata->Vendor);
			free(n->cfgdata->ProductName);
			free(n->cfgdata->SerialNumber);
			free(n->cfgdata);
		}
		free(n);
	}
	return((COMPONENT *)NULL);
}

LONG
DeleteComponent(COMPONENT *comp)
{
	cfgnode_t *c = (cfgnode_t *)comp;
	cfgnode_t *p, *b;

	/* check for invalid node, or children */
	if ((c == (cfgnode_t *)NULL) || (c->child != (cfgnode_t *)NULL))
		return(EINVAL);

	p = c->parent;

	/* check for rm of root */
	if (p == (cfgnode_t *)NULL) {
		cfgroot = c->peer;
		return(ESUCCESS);
	}

	b = (cfgnode_t *)NULL;
	p = p->child;
	while(p != c &&  p != (cfgnode_t *)NULL) {
		b = p;
		p = p->peer;
	}
	if (b == (cfgnode_t *)NULL)
		c->parent->child = c->peer;
	else

		b->peer = c->peer;

	free(c->comp.Identifier);
	if (c->cfgdata) {
		free(c->cfgdata->Type);
		free(c->cfgdata->Vendor);
		free(c->cfgdata->ProductName);
		free(c->cfgdata->SerialNumber);
		free(c->cfgdata);
	}
	free(c);

	return(ESUCCESS);
}

LONG
SaveConfiguration(void)
{
	return(cpu_savecfg());
}

/*  Initialize next level with parent at level (pl == parent level).  Parent
 * (p) must have no children at this point.
 */
static void
runstandcfg(struct standcfg *pl, COMPONENT *p)
{
	COMPONENT *c = 0, *prev = 0, *next;
	struct standcfg *cl = standcfg;

	while (cl->install) {
		/* install our current level if not handled by drvrinfo */
		if (cl->parent == pl) {
#ifdef SLOTCONFIG
			if (cl->drvrinfo)
				drvrinfo_install(cl);
			else
#endif
			cl->install(p,0);
			next = c ? GetPeer(c) : GetChild(p);

			/*  Look for more entries in standcfg for all new
			 * children.
			 */
			while (next) {
				runstandcfg(cl,next);
				next = GetPeer(prev=next);
			}
			c = prev;
		}
		cl++;
	}
	return;
}

#ifdef SLOTCONFIG
/*
 * SLOTCONFIG provides card/slot id based matching in standalone, integrated
 * with the older driver install based probing methods.  Bus providers call
 * slot_register() to register a slot and the config code will match this
 * against the data provided by standcfg.
 *
 * This code is an slightly better integrated version of Greg Limes'
 * slot_drvr, which is derived from the original pci_intf.
 *
 * .cf ORDERING: Some of this code requires the .cf file to give the
 * propper driver parent dependancies.
 *
 * CALL ORDERING: If multiple drivers are interested in a device, the
 * various drivers' install routines will be called with that device
 * in the order in which the drivers are in the .cf file.
 */

#define	DPRINTF(x)	/* printf x */

/*
 * slotinfo_list_t: trivial linked list of slotinfo_t nodes.
 */
typedef struct slotinfo_list_s	slotinfo_list_t;
struct slotinfo_list_s {
    slotinfo_list_t    *next;
    slotinfo_t	       *slot;
    int			flags;
#define SLOT_CLAIMED	0x01
};

/*
 * slotdrvr_ready: we are between init() and done() calls.
 */
static int slotdrvr_ready;

/*
 * linked list headers
 *
 * To maintain ordering of rendevous calls, I manage my linked lists
 * as FIFOs. The FIFO insertion and deletion code gets simpler if,
 * instead of keeping a pointer to the "last node", you keep a pointer
 * to the last "node pointer".
 */
static slotinfo_list_t *slots, **last_slotp;

/*
 * id_match: compare the ID values for a device and a driver; return
 * true if all the fields are the same, allowing the driver to specify
 * wildcard values. The revision compare returns true if the device's
 * revision number is greater than or equal to the revision number
 * that the driver supports.
 */
static int
id_match(slotinfo_t *slot, drvrinfo_t *drvr, int *np)
{
	slotdrvr_id_t      *sid = &slot->id;
	slotdrvr_id_t	   *did = drvr->id;
	int n = 0;

	while (did->bus != BUS_NONE) {
		DPRINTF(("id_match: compare drvr %x.%x.%x.%x",
			did->bus, did->mfgr, did->part, did->rev));
		DPRINTF(("vs slot %x.%x.%x.%x\n",
			sid->bus, sid->mfgr, sid->part, sid->rev));

		if (((did-> bus == sid-> bus) || (did-> bus ==  BUS_ANY)) &&
		    ((did->mfgr == sid->mfgr) || (did->mfgr == MFGR_ANY)) &&
		    ((did->part == sid->part) || (did->part == PART_ANY)) &&
		    ((did-> rev <= sid-> rev) || (did-> rev ==  REV_ANY))) {
			*np = n;	
			return 1;
		}

		did++;
		n++;
	}

	return 0;
}

/*
 * slotconfig_init: initialize rendevous package.
 *
 * This routine must be called before the package is used. It is not
 * automatically called by the register routines to prevent us
 * attempting to use the package before we can reasonably call
 * "malloc" and friends (calling slot_register during early init will
 * cause trouble).
 */
static void
slotconfig_init(void)
{
	DPRINTF(("slotdrvr_init\n"));
	last_slotp = &slots;
	slotdrvr_ready = 1;
}

/*
 * slot_register: register a device found.
 *
 */
void
slot_register(COMPONENT *parent, slotinfo_t *slot)
{
	slotinfo_list_t    *snode;
	slotinfo_t         *slotp;

	DPRINTF(("slot_register %d.%lX.%lX.%d ...\n",
		slot->id.bus,
		(unsigned long)slot->id.mfgr,
		(unsigned long)slot->id.part,
		(unsigned long)slot->id.rev));

	if (!slotdrvr_ready) {
		panic("slot_register called illegally!\n");
		/*NOTREACHED*/
	}

	slotp = (slotinfo_t *)malloc(sizeof *slotp);
	snode = (slotinfo_list_t *)malloc(sizeof *snode);
	if (!snode || !slotp) panic("slot_register: out of memory");
	bcopy(slot,slotp,sizeof *slotp);
	slotp->parent = parent;
	snode->slot = slotp;
	snode->flags = 0;
	snode->next = 0;

	*last_slotp = snode;
	last_slotp = &snode->next;
}

/*
 * drvrinfo_install: check for this driver against slot list.
 *
 * The list of devices is scanned, and the driver's install
 * routine is called for each device that matches the ID
 * information given.
 */
static void
drvrinfo_install(struct standcfg *cfg)
{
    slotinfo_list_t    *snode;
    slotinfo_t	       *slot;
    drvrinfo_t	       *drvr = cfg->drvrinfo;
    int			n;

#ifdef DEBUG
    DPRINTF(("drvrinfo_install "));
    if (drvr->id->bus == BUS_ANY)
	DPRINTF(("*"));
    else
	DPRINTF(("%d", drvr->id->bus));
    if (drvr->id->mfgr == MFGR_ANY)
	DPRINTF((".*"));
    else
	DPRINTF((".%lX", drvr->id->mfgr));
    if (drvr->id->part == PART_ANY)
	DPRINTF((".*"));
    else
	DPRINTF((".%lX", drvr->id->part));
    if (drvr->id->rev == REV_ANY)
	DPRINTF((".*"));
    else
	DPRINTF((".%d", drvr->id->rev));
    if (drvr->name)
	DPRINTF((" \"%s\"", drvr->name));
    DPRINTF((" ...\n"));
#endif

    for (snode = slots; snode; snode = snode->next) {
	if ((snode->flags & SLOT_CLAIMED) == 0) {
	    slot = snode->slot;
	    DPRINTF(("drvrinfo_install: slot=%X\n", slot));
	    if (id_match(slot, drvr, &n)) {
	        DPRINTF(("drvrinfo_install: match!\n"));
		slot->drvr_index = n;
		cfg->install(slot->parent,slot);
		snode->flags |= SLOT_CLAIMED;
	    }
	}
    }
    DPRINTF(("drvrinfo_install: scan done\n"));
}

/*
 * nulldriver_install: "helper" that bitches about matches.
 *
 * This is the sponge that consumes any remaining devices after
 * all the rendevous calls have been made.
 */
static int
nulldriver_install(COMPONENT *p, slotinfo_t *slot)
{
	char		pbuf[128];	/* none should be longer than 1 line */
	static int	nnulldevs;
	COMPONENT	tmpl;

	DPRINTF(("nulldriver_rendevous %d.%lX.%lX.%d\n",
		slot->id.bus,
		0xFFFFul & (unsigned long)slot->id.mfgr,
		0xFFFFul & (unsigned long)slot->id.part,
		(unsigned long)slot->id.rev));

	tmpl.Class = PeripheralClass;
	tmpl.Type = OtherPeripheral;
	tmpl.Flags = 0;
	tmpl.Version = SGI_ARCS_VERS;
	tmpl.Revision = SGI_ARCS_REV;
	tmpl.AffinityMask = 0;
	tmpl.ConfigurationDataSize = 0;

	switch (slot->id.bus) {
	case BUS_XTALK:
		sprintf(pbuf, "port %d part 0x%x mfgr 0x%x rev %d",
			slot->bus_slot,
			slot->id.part,
			slot->id.mfgr,
			slot->id.rev);
		break;

	case BUS_PCI:
		sprintf(pbuf, "slot %d vendor 0x%x part 0x%x rev %d",
			slot->bus_slot,
			slot->id.mfgr,
			slot->id.part,
			slot->id.rev);
		break;

	case BUS_GIO:
		sprintf(pbuf, "slot %d",
			slot->bus_slot);
		break;

	case BUS_IOC3:
		/* some things do not include all IOC3 sub-drivers */
		return 0;

	default:
		sprintf(pbuf, "bustype %d slot %d part 0x%x mfgr 0x%x rev %d",
			slot->id.bus,
			slot->bus_slot,
			slot->id.mfgr,
			slot->id.part,
			slot->id.rev);
	}

	tmpl.IdentifierLength = strlen(pbuf);
	tmpl.Identifier = pbuf;
	tmpl.Key = nnulldevs++;

	/* add us. don't need our pointer back. */
	(void)AddChild(p, &tmpl, 0);

	return 0;
}

/*
 * slotdrvr_done: shut down the rendevous mechanism.
 *
 * A small amount of dynamic memory is used for each unclaimed device
 * and each device driver. Calling slotdrvr_done triggers complaints
 * about any remaining unclaimed devices, then releases all dynamic
 * resources being used by the slotdrvr package.
 */
void
slotconfig_done(void)
{
	slotinfo_list_t	   *snode;

	DPRINTF(("slotdrvr_done...\n"));

	slotdrvr_ready = 0;

	while (snode = slots) {
		slots = snode->next;
		if ((snode->flags & SLOT_CLAIMED) == 0)
			nulldriver_install(snode->slot->parent,snode->slot);
		free(snode->slot);
		free(snode);
	}

	DPRINTF(("slotdrvr_done complete\n"));
}
#endif /* SLOTCONFIG */
