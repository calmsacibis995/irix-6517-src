#ident	"$Revision: 1.7 $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/signal.h"
#include "sys/errno.h"
#include "sys/debug.h"
#include "sys/sysmacros.h"
#include "sys/uio.h"
#include "sys/cred.h"
#include "sys/conf.h"
#include "sys/systm.h"
#include "sys/mload.h"

/* 
 * Simple illustrative loadable driver.  Main use is for testing mload support.
 */

char *mldtestmversion = M_VERSION;
int mldtestdevflag = D_MP;

/* Auxilliary information stored with the vertex */
#define MLDTEST_AUX_INFO "testing device driver"

/*
** mldtestinit is called before any other driver routine.
*/
int
mldtestinit()
{
	printf("mldtestinit called\n");
	return(0);
}

/*
** The system calls mldtestunload in order to see if it's OK to unload this
** driver.  The driver must insure that structures it uses have been freed.
** Driver global and static variables will be reinitialized to 0 when the
** driver is re-loaded.  If the driver is hwgraph-aware, it may choose to
** leave vertices in the hwgraph in order to take advantage of module 
** registration.
**
** If the driver has registered a hwgraph traverse routine, it must not 
** permit the unload to succeed.  (This may change in a future release.)
*/
int
mldtestunload(void)
{
	printf("mldtestunload called\n");
	return 0;
}


/*
** mldtestreg is called when the driver is registered.  Its main purpose
** is to add vertices to the hwgraph.  Don't try to set any global or static
** variables in this routine, becuase they'll get wiped out across driver 
** loads and unloads.  On the other hand, it's OK to inspect variables set
** by the "init" routine, since drivers are always initialized before they
** are registered.
*/
int
mldtestreg(void)
{

#ifdef DEBUG_MLDTEST
	arbitrary_info_t info_buf;
#endif /* DEBUG_MLDTEST */

	printf("mldtestreg called\n"); 

	if (hwgraph_char_device_add(GRAPH_VERTEX_NONE, "mldtest", "mldtest", NULL) == GRAPH_SUCCESS) {
		/* 
		 * Add the auxilliary info for this device driver here
		 */
		if (hwgraph_info_add_LBL(hwgraph_path_to_vertex("mldtest"),
					 "sgi_aux_info", 
					 (arbitrary_info_t) MLDTEST_AUX_INFO) 
		    == GRAPH_SUCCESS) {

#ifdef DEBUG_MLDTEST
			printf("label is added to the vertex");
			hwgraph_info_get_LBL(hwgraph_path_to_vertex
					     ("/hw/mldtest"),
					     "sgi_aux_info", 
					     &info_buf);
			printf("%s:%s\n", "/hw/mldtest", info_buf);
#endif /* DEBUG_MLDTEST */

			return(0);
		}
		else {
			return(1);
		}
	}
	else {
		return(1);
	}
}


/*
** mldtestunreg is called when the driver is unregistered.  Its purpose is
** to remove vertices from the hwgraph.  If the driver allows itself to be
** unregistered, it must not leave any vertices sitting in the hardware graph!
** In order to completely remove a vertex, there are several considerations:
**	-Disallow future opens by removing incoming edges (so there's no way
**	 to look up the vertex.
**	-Try to hwgraph_vertex_destroy the vertex.  If this operation fails,
**	 it's because some system module still refers to this vertex;  so, we
**	 cannot allow ourselves to be unregistered.
**
** Note that while we're in unregister code, the system doesn't allow anyone to
** enter our open code or other entry points.  Don't try to use or set any 
** driver-global or static variables in the unregister routine, becuase they'll
** get wiped out across driver loads and unloads.
*/
int
mldtestunreg(void)
{
	vertex_hdl_t mldtest_vhdl = GRAPH_VERTEX_NONE;
	graph_error_t rv;

	printf("mldtestunreg called\n");

	/* 
	 * Remove edges pointing into this vertex so future lookups will fail.
	 * This call might fail if we tried to unregister the driver earlier
	 * but couldn't because there were outstanding references to the vertex.
	 */
	rv = hwgraph_edge_remove(hwgraph_root, "mldtest", &mldtest_vhdl);

	if (rv != GRAPH_SUCCESS)
		return(-1);

	/* 
	 * We specified a non-NULL third parameter to hwgraph_edge_remove,
	 * so we just added an additional reference to the vertex.  Get rid of it.
	 * (The only reason we specified mldtest_vhdl was to figure out which
	 * vertex to destroy.)
	 */
	hwgraph_vertex_unref(mldtest_vhdl);

	/*
	 * Hopefully, the only reference left is the "creation reference"; so we can
	 * now destroy this vertex.
	 */
	if (hwgraph_vertex_destroy(mldtest_vhdl) == GRAPH_SUCCESS)
		return(0);
	else {
		/* Add back the device name for now, since unregistration failed. */
		hwgraph_edge_add(hwgraph_root, mldtest_vhdl, "mldtest");
		return(-1);
	}
}

/* ARGSUSED */
int
mldtestopen(dev_t *devp, int oflag, int otyp, cred_t *crp)
{ 
	printf("mldtestopen called\n");
	return 0;
}

/* ARGSUSED */
int
mldtestclose(dev_t devp, int oflag, int otyp, cred_t *crp)
{
	printf("mldtestclose called\n");
	return 0;
}

/* ARGSUSED */
int
mldtestread(dev_t dev, uio_t *uiop, cred_t *crp)
{
	printf("mldtestread called\n");
	uiop->uio_resid = 0;
	return 0;
}

/* ARGSUSED */
int
mldtestwrite(dev_t dev, uio_t *uiop, cred_t *crp)
{
	printf("mldtestwrite called\n");
	uiop->uio_resid = 0;
	return 0;
}


/* ARGSUSED */
int
mldtestioctl(dev_t dev, unsigned int cmd, caddr_t arg, int mode, cred_t *crp, int *rvalp)
{
	printf("mldtestioctl called\n");
	return 0;
}
