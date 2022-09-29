/**************************************************************************
 *                                                                        *
 *                  Copyright (C) 1995 Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *    cdl: Connection and Driver List
 *
 *      This module facilitates tracking a list of
 *      identification numbers, device drivers who wish
 *      to provide service for those numbers, and
 *      connection points that have presented those
 *      numbers.
 *
 *      The code comprising this module is derived from
 *      the registry code previously instantiated
 *      directly in the xtalk, pciio and gbrio modules.
 *
 *      Entry points are available for constructing the
 *      empty list, adding a driver to the list, adding
 *      a connection point to the list, removing a
 *      driver from the list, and removing a connection
 *      point from the list.
 *
 *      When a device driver is added, its attach
 *      routine will be called with all connection
 *      points currently registered at the matching ID,
 *      in the order they were added to the registry.
 *
 *      When a connection point is added, the attach
 *      routines of all the currently registered device
 *      drivers matching that ID are called, in the
 *      order they were added to the registry.
 *
 *      When a device driver is deleted, its detach
 *      routine will be called with all connection
 *      points currently registered at the matching ID.
 *
 *      When a connection point is deleted, the detach
 *      routines for all drivers at the matching ID will
 *      be called.
 *
 *      When the registry itself is deleted, all dynamic
 *      resources in the registry are released back to
 *      the system; HOWEVER, we do not go through and
 *      make detach() calls as if every driver and
 *      connection point were being deleted.
 *
 * INITIALIZATION ORDERING NOTE:
 *
 *      The system does not guarantee any particular
 *      ordering of the calls to the driver init
 *      routines. This complicates our callers a bit,
 *      and when we consider the possibility of making
 *      the init process parallel, we end up possibly
 *      trying to allocate the same CDL twice. Our
 *      callers will handle detection and destroy all
 *      but one of them; this code needs to be proof
 *      against such reallocations.
 *
 *      Multiple callers attempting to register at the
 *      same CDL entry point at the same time is handled
 *      efficiently by the current algorithm.
 */

#ident "$Revision: 1.37 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/sema.h>
#include <sys/debug.h>
#include <sys/driver.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/ioerror.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/runq.h>
#include <sys/conf.h>
#include <ksys/sthread.h>
#include <sys/cdl.h>

#define	NEW(ptr)	((ptr) = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define	NEWA(ptr,n)	((ptr) = kmem_zalloc((n)*sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free((ptr), sizeof (*(ptr))))
#define	DELA(ptr,n)	(kmem_free((ptr), (n)*sizeof (*(ptr))))

#if HWG_PERF_CHECK && IP30 && !DEBUG
#include <sys/RACER/heart.h>
static heart_piu_t     *heart_piu = HEART_PIU_K1PTR;
#define	RAW_COUNT()	(heart_piu->h_count)
#endif

typedef struct ent     *ent_p;

struct cdl {
    char                   *name;
    char                   *k1str;
    char                   *k2str;
    vertex_hdl_t	    bvtx;
    ent_p                   list;
};

struct ent {
    ent_p                   next;

    int                     key1;
    int                     key2;

    lock_t                  lock[1];
    int                     state;
    sv_t                    wait[1];
    int			    waiters;

    vertex_hdl_t            uvtx;
    vertex_hdl_t	    ivtx;

    mutex_t                 drivers_lock[1];
    int                     ndrivers_hi;
    int                     ndrivers;
    device_driver_t        *drivers;
    int                     driversz;

    mutex_t                 connpts_lock[1];
    int                     nconnpts;
    vertex_hdl_t           *connpts;
    int                     connptsz;
};

/* =====================================================================
 *            Function Table of Contents
 */

void			cdl_init(void);
static ent_p            cdl_ent(cdl_p, int, int);
static int              cdl_call_attach(device_driver_t, vertex_hdl_t);
static int              cdl_call_detach(device_driver_t, vertex_hdl_t);
static void		cdl_attach_thread(struct cdevsw *, vertex_hdl_t, async_attach_t);

static void             cdl_lock_alfa(ent_p);
static void             cdl_lock_beta(ent_p);
static void             cdl_lock_done(ent_p);

