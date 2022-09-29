/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.35 $"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/driver.h>
#include <sys/edt.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/strtbl.h>
#include <sys/strsubr.h>
#include <sys/systm.h>


/* 
 * Interfaces in this file are all platform-independent AND IObus-independent.
 * Be aware that there may be macro equivalents to each of these hiding in
 * header files which supercede these functions.
 */

/* =====Generic iobus support===== */

/* String table to hold names of interrupts. */
static struct string_table device_desc_string_table;

/* One time initialization for device descriptor support. */
static void
device_desc_init(void)
{
	string_table_init(&device_desc_string_table);
}


/* Drivers use these interfaces to manage device descriptors */
static device_desc_t
device_desc_alloc(void)
{
	device_desc_t device_desc;

	device_desc = (device_desc_t)kmem_zalloc(sizeof(struct device_desc_s), 0);
	device_desc->intr_target = GRAPH_VERTEX_NONE;

	ASSERT(device_desc->intr_policy == 0);
	device_desc->intr_swlevel = -1;
	ASSERT(device_desc->intr_name == NULL);
	ASSERT(device_desc->flags == 0);

	ASSERT(!(device_desc->flags & D_IS_ASSOC));

	return(device_desc);
}

void
device_desc_free(device_desc_t device_desc)
{
	if (!(device_desc->flags & D_IS_ASSOC)) /* sanity */
		kmem_free(device_desc, sizeof(struct device_desc_s));
}

device_desc_t
device_desc_dup(vertex_hdl_t dev)
{
	device_desc_t orig_device_desc, new_device_desc;


	new_device_desc = device_desc_alloc();
	orig_device_desc = device_desc_default_get(dev);
	if (orig_device_desc)
		*new_device_desc = *orig_device_desc;/* small structure copy */
	else {
		device_driver_t		driver;
		ilvl_t			pri;		
		/* 
		 * Use the driver's thread priority in 
		 * case the device thread priority has not
		 * been given.
		 */
		if (driver = device_driver_getbydev(dev)) {
			pri = device_driver_thread_pri_get(driver);
			device_desc_intr_swlevel_set(new_device_desc,pri);
		}
	}		
	new_device_desc->flags &= ~D_IS_ASSOC;
	
	return(new_device_desc);
}

device_desc_t	
device_desc_default_get(vertex_hdl_t dev)
{
	graph_error_t rc;
	device_desc_t device_desc;

	rc = hwgraph_info_get_LBL(dev, INFO_LBL_DEVICE_DESC, (arbitrary_info_t *)&device_desc);

	if (rc == GRAPH_SUCCESS)
		return(device_desc);
	else
		return(NULL);
}

void		
device_desc_default_set(vertex_hdl_t dev, device_desc_t new_device_desc)
{
	graph_error_t rc;
	device_desc_t old_device_desc = NULL;

	if (new_device_desc) {
		new_device_desc->flags |= D_IS_ASSOC;
		rc = hwgraph_info_add_LBL(dev, INFO_LBL_DEVICE_DESC, 
						(arbitrary_info_t)new_device_desc);
		if (rc == GRAPH_DUP) {
			rc = hwgraph_info_replace_LBL(dev, INFO_LBL_DEVICE_DESC, 
				(arbitrary_info_t)new_device_desc, 
				(arbitrary_info_t *)&old_device_desc);

			ASSERT(rc == GRAPH_SUCCESS);
		}
		hwgraph_info_export_LBL(dev, INFO_LBL_DEVICE_DESC,
					sizeof(struct device_desc_s));
	} else {
		rc = hwgraph_info_remove_LBL(dev, INFO_LBL_DEVICE_DESC,
					(arbitrary_info_t *)&old_device_desc);
	}

	if (old_device_desc) {
		ASSERT(old_device_desc->flags & D_IS_ASSOC);
		old_device_desc->flags &= ~D_IS_ASSOC;
		device_desc_free(old_device_desc);
	}
}

vertex_hdl_t
device_desc_intr_target_get(device_desc_t device_desc)
{
	return(device_desc->intr_target);
}

int
device_desc_intr_policy_get(device_desc_t device_desc)
{
	return(device_desc->intr_policy);
}

ilvl_t
device_desc_intr_swlevel_get(device_desc_t device_desc)
{
	return(device_desc->intr_swlevel);
}

char *
device_desc_intr_name_get(device_desc_t device_desc)
{
	return(device_desc->intr_name);
}


