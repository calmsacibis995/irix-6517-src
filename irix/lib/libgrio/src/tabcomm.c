#ident "$Header: /proj/irix6.5.7m/isms/irix/lib/libgrio/src/RCS/tabcomm.c,v 1.4 1997/03/14 00:24:41 kanoj Exp $"

#include <stdio.h>
#include <unistd.h>
#include <invent.h>
#include <string.h>
#include <bstring.h>
#include <grio.h>
#include <sys/scsi.h>
#include <sys/sysmacros.h>
#include <sys/bsd_types.h>
#include <sys/dkio.h>
#include <fcntl.h>
#include <sys/fs/xfs_inum.h>
#include <sys/uuid.h>
#include <sys/major.h>
#include <sys/utsname.h>

/*
 * Common routines for table access and manipulation.
 */
dev_info_t	unknown_dev_info = { 0, 0 };
int 		defaultoptiosize = 64;

/*
 * get_devclassptr()
 * 	Return the correct devclass table for the given
 *	optimal I/O size.
 *
 * RETURNS:
 *	a pointer to a dev_class table
 */
dev_class_t *
get_devclassptr(int iosize)
{
	dev_class_t *ptr;
	extern dev_class_t *devclasses64, *devclasses128; 
	extern dev_class_t *devclasses256, *devclasses512;

	if (iosize == -1) {
		printf("Internal error in get_devclassptr\n");
		exit(-1);
	}

	switch (iosize) {
		case 64:
			ptr = (dev_class_t *)(&devclasses64);
			break;
		case 128:
			ptr = (dev_class_t *)(&devclasses128);
			break;
		case 256:
			ptr = (dev_class_t *)(&devclasses256);
			break;
		case 512:
			ptr = (dev_class_t *)(&devclasses512);
			break;
		default:
			ptr = NULL;
			break;
	}
	return( ptr) ;
}


/*
 * get_dev_io_limits
 *      The routine does a table lookup of the given type of device
 *      and returns the dev_info_t structure corresponding to that device.
 *
 * RETURNS:
 *      a pointer to dev_info_t structure on success
 *      NULL on failure
 *
 * NOTE:
 *	ggd/cfg call this interface directly after setting the variable
 *	defaultoptiosize.
 */
dev_info_t *
get_dev_io_limits( int classnum, int typenum , int statenum )
{
        int i;
        dev_info_t  *devtp = NULL;
        dev_type_t  *devtypep = NULL;
        dev_class_t *devclassp = NULL;
	dev_class_t *devclasses;

	devclasses = GET_DEVCLASSES();

	if (devclasses == NULL) {
		return(&unknown_dev_info);
	}

        if (classnum) {
                devclassp = &devclasses[classnum];
                if (typenum) {
                        devtypep = &(devclassp->dev_type[typenum]);
                        if (statenum) {
                                for (i = 0; i < NUM_DEV_ITEMS; i++) {
                                        if (devtypep->dev_state[i].state_id ==
                                                                     statenum) {                                                devtp = &(devtypep->dev_state[i].dev_info);
                                                break;
                                        }
                                }
                        } else {
                                devtp = &(devtypep->dev_info);
                        }
                } else {
                        devtp = &(devclassp->dev_info);
                }
        }
        if (devtp == NULL) {
                devtp = &unknown_dev_info;
        }

        return( devtp );
}

int
grio_query_device( int classnum, int typenum , int statenum, int size )
{
	dev_info_t *dvinfo = NULL;

	defaultoptiosize = size;
	dvinfo = get_dev_io_limits(classnum, typenum, statenum);
	return(dvinfo->maxioops);
}