cdl_p                   cdl_new(char *, char *, char *);
void                    cdl_del(cdl_p);
int                     cdl_add_driver(cdl_p, int, int, char *, int);
void                    cdl_del_driver(cdl_p, char *);
int                     cdl_add_connpt(cdl_p, int, int, vertex_hdl_t);
void                    cdl_del_connpt(cdl_p, int, int, vertex_hdl_t);

/* =====================================================================
 *            Private Functions
 */

static mutex_t	async_attach_del_lock;

void
cdl_init(void) 
{
	mutex_init(&async_attach_del_lock, MUTEX_DEFAULT, "async_attach_del");
}

/*
 * cdl_ent: Find (or Add) an ENT to a CDL
 *
 * Locates the unique entry in this connection driver
 * list for the given ID information; if no such entry
 * exists, one is created and added. NOTE: this code is
 * carefully written to be efficiently lockless while
 * preventing duplicate entries from appearing.
 */
static ent_p
cdl_ent(cdl_p reg, int key1, int key2)
{
    ent_p                   newe;
    ent_p                   scan;
    ent_p                  *next;

    newe = NULL;
    next = &reg->list;

    while (1) {
	/*
	 * First time through we start at the front
	 * of the list, and scan until we match or
	 * find end-of-list. Subsequent times
	 * through, we continue where we left off.
	 * NOTE: if we have allocated a new entry
	 * and match an old one, we release the
	 * one we allocated back to the free pool.
	 */
	while ((scan = *next) != NULL) {
	    if ((scan->key1 == key1) && (scan->key2 == key2)) {
		if (newe != NULL) {
	    	    sv_destroy(newe->wait);
	    	    spinlock_destroy(newe->lock);
	    	    mutex_destroy(newe->drivers_lock);
	    	    mutex_destroy(newe->connpts_lock);
		    DEL(newe);
		}
		return scan;		/* FOUND */
	    }
	    next = &scan->next;
	}
	/*
	 * If we could not match the key info,
	 * allocate a new entry and construct
	 * the "unknown" heirarchy of the graph
	 * for this kind of device.
	 * NOTE: if we already allocated an entry,
	 * we can skip this.
	 * NOTE: don't add "unknown" if key2 is "-1"
	 */
	if (newe == NULL) {
	    NEW(newe);
	    if (newe == NULL)
		return NULL;		/* ERROR: allocation failed */
	    newe->next = NULL;
	    newe->key1 = key1;
	    newe->key2 = key2;
	    newe->uvtx = GRAPH_VERTEX_NONE;
	    newe->ivtx = GRAPH_VERTEX_NONE;
	    newe->waiters = 0;
	    spinlock_init(newe->lock, "cdlent");
	    mutex_init(newe->drivers_lock, MUTEX_DEFAULT, "cdldrv");
	    mutex_init(newe->connpts_lock, MUTEX_DEFAULT, "cdlcon");
	    sv_init(newe->wait, SV_FIFO, "cdlwait");
	}
	/*
	 * Attempt to add our new entry to the list;
	 * succeeds if the final "next" pointer is
	 * still NULL. If it fails, we go back and scan
	 * the new entry (or entries) on the list.
	 */
	if (compare_and_swap_ptr((void **) next, NULL, newe))
	    return newe;
    }
}

extern cpu_cookie_t setnoderun(cnodeid_t);
extern void restorenoderun(cpu_cookie_t);		