int
device_desc_flags_get(device_desc_t device_desc)
{
	return(device_desc->flags);
}

void
device_desc_intr_target_set(device_desc_t device_desc, vertex_hdl_t target)
{
	device_desc->intr_target = target;
}

void
device_desc_intr_policy_set(device_desc_t device_desc, int policy)
{
	device_desc->intr_policy = policy;
}

void
device_desc_intr_swlevel_set(device_desc_t device_desc, ilvl_t swlevel)
{
	device_desc->intr_swlevel = swlevel;
}

void
device_desc_intr_name_set(device_desc_t device_desc, char *name)
{
	device_desc->intr_name = string_table_insert(&device_desc_string_table, name);
}

void
device_desc_flags_set(device_desc_t device_desc, int flags)
{
	device_desc->flags = flags;
}

/* Drivers use these interfaces to manage Equipped Device Table entries */
void *
edt_bus_info_get(edt_t *edt)
{
	return(edt->e_bus_info);
}

vertex_hdl_t
edt_connectpt_get(edt_t *edt)
{
	return(edt->e_connectpt);
}

vertex_hdl_t
edt_master_get(edt_t *edt)
{
	return(edt->e_master);
}

device_desc_t
edt_device_desc_get(edt_t *edt)
{
	return(edt->e_device_desc);
}




/*============= device admin registry routines ===================== */

/* Linked list of <admin-name,admin-val> pairs */
typedef struct dev_admin_list_s {
	struct dev_admin_list_s		*admin_next; 	/* next entry in the
							 * list 
							 */
	char				*admin_name;	/* info label */
	char				*admin_val;	/* actual info */
} dev_admin_list_t;

/* Device/Driver administration registry */
typedef struct dev_admin_registry_s {
	mrlock_t			reg_lock;	/* To allow
							 * exclusive
							 * access
							 */
	dev_admin_list_t		*reg_first;	/* first entry in 
							 * the list
							 */
	dev_admin_list_t		**reg_last;	/* pointer to the
							 * next to last entry
							 * in the last which 
							 * is also the place
							 * where the new
							 * entry gets
							 * inserted
							 */
} dev_admin_registry_t;

/*
** device_driver_s associates a device driver prefix with device switch entries.
*/
struct device_driver_s {
	struct device_driver_s	*dd_next;	/* next element on hash chain */
	struct device_driver_s	*dd_prev;	/* previous element on hash chain */
	char			*dd_prefix;	/* driver prefix string */
	struct bdevsw		*dd_bdevsw;	/* driver's bdevsw */
	struct cdevsw		*dd_cdevsw;	/* driver's cdevsw */
	
	/* driver administration specific data structures need to
	 * maintain the list of <driver-paramater,value> pairs
	 */
	dev_admin_registry_t	dd_dev_admin_registry;
	ilvl_t			dd_thread_pri;	/* default thread priority for
						 *  all this driver's
						 * threads.
						 */

};

#define	NEW(_p)		(_p = kmem_alloc(sizeof(*_p),KM_SLEEP))
#define FREE(_p)	(kmem_free(_p,sizeof(*_p)))
	
/*
 * helpful lock macros
 */

#define DEV_ADMIN_REGISTRY_INITLOCK(lockp,name)	mrinit(lockp,name)
#define DEV_ADMIN_REGISTRY_RDLOCK(lockp)	mraccess(lockp)	       
#define DEV_ADMIN_REGISTRY_WRLOCK(lockp)	mrupdate(lockp)	       
#define DEV_ADMIN_REGISTRY_UNLOCK(lockp)	mrunlock(lockp)

/* Initialize the registry 
 */
static void
dev_admin_registry_init(dev_admin_registry_t *registry)
{
	DEV_ADMIN_REGISTRY_INITLOCK(&registry->reg_lock,
				    "dev_admin_registry_lock");
	registry->reg_first = NULL;
	registry->reg_last = &registry->reg_first;
	
}
/*
 * add an <name , value > entry to the dev admin registry.
 * if the name already exists in the registry then change the
 * value iff the new value differs from the old value.
 * if the name doesn't exist a new list entry is created and put
 * at the end.
 */
