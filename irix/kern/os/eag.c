/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.47 $"

/*
 * This collection of functions and data elements implements
 * Extended Attributes - plan G (eag) 
 */

#include "sys/types.h"
#include "sys/systm.h"
#include "sys/debug.h"
#include "sys/cmn_err.h"
#include "sys/cred.h"
#include "sys/proc.h"
#include "sys/errno.h"
#include "sys/file.h"
#include "sys/vnode.h"
#include "sys/vfs.h"
#include "sys/uio.h"
#include "sys/eag.h"
#include "sys/mac_label.h"
#include "sys/capability.h"
#include "sys/xlate.h"

void
eag_confignote(void)
{
        cmn_err(CE_CONT, "Extended Process Attributes Enabled.\n");
}

/*
 * Look for a particular attribute in the attribute string.
 * Format of the attribute string is:
 *	short	<total size>
 *	char	<attribute count>
 *		short	<size of this attribute>
 *		char	<size of this attribute name, including '\0'>
 *		char[?]	<attribute name>
 *		char[?]	<attribute value>
 *
 * Returns 1 on success, 0 if attrname not found.
 */
int
eag_parseattr(char *desired, char *attrs, void *result)
{
	char *dp;
	char *ap;
	char *attr_end;
	char attr_count;
	char name_size;
	short total_size;
	short attr_size;
	
	/*
	 * Fetch the total size of the attributes.
	 */
	bcopy(attrs, &total_size, sizeof(total_size));
	/*
	 * Mark the end of sanity.
	 */
	attr_end = attrs + total_size;
	/*
	 * Skip past the size
	 */
	attrs += sizeof(total_size);
	/*
	 * Fetch the number of the attributes.
	 */
	bcopy(attrs, &attr_count, sizeof(attr_count));
	/*
	 * And skip past the count, too.
	 */
	attrs += sizeof(attr_count);

	/*
	 * Loop through the attributes, looking for the desired one.
	 */
	for (; attr_count > 0; attr_count--) {
		/*
		 * Verify that the new value of attr remains sane...
		 */
		bcopy(attrs, &attr_size, sizeof(attr_size));
		if ((attrs += sizeof(attr_size)) > attr_end)
			return (0);

		bcopy(attrs, &name_size, sizeof(name_size));
		if ((attrs += sizeof(name_size)) > attr_end)
			return (0);

		attr_size -= sizeof(attr_size) + sizeof(name_size) + name_size;
		if (attrs + name_size + attr_size > attr_end)
			return (0);

		for (ap = attrs, dp = desired; *ap == *dp; ap++, dp++) {
			if (*ap == '\0' && *dp == '\0') {
				if (result)
					bcopy(attrs+name_size,result,attr_size);
				return (1);
			}
		}
		if ((attrs += attr_size + name_size) > attr_end)
			return (0);
	}
	return (0);
}


#if (_MIPS_SIM == _MIPS_SIM_ABI64)
/*ARGSUSED*/
int
irix5_to_eag_mount(enum xlate_mode mode, void *to, int count,
		   xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_eag_mount_s, eag_mount_s);

	target->spec    = (char *) (__psunsigned_t) source->spec;
	target->dir     = (char *) (__psunsigned_t) source->dir;
	target->flags   = source->flags;
	target->fstype  = (char *) (__psunsigned_t) source->fstype;
	target->dataptr = (char *) (__psunsigned_t) source->dataptr;
	target->datalen = source->datalen;

	return 0;
}
#endif

/*
 * Mount a file system, specifying attributes.
 */
int
eag_mount(struct eag_mount_s *ep, rval_t *rvp, char *attrs)
{
	short attr_size;
	char *kattrs;	/* attributes in kernel space */
	int error;
	struct eag_mount_s eag_mount;
	struct mounta mounta;

	if (!cap_able(CAP_MOUNT_MGT))
		return (EPERM);

	/* Copy in the mount options */
	if (COPYIN_XLATE(ep, &eag_mount, sizeof(eag_mount),
			 irix5_to_eag_mount, get_current_abi(), 1)) {
		return(EFAULT);
	}

	/* Copy in attribute size */
	if (copyin(attrs, &attr_size, sizeof(attr_size))) {
#ifdef DEBUG
		cmn_err(CE_WARN, "eag_mount: copyin of attr_size");
#endif /* DEBUG */
		return (EFAULT);
	}

	kattrs = kern_malloc(attr_size);
	ASSERT(kattrs);

	/* Copy in attribute buffer */
	if (copyin(attrs, kattrs, attr_size)) {
#ifdef DEBUG
		cmn_err(CE_WARN, "eag_mount: FAILED %d bytes of attributes",
		    attr_size);
#endif /* DEBUG */
		kern_free(kattrs);
		return (EFAULT);
	}

	mounta.spec = eag_mount.spec;
	mounta.dir = eag_mount.dir;
	mounta.flags = eag_mount.flags;
	mounta.fstype = eag_mount.fstype;
	mounta.dataptr = eag_mount.dataptr;
	mounta.datalen = eag_mount.datalen;

	error = cmount(&mounta, rvp, kattrs, UIO_USERSPACE);
#ifdef DEBUG
	if (error)
		cmn_err(CE_WARN, "eag_mount: error %d from cmount", error);
#endif /* DEBUG */

	kern_free(kattrs);

	return (error);
}