static int
cdl_call_attach(device_driver_t driver,
		vertex_hdl_t	vhdl)
{
    int                     retval;
    struct cdevsw          *my_cdevsw;
    struct bdevsw          *my_bdevsw;
    cpu_cookie_t	    c;	
    async_attach_t          aa = NULL;
    extern int		    cdl_attach_pri;
    extern int		    io_init_async;

#if HWG_PERF_CHECK && IP30 && !DEBUG
    unsigned long	    cval;
    extern void		    hwg_hprint(unsigned long, char *);

#define	NEED_NAME
#elif DEBUG && ATTACH_DEBUG
#define	NEED_NAME
#endif

#ifdef NEED_NAME
    char                    nbuf[40];

    nbuf[0] = 0;
    device_driver_name_get(driver, nbuf, sizeof nbuf - 10);
    strcat(nbuf, "attach()");
#endif
    /* Propagate the driver priority to the device 
     * if device priority is not specified thru a device admin
     */
    if (!device_admin_info_get(vhdl,ADMIN_LBL_INTR_SWLEVEL)) {
	    ilvl_t	    	driver_pri;
	    device_desc_t	dev_desc;
    
	    /* Check if there is a driver priority specified
	     * thru the driver master file hints.
	     */
	    driver_pri = device_driver_thread_pri_get(driver);
	    if (driver_pri != DRIVER_THREAD_PRI_INVALID) {
		    /* We have a valid driver priority & no device
		     * priority . 
		     */

		    /* Get a copy of the device descriptor */
		    dev_desc = device_desc_dup(vhdl);

		    /* Propagate the driver priority to  the device */
		    device_desc_intr_swlevel_set(dev_desc,driver_pri);

		    /* Update the device descriptor */
		    device_desc_default_set(vhdl,dev_desc);
	    }
    }
#if DEBUG && ATTACH_DEBUG
    cmn_err(CE_CONT, "%v cdl_call_attach calling %s\n", vhdl, nbuf);
#endif

    device_driver_devsw_get(driver, &my_bdevsw, &my_cdevsw);

    /* It is a good practice to pair up setnoderun with restorenoderun
     * similar to the way setmustrun & restoremustrun are used 
     */
    c = setnoderun(master_node_get(vhdl));

#if HWG_PERF_CHECK && IP30 && !DEBUG
    cval = RAW_COUNT();
#endif

    aa = async_attach_get_info(vhdl);

    if (io_init_async && aa && (*my_cdevsw->d_flags & D_ASYNC_ATTACH)) {
	    char thread_name[16];

	    sprintf(thread_name, "cdlattach%d", vhdl);
	    sthread_create(thread_name, 0, 0, 0, cdl_attach_pri, KT_PS, 
			   (st_func_t *)cdl_attach_thread, 
			   (void *)my_cdevsw, (void *)(__psunsigned_t)vhdl, (void *)aa, NULL);
	    async_attach_signal_start(aa);
	    retval = 0;
    }
    else {
#ifdef AA_DEBUG
	    char pathname[256];
	    int  start = lbolt;
	    int  stop;

	    sprintf(pathname, "%v", vhdl);
	    cmn_err(CE_DEBUG, "sync attach starting %d: %s\n", lbolt, pathname);
#endif

	    retval = cdattach(my_cdevsw, vhdl);

#ifdef AA_DEBUG
	    stop = lbolt;
	    sprintf(pathname, "%v", vhdl);
	    cmn_err(CE_DEBUG, "sync attach done %d-%d=%d mS: %s\n", 
		    stop, start, (stop-start)*10, pathname);
#endif
	    if (aa)
		    async_attach_del_info(vhdl);
    }

#if HWG_PERF_CHECK && IP30 && !DEBUG
    /* RAW_COUNT wraps every 2^52*80ns,
     * which is about 11 years.
     */
    cval = RAW_COUNT() - cval;
    hwg_hprint(cval, nbuf);
#endif

    restorenoderun(c);
    return retval;
}

static int
cdl_call_detach(device_driver_t driver,
		vertex_hdl_t vhdl)
{
    int                     retval;
    struct cdevsw          *my_cdevsw;
    struct bdevsw          *my_bdevsw;
    cpu_cookie_t	   c;
	
    device_driver_devsw_get(driver, &my_bdevsw, &my_cdevsw);
    
    /* It is a good practice to pair up setnoderun with restorenoderun
     * similar to the way setmustrun & restoremustrun are used 
     */
    c = setnoderun(master_node_get(vhdl));

    retval = cddetach(my_cdevsw, vhdl);

    restorenoderun(c);

    return retval;
}

void
cdl_attach_thread(struct cdevsw		*my_cdevsw,
		  vertex_hdl_t		vhdl,
		  async_attach_t	aa)
{
#ifdef AA_DEBUG
	char pathname[256];
	int  start = lbolt;
	int  stop;

	sprintf(pathname, "%v", vhdl);
	cmn_err(CE_DEBUG, "cdl_attach_thread starting %d: %s\n", lbolt, pathname);
#endif

	cdattach(my_cdevsw, vhdl);

#ifdef AA_DEBUG
	stop = lbolt;
	sprintf(pathname, "%v", vhdl);
	cmn_err(CE_DEBUG, "cdl_attach_thread done %d-%d=%d S: %s\n", 
		stop, start, (stop-start)/100, pathname);
#endif

	async_attach_signal_done(aa);
	async_attach_del_info(vhdl);
	sthread_exit();
}

