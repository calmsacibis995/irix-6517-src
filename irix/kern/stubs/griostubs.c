/*
 * Grio driver stubs.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/kmem.h>

zone_t *grio_buf_data_zone = (zone_t *)0;

void grio_iodone() { }

void griostrategy(struct buf *tlvb) {
	struct bdevsw *my_bdevsw;
	my_bdevsw = get_bdevsw(tlvb->b_edev);
	bdstrat( my_bdevsw, tlvb );
}

void griostrategy2(struct buf *bp) {
#if CELL_IRIX
	vnode_t	*vp;
	vp = bp->b_target->specvp;
	ASSERT(vp);
	VOP_STRATEGY(vp, bp);
#else
	struct bdevsw *my_bdevsw;
	my_bdevsw = bp->b_target->bdevsw;
	bdstrat(my_bdevsw, bp);
#endif
}

int grio_config() { return(ENOSYS); }

int grio_purge_vdisk_cache() { return(-1); }

int grio_monitor_io_start() { return(-1); }

int grio_monitor_io_end() { return(-1); }

int grio_remove_reservation_with_fp() { return(-1); }

int grio_io_is_guaranteed() { return -1; }