static void
dev_admin_registry_add(dev_admin_registry_t	*registry,
		       char			*name,
		       char			*val)
{
	dev_admin_list_t	*reg_entry;
	dev_admin_list_t	*scan = 0;

	DEV_ADMIN_REGISTRY_WRLOCK(&registry->reg_lock);

	/* check if the name already exists in the registry */
	scan = registry->reg_first;

	while (scan) {
		if (strcmp(scan->admin_name,name) == 0) {
			/* name is there in the registry */
			if (strcmp(scan->admin_val,val)) {
				/* old value != new value 
				 * reallocate  memory and copy the new value
				 */
				FREE(scan->admin_val);
				scan->admin_val = 
					(char *)kern_calloc(1,strlen(val)+1);
				strcpy(scan->admin_val,val);
				goto out;
			}
			goto out;	/* old value == new value */
		}
		scan = scan->admin_next;
	}

	/* name is not there in the registry.
	 * allocate memory for the new registry entry 
	 */
	NEW(reg_entry);
	
	reg_entry->admin_next   	= 0;
	reg_entry->admin_name	= (char *)kern_calloc(1,strlen(name)+1);
	strcpy(reg_entry->admin_name,name);
	reg_entry->admin_val	= (char *)kern_calloc(1,strlen(val)+1);
	strcpy(reg_entry->admin_val,val);

	/* add the entry at the end of the registry */

	*(registry->reg_last)	= reg_entry;
	registry->reg_last	= &reg_entry->admin_next;

out:	DEV_ADMIN_REGISTRY_UNLOCK(&registry->reg_lock);
}
/*
 * check if there is an info corr. to a particular
 * name starting from the cursor position in the 
 * registry
 */
static char *
dev_admin_registry_find(dev_admin_registry_t *registry,char *name)
{
	dev_admin_list_t	*scan = 0;
	
	DEV_ADMIN_REGISTRY_RDLOCK(&registry->reg_lock);
	scan = registry->reg_first;

	while (scan) {
		if (strcmp(scan->admin_name,name) == 0) {
			DEV_ADMIN_REGISTRY_UNLOCK(&registry->reg_lock);
			return scan->admin_val;
		}
		scan = scan->admin_next;
	}
	DEV_ADMIN_REGISTRY_UNLOCK(&registry->reg_lock);
	return 0;
}
/*============= MAIN DEVICE/ DRIVER ADMINISTRATION INTERFACE================ */
/*
 * return any labelled info associated with a device.
 * called by any kernel code including device drivers.
 */
char *
device_admin_info_get(vertex_hdl_t	dev_vhdl,
		      char		*info_lbl)
{
	char		*info = 0;

	/* return value need not be GRAPH_SUCCESS as the labelled
	 * info may not be present
	 */
	(void)hwgraph_info_get_LBL(dev_vhdl,info_lbl,
				   (arbitrary_info_t *)&info);

	
	return info;
}

/*
 * set labelled info associated with a device.
 * called by hwgraph infrastructure . may also be called
 * by device drivers etc.
 */