/* =====================================================================
 *          Data Locking
 *
 *	The locking on the CDL entries is organized to
 *	allow multiple callers through the
 *	cdl_add_drivers interface, or multiple callers
 *	through the cdl_add_connpts interface, but not
 *	at the same time. Additionally, the actual act
 *	of appending a driver or a connection point is
 *	protected with an appropriate mutex.
 *
 *	The idea is to make sure that when we add a
 *	connection point, we attach it to all the
 *	drivers that were already there; and when we add
 *	a driver, we attach it to all the connection
 *	points that are already there.
 *
 *	Generally, the system flows through this module
 *	in several discrete waves. First, during
 *	initialization, drivers register themselves;
 *	there are no connection points, so the only
 *	locking occurring is the mutex that makes the
 *	driver appending to the entry atomic. When
 *	drivers are being added, the ALFA side of the
 *	lock is active.
 *
 *	(The process of finding or inserting a new entry
 *	in the CDL is done using lockless operations;
 *	this is simple enough, since an entry in this
 *	list may become empty but will never be
 *	removed.)
 *
 *	Once all the drivers present in the kernel have
 *	registered themselves, we start building up the
 *	hardware graph and calling attach routines. On
 *	large systems, each node handles its own
 *	devices, so we could have many callers entering
 *	cdl_add_connpt for the same kind of device (say,
 *	an XIO-PCI Bridge ASIC based thing) from all
 *	over the system. During these calls, the BETA
 *	side of the lock is used, locking out any
 *	potential ALFA side callers who would want to
 *	freeze the connection point list and change the
 *	driver list. However, each of these callers once
 *	sequenced through the (normally) rapid process
 *	of appending to the connection point list, can
 *	call their attach routines in parallel.
 *
 *	Some time later, we have to start dealing with
 *	loadable device drivers, which may register and
 *	unregister freely. As the driver is registered,
 *	it holds the ALFA side of the lock on the entry;
 *	if the driver is a loadable driver for a bridge
 *	device of some sort, new connection points for
 *	the client bus might be added -- and this of
 *	course would use the BETA side of the lock,
 *	handling the case where we are loading a number
 *	of drivers, possibly including drivers for
 *	devices that are only now being discovered by
 *	the newly loaded bridge code.
 *
 *	The "del" entry points use the same locks as the
 *	corresponding "add" entry points, to maintain
 *	consistancy of the lists as we append and erase
 *	various entries.
 *
 *	In practice, there is little if any contention
 *	on the ALFA versus BETA halves of the lock; and
 *	while the operation of scanning or appending to
 *	the arrays is within a lock, these lists are
 *	typically short and the operations fast. We may
 *	want to revisit this for multi-kiloprocessor
 *	configurations where connection point list
 *	management might benefit from being distributed
 *	across the nodes, rather than being centralized
 *	in whatever node last reallocated the list.
 *
 *	Most "driver list" arrays will contain one
 *	entry; some may contain two. One entry in
 *	particular, corresponding to the XIO-PCI/GIO
 *	Bridge ASIC, may eventually contain a dozen as
 *	more XIO cards are built using this frontend.
 *
 *	Connection point lists will be sized depending
 *	on how much the customer spends on peripherals;
 *	if he attaches two hundred PCI QL cards, then
 *	the entry for PCI QL will have that many
 *	connection points.
 *
 *	Delete operations are not considered time critical.
 */

static void
cdl_lock_alfa(ent_p ent)
{
    unsigned                s;

    s = mutex_spinlock(ent->lock);
    while (ent->state < 0) {
#if CDL_DEBUG
	printf("!cdl_lock_alfa: wait at 0x%x\n", ent->wait);
#endif
	ent->waiters ++;
	sv_wait(ent->wait, PZERO, ent->lock, s);
	s = mutex_spinlock(ent->lock);
    }
    ent->state++;
#if CDL_DEBUG
    printf("!cdl_lock_alfa: done, state %d\n", ent->state);
#endif
    mutex_spinunlock(ent->lock, s);
}