/*
 * Fetch an arbitrary attribute associated with the current process.
 */
int
proc_attr_get(char *attrname, char *result)
{
	char kattrname[EAG_MAX_ATTR_NAME];	/* attribute name in kernel
						   space */
	int error;				/* error code */
	size_t s;				/* size of attribute name
						   including trailing NUL */
	cred_t *cr;				/* user's credential */

	/* check arguments */
	if (attrname == NULL || result == NULL)
		return(EINVAL);

	/* copy attribute name to kernel space */
	if (error = copyinstr(attrname, kattrname, sizeof(kattrname), &s))
		return(error);

	/* get credential */
	cr = get_current_cred();

	/* copy attribute value to user space */
	if (s == sizeof(SGI_MAC_PROCESS) &&
	    !bcmp(SGI_MAC_PROCESS, kattrname, s)) {
		if (mac_enabled) {
			if (copyout((caddr_t)cr->cr_mac, (caddr_t)result,
				    _MAC_SIZE(cr->cr_mac)))
				error = EFAULT;
		}
		else
			error = ENOSYS;
	}
	else if (s == sizeof(SGI_CAP_PROCESS) &&
		 !bcmp(SGI_CAP_PROCESS, kattrname, s)) {
		if (copyout((caddr_t)&cr->cr_cap, (caddr_t)result,
			    sizeof(cap_set_t)))
			error = EFAULT;
	}
	else
		error = ENOATTR;

	return (error);
}

/* each type of attribute value passed to proc_attr_set() goes here */
union proc_attr_value {
	mac_label ml;
	cap_set_t cs;
	cap_value_t cv;
};

/*
 * Set an arbitrary attribute associated with the current process.
 */
int
proc_attr_set(char *attrname, char *value, int size)
{
	union proc_attr_value kvalue;		/* attribute value in kernel
						   space */
	char kattrname[EAG_MAX_ATTR_NAME];	/* attribute name in kernel
						   space */
	int error;				/* error code */
	size_t s;				/* size of attribute name
						   including trailing NUL */

	/* check arguments */
	if (attrname == NULL || value == NULL || size < 0 || size > 0xffff)
		return(EINVAL);

	/* copy attribute name to kernel space */
	if (error = copyinstr(attrname, kattrname, sizeof(kattrname), &s))
		return (error);

	/* copy attribute value to kernel space and execute system call */
	if (s == sizeof(SGI_MAC_PROCESS) &&
	    !bcmp(SGI_MAC_PROCESS, kattrname, s)) {
		if (copyin(value, &kvalue.ml, sizeof(kvalue.ml)))
			error = EFAULT;
		else
			error = _MAC_SETPLABEL(&kvalue.ml, 0);
	} else if (s == sizeof(SGI_CAP_PROCESS) &&
		   !bcmp(SGI_CAP_PROCESS, kattrname, s)) {
		if (copyin(value, &kvalue.cs, sizeof(kvalue.cs)))
			error = EFAULT;
		else
			error = cap_setpcap(&kvalue.cs, (cap_value_t *) NULL);
	} else if (s == sizeof(SGI_CAP_PROCESS_FLAGS) &&
		   !bcmp(SGI_CAP_PROCESS_FLAGS, kattrname, s)) {
		if (copyin(value, &kvalue.cv, sizeof(kvalue.cv)))
			error = EFAULT;
		else
			error = cap_setpcap((cap_t) NULL, &kvalue.cv);
	} else if (s == sizeof(SGI_CAP_REQUEST) &&
		   !bcmp(SGI_CAP_REQUEST, kattrname, s)) {
		if (copyin(value, &kvalue.cv, sizeof(kvalue.cv)))
			error = EFAULT;
		else
			error = cap_request(kvalue.cv);
	} else if (s == sizeof(SGI_CAP_SURRENDER) &&
		   !bcmp(SGI_CAP_SURRENDER, kattrname, s)) {
		if (copyin(value, &kvalue.cv, sizeof(kvalue.cv)))
			error = EFAULT;
		else
			error = cap_surrender(kvalue.cv);
#ifdef DEBUG
	} else if (s == sizeof(SGI_CAP_SUPERUSER) &&
		   !bcmp(SGI_CAP_SUPERUSER, kattrname, s)) {
		error = cap_style(CAP_SYS_SUPERUSER);
	} else if (s == sizeof(SGI_CAP_NO_SUPERUSER) &&
		   !bcmp(SGI_CAP_NO_SUPERUSER, kattrname, s)) {
		error = cap_style(CAP_SYS_NO_SUPERUSER);
	} else if (s == sizeof(SGI_CAP_DISABLED) &&
		   !bcmp(SGI_CAP_DISABLED, kattrname, s)) {
		error = cap_style(CAP_SYS_DISABLED);
#endif /* DEBUG */
	} else
		error = ENOATTR;

	return (error);
}