int
device_admin_info_set(vertex_hdl_t	dev_vhdl,
		      char		*dev_info_lbl,
		      char		*dev_info_val)
{
	graph_error_t		rv;
	arbitrary_info_t	old_info;

	/* Handle the labelled info
	 *		intr_target
	 *		sw_level 
	 * in a special way. These are part of device_desc_t
	 * Right now this is the only case where we have 
	 * a set of related device_admin attributes which 
	 * are grouped together.
	 * In case there is a need for another set we need to
	 * take a more generic approach to solving this.
	 * Basically a registry should be implemented. This
	 * registry is initialized with the callbacks for the
	 * attributes which need to handled in a special way
	 * For example:
	 * Consider
	 * 		device_desc
	 *			intr_target
	 *			intr_swlevel
	 * register "do_intr_target" for intr_target
	 * register "do_intr_swlevel" for intr_swlevel.
	 * When the device_admin interface layer gets an <attr,val> pair
	 * it looks in the registry to see if there is a function registered to
	 * handle "attr. If not follow the default path of setting the <attr,val>
	 * as labelled information hanging off the vertex.
	 * In the above example:
	 * "do_intr_target" does what is being done below for the ADMIN_LBL_INTR_TARGET
	 * case
	 */		
	if (!strcmp(dev_info_lbl,ADMIN_LBL_INTR_TARGET) ||
	    !strcmp(dev_info_lbl,ADMIN_LBL_INTR_SWLEVEL)) {

		device_desc_t	device_desc;
		
		/* Check if there is a default device descriptor
		 * information for this vertex. If not dup one .
		 */
		if (!(device_desc = device_desc_default_get(dev_vhdl))) {
			device_desc = device_desc_dup(dev_vhdl);
			device_desc_default_set(dev_vhdl,device_desc);

		}
		if (!strcmp(dev_info_lbl,ADMIN_LBL_INTR_TARGET)) {
			/* Check if a target cpu has been specified
			 * for this device by a device administration
			 * directive
			 */
#ifdef DEBUG	
			printf(ADMIN_LBL_INTR_TARGET
			       "dev = 0x%x "
			       "dev_admin_info = %s"
			       " target cpu = 0x%x\n",
			       dev_vhdl,
			       dev_info_lbl,
			       hwgraph_path_to_vertex(dev_info_val));
#endif	

			device_desc->intr_target = 
				hwgraph_path_to_vertex(dev_info_val);
		} else if (!strcmp(dev_info_lbl,ADMIN_LBL_INTR_SWLEVEL)) {
			/* Check if the ithread priority level  has been 
			 * specified for this device by a device administration
			 * directive
			 */
#ifdef DEBUG	
			printf(ADMIN_LBL_INTR_SWLEVEL
			       "dev = 0x%x "
			       "dev_admin_info = %s"
			       " sw level = 0x%x\n",
			       dev_vhdl,
			       dev_info_lbl,
			       atoi(dev_info_val));
#endif	
			device_desc->intr_swlevel = atoi(dev_info_val);
		}

	}
	if (!dev_info_val)
		rv = hwgraph_info_remove_LBL(dev_vhdl,
					     dev_info_lbl,
					     &old_info);
	else {

		rv = hwgraph_info_add_LBL(dev_vhdl,
					  dev_info_lbl,
					  (arbitrary_info_t)dev_info_val);
	
		if (rv == GRAPH_DUP)  {
			rv = hwgraph_info_replace_LBL(dev_vhdl,
					      dev_info_lbl,
					      (arbitrary_info_t)dev_info_val,
					      &old_info);
		}
	}
	ASSERT(rv == GRAPH_SUCCESS);
	return 0;
}

/*
 * return labelled info associated with a device driver
 * called by kernel code including device drivers
 */
char *
device_driver_admin_info_get(char		*driver_prefix,
			     char		*driver_info_lbl)
{
	device_driver_t driver;

	driver = device_driver_get(driver_prefix);
	return (dev_admin_registry_find(&driver->dd_dev_admin_registry,
					driver_info_lbl));
}

/*
 * set labelled info associated with a device driver.
 * called by hwgraph infrastructure . may also be called
 * from drivers etc.
 */
int
device_driver_admin_info_set(char		*driver_prefix,
			     char		*driver_info_lbl,
			     char		*driver_info_val)
{
	device_driver_t driver;

	driver = device_driver_get(driver_prefix);
	dev_admin_registry_add(&driver->dd_dev_admin_registry,	
			       driver_info_lbl,
			       driver_info_val);
	
			       
	return 0;
}
/*================== device / driver  admin support routines================*/

/* static tables created by lboot */
extern dev_admin_info_t	dev_admin_table[];
extern dev_admin_info_t	drv_admin_table[];
extern int		dev_admin_table_size;
extern int		drv_admin_table_size;

/* Extend the device admin table to allow the kernel startup code to 
 * provide some device specific administrative hints
 */
#define ADMIN_TABLE_CHUNK	100
static dev_admin_info_t extended_dev_admin_table[ADMIN_TABLE_CHUNK];	
static int		extended_dev_admin_table_size = 0;
static mrlock_t		extended_dev_admin_table_lock;

/* Initialize the extended device admin table */
void
device_admin_table_init(void)
{
	extended_dev_admin_table_size = 0;
	mrinit(&extended_dev_admin_table_lock,
	       "extended_dev_admin_table_lock");
}
/* Add <device-name , parameter-name , parameter-value> triple to
 * the extended device administration info table. This is helpful
 * for kernel startup code to put some hints before the hwgraph
 * is setup 
 */