static void
cdl_lock_beta(ent_p ent)
{
    unsigned                s;

    s = mutex_spinlock(ent->lock);
    while (ent->state > 0) {
#if CDL_DEBUG
	printf("!cdl_lock_beta: wait at 0x%x\n", ent->wait);
#endif
	ent->waiters ++;
	sv_wait(ent->wait, PZERO, ent->lock, s);
	s = mutex_spinlock(ent->lock);
    }
    ent->state--;
#if CDL_DEBUG
    printf("!cdl_lock_beta: done, state %d\n", ent->state);
#endif
    mutex_spinunlock(ent->lock, s);
}

static void
cdl_lock_done(ent_p ent)
{
    unsigned                s;

    s = mutex_spinlock(ent->lock);
    if (ent->state > 0)
	ent->state--;
    if (ent->state < 0)
	ent->state++;
    if (ent->waiters && !ent->state) {
#if CDL_DEBUG
	printf("!cdl_lock_done: wake %d on 0x%x\n", ent->waiters, ent->wait);
#endif
	sv_broadcast(ent->wait);
    }
#if CDL_DEBUG
    printf("!cdl_lock_done: done, state %d\n", ent->state);
#endif
    mutex_spinunlock(ent->lock, s);
}

/* =====================================================================
 *            Public Functions
 */

/*
 * cdl_new: create a new CDL
 * with the specified list name
 * and key field names.
 */
cdl_p
cdl_new(char *name, char *k1str, char *k2str)
{
    cdl_p                   reg;

    NEW(reg);
    reg->name = name;
    reg->k1str = k1str;
    reg->k2str = k2str;
    reg->bvtx = GRAPH_VERTEX_NONE;
    return reg;
}

/*
 * cdl_del: tear down a CDL
 * releasing all dynamic resources
 * back to the system.
 * NOTE: does not call detach
 * routines.
 */
void
cdl_del(cdl_p reg)
{
    if (reg != NULL) {
	ent_p                   ent, next;

	next = reg->list;
	while ((ent = next) != NULL) {
	    next = ent->next;
	    sv_destroy(ent->wait);
	    spinlock_destroy(ent->lock);
	    mutex_destroy(ent->drivers_lock);
	    mutex_destroy(ent->connpts_lock);
	    if (ent->driversz > 0)
		DELA(ent->drivers, ent->driversz);
	    if (ent->connptsz > 0)
		DELA(ent->connpts, ent->connptsz);
	    DEL(ent);
	}
	DEL(reg);
    }
}

/*
 * cdl_add_driver: add a driver to the registry,
 * and call its attach routine for each matching
 * connection point. returns -1 if there was a
 * problem, or 0 if all went well.
 */
int
cdl_add_driver(cdl_p reg, int key1, int key2, char *prefix, int flags)
{
    device_driver_t         driver;
    ent_p                   ent;

    if (prefix == NULL)
	return -1;

    driver = device_driver_get(prefix);
    if (driver == DEVICE_DRIVER_NONE) {
	printf("cdl_add_driver: unable to find driver '%s'\n",
	       prefix);
	return -1;
    }
    ent = cdl_ent(reg, key1, key2);
    if (ent == NULL)
	return -1;			/* allocation failure */

    cdl_lock_alfa(ent);
    mutex_lock(ent->drivers_lock, PZERO);

    /* Step 1: add the driver to the driver list.
     */
    {
	int                     lim;
	int                     wix;
	device_driver_t        *dl;

	lim = ent->driversz;
	wix = ent->ndrivers;
	dl = ent->drivers;
	if (wix >= lim) {
	    int                     nlim = lim * 2 + 1;
	    device_driver_t        *ndl;
	    device_driver_t         dd;
	    int                     rlim;
	    int                     rix;

	    NEWA(ndl, nlim);
	    rlim = wix;
	    rix = 0;
	    wix = 0;
	    if (dl != NULL)
		for (rix = 0; rix < rlim; ++rix)
		    if ((dd = dl[rix]) != 0)
			ndl[wix++] = dd;
	    ent->drivers = ndl;
	    ent->driversz = nlim;
	    if (dl != NULL)
		DELA(dl, lim);
	    dl = ndl;
	}
	ent->ndrivers = wix + 1;
	if (flags & CDL_PRI_HI) {
	    int                     hix;

	    hix = ent->ndrivers_hi;
	    while (wix > hix) {
		wix--;
		dl[wix + 1] = dl[wix];
	    }
	    ent->ndrivers_hi = wix + 1;
	}
	dl[wix] = driver;
    }

    mutex_unlock(ent->drivers_lock);

    /* Step 2: scan the connection point list,
     * and fire off the attach calls.
     *
     * XXX- someday we ought to put each call
     * in its own thread; when we do, don't
     * forget to synchronize on all of them
     * finishing before we cdl_lock_done().
     */

    {
	int                     hwm;
	vertex_hdl_t           *cl;
	int                     ix;

	hwm = ent->nconnpts;
	cl = ent->connpts;
	for (ix = 0; ix < hwm; ++ix)
	    if (cl[ix] != GRAPH_VERTEX_NONE)
		if (cdl_call_attach(driver, cl[ix]) == 0) {
		    char                    edge[32];

		    sprintf(edge, "%u", cl[ix]);
		    if (ent->uvtx != GRAPH_VERTEX_NONE)
			hwgraph_edge_remove(ent->uvtx, edge, 0);
		}
    }

    cdl_lock_done(ent);

    return 0;
}

