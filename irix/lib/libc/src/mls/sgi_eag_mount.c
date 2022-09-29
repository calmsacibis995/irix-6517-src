/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1989, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.4 $"

#ifdef __STDC__
	#pragma weak sgi_eag_mount = _sgi_eag_mount
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/eag.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <sys/mount.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
 * Format of the attribute string is:
 *      short   <total size>
 *      char    <attribute count>
 *              short   <size of this attribute>
 *              char    <size of this attribute name, including '\0'>
 *              char[?] <attribute name>
 *              char[?] <attribute value>
 *
 */
static char *
sgi_eag_form_attr(char *attrname, ssize_t data_size, void *data, char *addto)
{
	short new_total_size;
	short total_size;
	short attr_size;
	char attr_count;
	unsigned char name_size;
	char *cp;
	size_t i;

	/*
	 * Do this check now to avoid any possibility of hosing addto.
	 */
	if ((i = strlen(attrname) + 1) > 0xff)
		return (NULL);
	name_size = (unsigned char) i;
	if (data_size > 0xffff)
		return (NULL);
	/*
	 * If this is the first time in create a header and try again.
	 */
	if (!addto)
		return (sgi_eag_form_attr(attrname, data_size, data,
		    calloc(1, sizeof(short) + sizeof(char))));

	/*
	 * Get the current size from the existing header.
	 */
	memcpy(&total_size, addto, sizeof(total_size));
	if (total_size == 0)
		total_size = sizeof(short) + sizeof(char);
	memcpy(&attr_count, addto + sizeof(total_size), sizeof(attr_count));

	/*
	 * Create a new buffer to put the data into.
	 */
	attr_size = (short)data_size + name_size + (short) sizeof(short) +
		(short) sizeof(char);
	new_total_size = total_size + attr_size;

	if ((addto = realloc(addto, new_total_size)) == NULL)
		return (NULL);

	/*
	 * Update the header information.
	 */
	attr_count++;

	memcpy(addto, &new_total_size, sizeof(new_total_size));
	memcpy(addto + sizeof(total_size), &attr_count, sizeof(attr_count));

	/*
	 * Add the new attribute.
	 */
	cp = addto + total_size;
	memcpy(cp, &attr_size, sizeof(attr_size));

	cp += sizeof(attr_size);
	memcpy(cp, &name_size, sizeof(name_size));

	cp += sizeof(name_size);
	memcpy(cp, attrname, name_size);

	cp += name_size;
	memcpy(cp, data, data_size);

	return (addto);
}

int
sgi_eag_mount(char *fspec, char *dir, int flags, char *fstyp,
	      char *dataptr, int datalen, char *attrs)
{
	struct eag_mount_s arg1;
	char *arg2;
	char *src;
	char *equal;
	char *colon;
	int error = 0;
	mac_t mac = NULL;

	if (attrs == NULL)
		return (mount(fspec, dir, flags, fstyp, dataptr, datalen));

	if (strncmp(attrs, "eag:", 4)) {
		/*
		 * Pretend to be a system call.
		 */
		setoserror(EINVAL);
		return (-1);
	}
	attrs = strdup(attrs + 4);
	if (equal = strchr(attrs, ','))
		*equal = '\0';
	if (equal = strchr(attrs, ' '))
		*equal = '\0';
	if (equal = strchr(attrs, '\t'))
		*equal = '\0';

	for (src = attrs, arg2 = NULL; src; src = colon) {
		if (colon = strchr(src, ':'))
			*colon++ = '\0';
		if (equal = strchr(src, '='))
			*equal++ = '\0';
		else {
			error = EINVAL;
			break;
		}

		if (!strcmp(src, MAC_MOUNT_IP)) {
			if (!(mac = mac_from_text(equal))) {
				error = EINVAL;
				break;
			}
			arg2 = sgi_eag_form_attr(MAC_MOUNT_IP,
						 mac_size(mac), mac, arg2);
			free(mac);
			continue;
		}
		if (!strcmp(src, MAC_MOUNT_DEFAULT)) {
			if (!(mac = mac_from_text(equal))) {
				error = EINVAL;
				break;
			}
			arg2 = sgi_eag_form_attr(MAC_MOUNT_DEFAULT,
						 mac_size(mac), mac, arg2);
			free(mac);
			continue;
		}
		error = EINVAL;
		break;
	}

	if (error) {
		setoserror(error);
		error = -1;
	}
	else {
		arg1.spec = fspec;
		arg1.dir = dir;
		arg1.flags = flags;
		arg1.fstype = fstyp;
		arg1.dataptr = dataptr;
		arg1.datalen = datalen;

		error = (int) syssgi(SGI_PLANGMOUNT, &arg1, arg2);
	}

	free(attrs);
	if (arg2)
		free(arg2);

	return (error);
}