void
device_admin_table_update(char *name,char *label,char *value)
{
	dev_admin_info_t	*p;

	mrupdate(&extended_dev_admin_table_lock);

	/* Safety check that we haven't exceeded array limits */
	ASSERT(extended_dev_admin_table_size < ADMIN_TABLE_CHUNK);

	if (extended_dev_admin_table_size == ADMIN_TABLE_CHUNK)
		goto out;
	
	/* Get the pointer to the entry in the table where we are
	 * going to put the new information 
	 */
	p = &extended_dev_admin_table[extended_dev_admin_table_size++];

	/* Allocate memory for the strings and copy them in */
	p->dai_name = (char *)kern_calloc(1,strlen(name)+1);
	strcpy(p->dai_name,name);
	p->dai_param_name = (char *)kern_calloc(1,strlen(label)+1);
	strcpy(p->dai_param_name,label);
	p->dai_param_val = (char *)kern_calloc(1,strlen(value)+1);
	strcpy(p->dai_param_val,value);

out:	mrunlock(&extended_dev_admin_table_lock);
}
/* Extend the device driver  admin table to allow the kernel startup code to 
 * provide some device driver specific administrative hints
 */

static dev_admin_info_t extended_drv_admin_table[ADMIN_TABLE_CHUNK];	
static int		extended_drv_admin_table_size = 0;
mrlock_t		extended_drv_admin_table_lock;

/* Initialize the extended device driver admin table */
void
device_driver_admin_table_init(void)
{
	extended_drv_admin_table_size = 0;
	mrinit(&extended_drv_admin_table_lock,
	       "extended_drv_admin_table_lock");
}
/* Add <device-driver prefix , parameter-name , parameter-value> triple to
 * the extended device administration info table. This is helpful
 * for kernel startup code to put some hints before the hwgraph
 * is setup 
 */
void
device_driver_admin_table_update(char *name,char *label,char *value)
{
	dev_admin_info_t	*p;

	mrupdate(&extended_dev_admin_table_lock);

	/* Safety check that we haven't exceeded array limits */
	ASSERT(extended_drv_admin_table_size < ADMIN_TABLE_CHUNK);

	if (extended_drv_admin_table_size == ADMIN_TABLE_CHUNK)
		goto out;
	
	/* Get the pointer to the entry in the table where we are
	 * going to put the new information 
	 */
	p = &extended_drv_admin_table[extended_drv_admin_table_size++];

	/* Allocate memory for the strings and copy them in */
	p->dai_name = (char *)kern_calloc(1,strlen(name)+1);
	strcpy(p->dai_name,name);
	p->dai_param_name = (char *)kern_calloc(1,strlen(label)+1);
	strcpy(p->dai_param_name,label);
	p->dai_param_val = (char *)kern_calloc(1,strlen(value)+1);
	strcpy(p->dai_param_val,value);

out:	mrunlock(&extended_drv_admin_table_lock);
}
/*
 * keeps on adding the labelled info for each new (lbl,value) pair
 * that it finds in the static dev admin table (  created by lboot)
 * and the extended dev admin table ( created if at all by the kernel startup
 *  code) corresponding to a device in the hardware graph.
 */
void
device_admin_info_update(vertex_hdl_t	dev_vhdl)
{
	int			i = 0;
	dev_admin_info_t	*scan;
	vertex_hdl_t		scan_vhdl;
	
	/* Check the static device administration info table */
	scan = dev_admin_table;
	while (i < dev_admin_table_size) {
		
		scan_vhdl = hwgraph_path_to_dev(scan->dai_name);
		if (scan_vhdl == dev_vhdl) {
			device_admin_info_set(dev_vhdl,
					      scan->dai_param_name,
					      scan->dai_param_val);
		}
		if (scan_vhdl != NODEV)
			hwgraph_vertex_unref(scan_vhdl);
		scan++;i++;

	}
	i = 0;
	/* Check the extended device administration info table */
	scan = extended_dev_admin_table;
	while (i < extended_dev_admin_table_size) {
		scan_vhdl = hwgraph_path_to_dev(scan->dai_name);
		if (scan_vhdl == dev_vhdl) {
			device_admin_info_set(dev_vhdl,
					      scan->dai_param_name,
					      scan->dai_param_val);
		}
		if (scan_vhdl != NODEV)
			hwgraph_vertex_unref(scan_vhdl);
		scan++;i++;

	}


}

/* looks up the static drv admin table ( created by the lboot) and the extended
 * drv admin table (created if at all by the kernel startup code) 
 * for this driver specific administration info and adds it to the admin info 
 * associated with this device driver's object
 */
void
device_driver_admin_info_update(device_driver_t	driver)