/*
 * cdl_del_driver: remove a driver from the registry,
 * and call its detach routine for each matching
 * connection point.
 */
void
cdl_del_driver(cdl_p reg,
	       char *prefix)
{
    device_driver_t         driver;
    ent_p                   ent;
    int                     found;

    if (prefix == NULL)
	return;

    driver = device_driver_get(prefix);
    if (driver == DEVICE_DRIVER_NONE) {
	printf("cdl_del_driver: unable to find driver '%s'\n",
	       prefix);
	return;
    }
    for (ent = reg->list; ent != NULL; ent = ent->next) {

	cdl_lock_alfa(ent);
	mutex_lock(ent->drivers_lock, PZERO);

	{
	    int                     hwm;
	    device_driver_t        *dl;
	    int                     ix;

	    hwm = ent->ndrivers;
	    dl = ent->drivers;
	    found = 0;
	    for (ix = 0; ix < hwm; ++ix)
		if (dl[ix] == driver) {
		    dl[ix] = 0;
		    found++;
		}
	}

	mutex_unlock(ent->drivers_lock);

	if (found) {
	    int                     hwm;
	    vertex_hdl_t           *cl;
	    int                     ix;

	    hwm = ent->nconnpts;
	    cl = ent->connpts;
	    while (found-- > 0)
		for (ix = 0; ix < hwm; ++ix)
		    if (cl[ix] != GRAPH_VERTEX_NONE)
			cdl_call_detach(driver, cl[ix]);
	}

	cdl_lock_done(ent);
    }
}

/*
 * cdl_add_connpt: add a connection point to the
 * registry, and call the attach routine for each
 * matching driver. Returns zero if all went well,
 * or -1 if there was a problem.
 *
 * If all went well, we also attempt to create
 * entry listings in support of "-1" wildcarding
 * on either key (or both).
 */
