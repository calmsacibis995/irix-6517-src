/**************************************************************************
 *									  *
 *		 Copyright (C) 1990-1993, Silicon Graphics, Inc		  *
 *									  *
 *  These coded instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"io/lio/xiocfg.c: $Revision: 1.2 $"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/mload.h>
#include <sys/kmem.h>
#include <sys/uio.h>
#include <sys/xtalk/xtalk.h>
#include <sys/xtalk/xwidget.h>
#include <ksys/ddmap.h>

#define NEW(ptr)	(ptr = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

char                   *xioba_mversion = M_VERSION;
int                     xioba_devflag = D_MP;

/* busy counts the reasons why we can not
 * currently unload this driver.
 */
unsigned		xioba_busy = 0;

typedef struct xioba_soft_s *xioba_soft_t;

#define	xioba_soft_get(v)	(xioba_soft_t)hwgraph_fastinfo_get(v)

struct xioba_soft_s {
    vertex_hdl_t            conn;
    vertex_hdl_t            vhdl;
};

void
xioba_init(void)
{
}

int
xioba_unload(void)
{
    if (xioba_busy)
	return -1;

    return 0;				/* always ok to unload */
}

int
xioba_reg(void)
{
    xwidget_driver_register(-1, -1, "xioba_", 0);
    return 0;
}

int
xioba_unreg(void)
{
    if (xioba_busy)
	return -1;

    xwidget_driver_unregister("xioba_");
    return 0;
}

int
xioba_attach(vertex_hdl_t conn)
{
    xioba_soft_t            soft;

    NEW(soft);
    soft->conn = conn;

    hwgraph_char_device_add(conn, "direct", "xioba_", &soft->vhdl);
    hwgraph_fastinfo_set(soft->vhdl, (arbitrary_info_t) soft);

    return 0;
}

int
xioba_detach(vertex_hdl_t conn)
{
    vertex_hdl_t            vhdl;
    xioba_soft_t            soft;

    if (GRAPH_SUCCESS !=
	hwgraph_edge_remove(conn, "direct", &vhdl))
	return -1;
    soft = xioba_soft_get(vhdl);
    if (!soft)
	return -1;
    hwgraph_fastinfo_set(vhdl, 0);
    hwgraph_vertex_destroy(vhdl);
    DEL(soft);
    return 0;
}

/*ARGSUSED */
int
xioba_open(dev_t *devp, int flag, int otyp, struct cred *crp)
{
    return 0;
}

/*ARGSUSED */
int
xioba_close(dev_t dev)
{
    return 0;
}

/*ARGSUSED */
int
xioba_read(dev_t dev, uio_t * uiop, cred_t *crp)
{
    return EOPNOTSUPP;
}

/*ARGSUSED */
int
xioba_write()
{
    return EOPNOTSUPP;
}

/*ARGSUSED*/
int
xioba_ioctl(dev_t dev, int cmd, void *uarg, int mode, cred_t *crp, int *rvalp)
{
    return *rvalp = ENOTTY;
}

/*ARGSUSED*/
int
xioba_map(dev_t dev, vhandl_t *vt,
	  off_t off, size_t len, uint_t prot)
{
    vertex_hdl_t	    vhdl = dev_to_vhdl(dev);
    xioba_soft_t	    soft = xioba_soft_get(vhdl);
    vertex_hdl_t	    conn = soft->conn;
    xtalk_piomap_t	    map;
    caddr_t		    kaddr;

    /* If we have to use a piomap, then we
     * will have to tuck the piomap handle
     * away where it can be handed to
     * piomap_free from xioba_unmap.
     */
    kaddr = (caddr_t) xtalk_pio_addr
	(conn, 0, off, len, &map, 0);

    if (kaddr == NULL)
	return EINVAL;

    /* Inform the system of the correct
     * kvaddr corresponding to the thing
     * that is being mapped.
     */
    v_mapphys(vt, kaddr, len);

    return 0;
}

/*ARGSUSED*/
int
xioba_unmap(dev_t dev, vhandl_t *vt)
{
    return (0);
}