{
	int			i = 0;
	dev_admin_info_t	*scan;

	/* Check the static device driver administration info table */
	scan = drv_admin_table;
	while (i < drv_admin_table_size) {

		if (strcmp(scan->dai_name,driver->dd_prefix) == 0) {
			dev_admin_registry_add(&driver->dd_dev_admin_registry,
						scan->dai_param_name,
					 	scan->dai_param_val);
		}
		scan++;i++;
	}
	i = 0;
	/* Check the extended device driver administration info table */
	scan = extended_drv_admin_table;
	while (i < extended_drv_admin_table_size) {

		if (strcmp(scan->dai_name,driver->dd_prefix) == 0) {
			dev_admin_registry_add(&driver->dd_dev_admin_registry,
						scan->dai_param_name,
					 	scan->dai_param_val);
		}
		scan++;i++;
	}
}

/* =====Device Driver Support===== */



/*
** Generic device driver support routines for use by kernel modules that
** deal with device drivers (but NOT for use by the drivers themselves).
** EVERY registered driver currently in the system -- static or loadable --
** has an entry in the device_driver_hash table.  A pointer to such an entry
** serves as a generic device driver handle.
*/

#define DEVICE_DRIVER_HASH_SIZE 32
lock_t device_driver_lock[DEVICE_DRIVER_HASH_SIZE];
device_driver_t device_driver_hash[DEVICE_DRIVER_HASH_SIZE];
static struct string_table driver_prefix_string_table;

/*
** Initialize device driver infrastructure.
*/
void
device_driver_init(void)
{
	int i;
	extern void alenlist_init(void);
	extern void hwgraph_init(void);
	extern void device_desc_init(void);

	ASSERT(DEVICE_DRIVER_NONE == NULL);
	alenlist_init();
	hwgraph_init();
	device_desc_init();

	string_table_init(&driver_prefix_string_table);

	for (i=0; i<DEVICE_DRIVER_HASH_SIZE; i++) {
		spinlock_init(&device_driver_lock[i], "devdrv");
		device_driver_hash[i] = NULL;
	}

	/* Initialize static drivers from master.c table */
	for (i=0; i<static_devsw_count; i++) {
		device_driver_t driver;
		static_device_driver_desc_t desc;
		int pri;

		desc = &static_device_driver_table[i];
		driver = device_driver_get(desc->sdd_prefix);
		if (!driver)
			driver = device_driver_alloc(desc->sdd_prefix);
		pri = device_driver_sysgen_thread_pri_get(desc->sdd_prefix);
		device_driver_thread_pri_set(driver, pri);
		device_driver_devsw_put(driver, desc->sdd_bdevsw, desc->sdd_cdevsw);
	}
}

/*
** Hash a prefix string into a hash table chain.
*/
static int
driver_prefix_hash(char *prefix)
{
	int accum = 0;
	char nextchar;

	while (nextchar = *prefix++)
		accum = accum ^ nextchar;

	return(accum % DEVICE_DRIVER_HASH_SIZE);
}


/*
** Allocate a driver handle.
** Returns the driver handle, or NULL if the driver prefix 
** already has a handle.
** 
** Upper layers prevent races among device_driver_alloc,
** device_driver_free, and device_driver_get*.
*/
device_driver_t
device_driver_alloc(char *prefix)
{
	int which_hash;
	device_driver_t new_driver;
	int s;
		
	which_hash = driver_prefix_hash(prefix);

	new_driver = kern_calloc(1, sizeof(*new_driver));
	ASSERT(new_driver != NULL);
	new_driver->dd_prev = NULL;
	new_driver->dd_prefix = string_table_insert(&driver_prefix_string_table, prefix);
	new_driver->dd_bdevsw = NULL;
	new_driver->dd_cdevsw = NULL;

	dev_admin_registry_init(&new_driver->dd_dev_admin_registry);
	device_driver_admin_info_update(new_driver);

	s = mutex_spinlock(&device_driver_lock[which_hash]);

#if DEBUG
	{
		device_driver_t drvscan;

		/* Make sure we haven't already added a driver with this prefix */
		drvscan = device_driver_hash[which_hash];
		while (drvscan && 
		        strcmp(drvscan->dd_prefix, prefix)) {
			drvscan = drvscan->dd_next;
		}

		ASSERT(!drvscan);
	}
#endif /* DEBUG */


	/* Add new_driver to front of hash chain. */
	new_driver->dd_next = device_driver_hash[which_hash];
	if (new_driver->dd_next)
		new_driver->dd_next->dd_prev = new_driver;
	device_driver_hash[which_hash] = new_driver;

	mutex_spinunlock(&device_driver_lock[which_hash], s);

	return(new_driver);
}