int
cdl_add_connpt(cdl_p reg, int key1, int key2, 
	       vertex_hdl_t connpt)
{
    ent_p                   ent;
    vertex_hdl_t	    ivtx;

    ent = cdl_ent(reg, key1, key2);
    if (ent == NULL)
	return -1;			/* allocation failure */

    hwgraph_vertex_ref(connpt);

    cdl_lock_beta(ent);
    mutex_lock(ent->connpts_lock, PZERO);

    {
	int                     lim;
	int                     hwm;
	vertex_hdl_t           *cpl;

	lim = ent->connptsz;
	hwm = ent->nconnpts;
	cpl = ent->connpts;
	if (hwm < lim) {
	    cpl[hwm++] = connpt;
	    ent->nconnpts = hwm;
	} else {
	    int                     nlim = lim * 2 + 1;
	    vertex_hdl_t           *ncpl;
	    vertex_hdl_t            cp;
	    int                     rix;
	    int                     wix;

	    NEWA(ncpl, nlim);
	    wix = 0;
	    if (cpl != NULL)
		for (rix = 0; rix < hwm; ++rix)
		    if ((cp = cpl[rix]) != GRAPH_VERTEX_NONE)
			ncpl[wix++] = cp;
	    ncpl[wix++] = connpt;
	    ent->connpts = ncpl;
	    ent->nconnpts = wix;
	    ent->connptsz = nlim;
#if GRAPH_VERTEX_NONE
	    while (wix < nlim)
		ncpl[wix++] = GRAPH_VERTEX_NONE;
#endif
	    if (cpl != NULL)
		DELA(cpl, lim);
	}
    }
    ivtx = ent->ivtx;
    if ((GRAPH_VERTEX_NONE == ivtx) &&
	(key1 != -1) && (key2 != -1)) {
	vertex_hdl_t	bvtx;
	char		iname[16];
	int		v;
	int		i;
	char	       *np;

	bvtx = reg->bvtx;
	if (GRAPH_VERTEX_NONE == bvtx) {
	    vertex_hdl_t	tvtx;
	    hwgraph_path_add(hwgraph_root, ".id", &tvtx);
	    if (GRAPH_VERTEX_NONE != tvtx)
		hwgraph_path_add(tvtx, reg->name, &bvtx);
	    reg->bvtx = bvtx;
	}

	np = iname + sizeof iname;
	*--np = 0;

	v = key2;
	for (i=0; i<4; ++i) {
	    *--np = "0123456789ABCDEF"[v&15];
	    v >>= 4;
	}

	v = key1;
	for (i=0; i<4; ++i) {
	    *--np = "0123456789ABCDEF"[v&15];
	    v >>= 4;
	}

	if (GRAPH_VERTEX_NONE != bvtx)
	    hwgraph_path_add(bvtx, np, &ivtx);
	ent->ivtx = ivtx;
    }

    mutex_unlock(ent->connpts_lock);

    if (GRAPH_VERTEX_NONE != ivtx) {
	char		iname[32];
	unsigned	v;
	char	       *np;

	np = iname + sizeof iname;
	*--np = 0;
	v = connpt;
	do {
	    *--np = '0' + (v%10);
	} while (v /= 10);
	hwgraph_edge_add(ivtx, connpt, np);
    }

    {
	int                     hwm;
	device_driver_t        *dl;
	int                     ix;
	int                     found = 0;

	hwm = ent->ndrivers;
	dl = ent->drivers;
	for (ix = 0; ix < hwm; ix++)
	    if (dl[ix] != NULL)
		if (cdl_call_attach(dl[ix], connpt) == 0)
		    found++;

	if (!found && (key1 != -1) && (key2 != -1)) {
	    char                    buf[80];

	    if (ent->uvtx == GRAPH_VERTEX_NONE) {
		sprintf(buf, "%s/%s/%s/0x%x/%s/0x%x/instance",
			EDGE_LBL_UNKNOWN, reg->name,
			reg->k1str, key1, reg->k2str, key2);
		(void) hwgraph_path_add(hwgraph_root, buf, &ent->uvtx);
	    }
	    sprintf(buf, "%u", connpt);
	    hwgraph_edge_add(ent->uvtx, connpt, buf);
	}
    }

    cdl_lock_done(ent);

    if ((key1 != -1) && (key2 != -1)) {
	cdl_add_connpt(reg, -1, key2, connpt);
	cdl_add_connpt(reg, key1, -1, connpt);
	cdl_add_connpt(reg, -1, -1, connpt);
    }

    return 0;
}

/*
 * cdl_del_connpt: remove a connection point from the
 * registry, and call the detach routine for each
 * matching driver.
 */