/*
** Free a driver handle.
**
** Statically loaded drivers should never device_driver_free.
** Dynamically loaded drivers device_driver_free when either an
** unloaded driver is unregistered, or when an unregistered driver
** is unloaded.
*/
void
device_driver_free(device_driver_t driver)
{
	int which_hash;
	int s;

	if (!driver)
		return;

	which_hash = driver_prefix_hash(driver->dd_prefix);

	s = mutex_spinlock(&device_driver_lock[which_hash]);

#if DEBUG
	{
		device_driver_t drvscan;

		/* Make sure we're dealing with the right list */
		drvscan = device_driver_hash[which_hash];
		while (drvscan && (drvscan != driver))
			drvscan = drvscan->dd_next;

		ASSERT(drvscan);
	}
#endif /* DEBUG */

	if (driver->dd_next)
		driver->dd_next->dd_prev = driver->dd_prev;

	if (driver->dd_prev)
		driver->dd_prev->dd_next = driver->dd_next;
	else
		device_driver_hash[which_hash] = driver->dd_next;

	mutex_spinunlock(&device_driver_lock[which_hash], s);

	driver->dd_next = NULL;		/* sanity */
	driver->dd_prev = NULL;		/* sanity */
	driver->dd_prefix = NULL;	/* sanity */

	if (driver->dd_bdevsw) {
		driver->dd_bdevsw->d_driver = NULL;
		driver->dd_bdevsw = NULL;
	}

	if (driver->dd_cdevsw) {
		if (driver->dd_cdevsw->d_str) {
			str_free_mux_node(driver);
		}
		driver->dd_cdevsw->d_driver = NULL;
		driver->dd_cdevsw = NULL;
	}

	kern_free(driver);
}


/*
** Given a device driver prefix, return a handle to the caller.
*/
device_driver_t
device_driver_get(char *prefix)
{
	int which_hash;
	device_driver_t drvscan;
	int s;

	if (prefix == NULL)
		return(NULL);
		
	which_hash = driver_prefix_hash(prefix);

	s = mutex_spinlock(&device_driver_lock[which_hash]);

	drvscan = device_driver_hash[which_hash];
	while (drvscan && strcmp(drvscan->dd_prefix, prefix))
		drvscan = drvscan->dd_next;

	mutex_spinunlock(&device_driver_lock[which_hash], s);

	return(drvscan);
}


/*
** Given a block or char special file dev_t, find the 
** device driver that controls it.
*/
device_driver_t
device_driver_getbydev(dev_t device)
{
	struct bdevsw *my_bdevsw;
	struct cdevsw *my_cdevsw;

	my_cdevsw = get_cdevsw(device);
	if (my_cdevsw != NULL)
		return(my_cdevsw->d_driver);

	my_bdevsw = get_bdevsw(device);
	if (my_bdevsw != NULL)
		return(my_bdevsw->d_driver);

	return(NULL);
}


/*
** Associate a driver with bdevsw/cdevsw pointers.
**
** Statically loaded drivers are permanently and automatically associated
** with the proper bdevsw/cdevsw.  Dynamically loaded drivers associate
** themselves when the driver is registered, and disassociate when the
** driver unregisters.
**
** Returns 0 on success, -1 on failure (devsw already associated with driver)
*/
int
device_driver_devsw_put(device_driver_t driver,
			struct bdevsw *my_bdevsw,
			struct cdevsw *my_cdevsw)
{
	int i;

	if (!driver)
		return(-1);

	/* Trying to re-register data?  */
	if (((my_bdevsw != NULL) && (driver->dd_bdevsw != NULL)) ||
	    ((my_cdevsw != NULL) && (driver->dd_cdevsw != NULL)))
		return(-1);

	if (my_bdevsw != NULL) {
		driver->dd_bdevsw = my_bdevsw;
		my_bdevsw->d_driver = driver;
		for (i = 0; i < bdevmax; i++) {
			if (driver->dd_bdevsw->d_flags == bdevsw[i].d_flags) {
				bdevsw[i].d_driver = driver;
				break;
			}
		}
	}

	if (my_cdevsw != NULL) {
		driver->dd_cdevsw = my_cdevsw;
		my_cdevsw->d_driver = driver;
		for (i = 0; i < cdevmax; i++) {
			if (driver->dd_cdevsw->d_flags == cdevsw[i].d_flags) {
				cdevsw[i].d_driver = driver;
				break;
			}
		}
	}

	return(0);
}


/*
** Given a driver, return the corresponding bdevsw and cdevsw pointers.
*/
void
device_driver_devsw_get(	device_driver_t driver, 
				struct bdevsw **bdevswp,
				struct cdevsw **cdevswp)
{

	if (!driver) {
		*bdevswp = NULL;
		*cdevswp = NULL;
	} else {
		*bdevswp = driver->dd_bdevsw;
		*cdevswp = driver->dd_cdevsw;
	}
}
/*
 * device_driver_thread_pri_set
 *	Given a driver try to set its thread priority.
 *	Returns 0 on success , -1 on failure.
 */ 
int
device_driver_thread_pri_set(device_driver_t driver,ilvl_t pri)
{
	if (!driver)
		return(-1);
	driver->dd_thread_pri = pri;
	return(0);
}
/*
 * device_driver_thread_pri_get
 * 	Given a driver return the driver thread priority.
 * 	If the driver is NULL return invalid driver thread
 * 	priority.
 */
ilvl_t
device_driver_thread_pri_get(device_driver_t driver)
{
	if (driver)
		return(driver->dd_thread_pri);
	else
		return(DRIVER_THREAD_PRI_INVALID);
}
/*
** Given a device driver, return it's handle (prefix).
*/
void
device_driver_name_get(device_driver_t driver, char *buffer, int length)
{
	if (driver == NULL)
		return;

	strncpy(buffer, driver->dd_prefix, length);
}


/*
** Associate a pointer-sized piece of information with a device.
*/
void 
device_info_set(dev_t device, void *info)
{
	hwgraph_fastinfo_set(device, (arbitrary_info_t)info);
}


/*
** Retrieve a pointer-sized piece of information associated with a device.
*/
void *
device_info_get(dev_t device)
{
	return((void *)hwgraph_fastinfo_get(device));
}

/*
 * Find the thread priority for a device, from the various
 * sysgen files.
 */
int
device_driver_sysgen_thread_pri_get(char *dev_prefix)
{
	int pri;
	char *pri_s;
	char *class;

	extern default_intr_pri;
	extern disk_intr_pri;
	extern serial_intr_pri;
	extern parallel_intr_pri;
	extern tape_intr_pri;
	extern graphics_intr_pri;
	extern network_intr_pri;
	extern scsi_intr_pri;
	extern audio_intr_pri;
	extern video_intr_pri;
	extern external_intr_pri;
	extern tserialio_intr_pri;

	/* Check if there is a thread priority specified for
	 * this driver's thread thru admin hints. If so 
	 * use that value. Otherwise set it to its default
	 * class value, otherwise set it to the default
	 * value.
	 */

	if (pri_s = device_driver_admin_info_get(dev_prefix,
						ADMIN_LBL_THREAD_PRI)) {
		pri = atoi(pri_s);
	} else if (class = device_driver_admin_info_get(dev_prefix,
						ADMIN_LBL_THREAD_CLASS)) {
		if (strcmp(class, "disk") == 0)
			pri = disk_intr_pri;
		else if (strcmp(class, "serial") == 0)
			pri = serial_intr_pri;
		else if (strcmp(class, "parallel") == 0)
			pri = parallel_intr_pri;
		else if (strcmp(class, "tape") == 0)
			pri = tape_intr_pri;
		else if (strcmp(class, "graphics") == 0)
			pri = graphics_intr_pri;
		else if (strcmp(class, "network") == 0)
			pri = network_intr_pri;
		else if (strcmp(class, "scsi") == 0)
			pri = scsi_intr_pri;
		else if (strcmp(class, "audio") == 0)
			pri = audio_intr_pri;
		else if (strcmp(class, "video") == 0)
			pri = video_intr_pri;
		else if (strcmp(class, "external") == 0)
			pri = external_intr_pri;
		else if (strcmp(class, "tserialio") == 0)
			pri = tserialio_intr_pri;
		else
			pri = default_intr_pri;
	} else
		pri = default_intr_pri;

	if (pri > 255)
		pri = 255;
	else if (pri < 0)
		pri = 0;
	return pri;
}