void
cdl_del_connpt(cdl_p reg, int key1, int key2, vertex_hdl_t connpt)
{
    ent_p                   ent;

    ent = cdl_ent(reg, key1, key2);
    if (ent == NULL)
	return;

    cdl_lock_beta(ent);
    mutex_lock(ent->connpts_lock, PZERO);

    {
	int                     hwm;
	int                     ix;
	vertex_hdl_t           *cpl;

	hwm = ent->nconnpts;
	cpl = ent->connpts;
	for (ix = 0; ix < hwm; ++ix)
	    if (cpl[ix] == connpt)
		cpl[ix] = GRAPH_VERTEX_NONE;
    }

    mutex_unlock(ent->connpts_lock);

    {
	int                     hwm;
	device_driver_t        *dl;
	int                     ix;

	hwm = ent->ndrivers;
	dl = ent->drivers;
	for (ix = 0; ix < hwm; ix++)
	    if (dl[ix] != NULL)
		cdl_call_detach(dl[ix], connpt);
    }

    {
	vertex_hdl_t	ivtx;

	ivtx = ent->ivtx;

	if (ivtx != GRAPH_VERTEX_NONE) {
	    char		iname[16];
	    unsigned		v;
	    char	       *np;

	    np = iname + sizeof iname;
	    *--np = 0;
	    v = connpt;
	    do {
		*--np = '0' + (v%10);
	    } while (v /= 10);
	    hwgraph_edge_remove(ivtx, np, 0);
	}
    }

    cdl_lock_done(ent);

    hwgraph_vertex_ref(connpt);
}

/*
 *    cdl_iterate: find all verticies in the registry
 *      corresponding to the named driver and call them
 *      with the specified function (giving the vertex
 *      as the parameter).
 */
void
cdl_iterate(cdl_p reg,
	    char *prefix,
	    cdl_iter_f * func)
{
    device_driver_t         driver;
    ent_p                   ent;
    int                     found;

    if (prefix == NULL)
	return;

    driver = device_driver_get(prefix);
    if (driver == DEVICE_DRIVER_NONE) {
	printf("cdl_del_driver: unable to find driver '%s'\n",
	       prefix);
	return;
    }
    for (ent = reg->list; ent != NULL; ent = ent->next) {
	int                     hwm;
	device_driver_t        *dl;
	vertex_hdl_t           *cl;
	int                     ix;

	cdl_lock_alfa(ent);
	mutex_lock(ent->drivers_lock, PZERO);

	found = 0;
	hwm = ent->ndrivers;
	dl = ent->drivers;
	for (ix = 0; ix < hwm; ++ix)
	    if (dl[ix] == driver)
		found++;

	mutex_unlock(ent->drivers_lock);

	if (found) {
	    hwm = ent->nconnpts;
	    cl = ent->connpts;
	    while (found-- > 0)
		for (ix = 0; ix < hwm; ++ix)
		    if (cl[ix] != GRAPH_VERTEX_NONE)
			func(cl[ix]);
	}
	cdl_lock_done(ent);
    }
}

async_attach_t 
async_attach_new(void)
{
	async_attach_t aa;

	NEW(aa);
	initsema(&aa->async_sema, 0);
	return(aa);
}

void 
async_attach_free(async_attach_t aa)
{
	DEL(aa);
}

async_attach_t 
async_attach_get_info(vertex_hdl_t vhdl)
{
	async_attach_t aa;

	if (hwgraph_info_get_LBL(vhdl, INFO_LBL_ASYNC_ATTACH, (arbitrary_info_t *)&aa) == GRAPH_SUCCESS)
		return(aa);
	else
		return(NULL);
}

void            
async_attach_add_info(vertex_hdl_t vhdl, async_attach_t aa)
{
	hwgraph_info_add_LBL(vhdl, INFO_LBL_ASYNC_ATTACH, (arbitrary_info_t)aa);
}

void            
async_attach_del_info(vertex_hdl_t vhdl)
{
	async_attach_t	aa;
	
	mutex_lock(&async_attach_del_lock, PZERO);
	if (hwgraph_info_get_LBL(vhdl, INFO_LBL_ASYNC_ATTACH, (arbitrary_info_t *)&aa) == GRAPH_SUCCESS)
		hwgraph_info_remove_LBL(vhdl, INFO_LBL_ASYNC_ATTACH, NULL);
	mutex_unlock(&async_attach_del_lock);
}

void async_attach_signal_start(async_attach_t aa)
{
	atomicAddInt(&aa->async_count, 1);
}

void async_attach_signal_done(async_attach_t aa)
{
	vsema(&aa->async_sema);
}

void async_attach_waitall(async_attach_t aa)
{
	int i;
	
	for (i = 0; i < aa->async_count; ++i)
		psema(&aa->async_sema, PZERO);

}

