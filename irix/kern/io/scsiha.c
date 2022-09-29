#include <sys/types.h>
#include <sys/sema.h>
#include <sys/cred.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/scsi.h>
#include <sys/conf.h>
#include <sys/kabi.h>
#include <sys/iograph.h>
#include <sys/debug.h>
#include <sys/invent.h>

#define SB_CTLR(dev) minor(dev)

int scsiha_devflag = D_MP;

/* ARGSUSED */
int
scsiha_open(dev_t *devp, int flag, int otyp, cred_t *crp)
{
	return 0;
}

/* ARGSUSED */
int
scsiha_close(dev_t dev, int flag, int otyp, cred_t *crp)
{
	return 0;
}


/* ARGSUSED */
int
scsiha_ioctl(dev_t dev, int cmd, __psint_t arg, int mode, struct cred *crp, int *rvalp)
{
	struct scsi_ha_op	*ha_ioctl;
	int			 error;
	scsi_ctlr_info_t	*scsi_ctlr_info;
	
	if (!dev_is_vertex(dev))
		return ENXIO;

	scsi_ctlr_info = scsi_ctlr_info_get(dev);
	/*
	 * Generic build of controler alias
	 */
	if (cmd == SOP_MAKE_CTL_ALIAS) {
		dev_t sctl, to, dev_vhdl;
		int rc;
		int contr;
#define NUMOFCHARPERINT       10
		char buf[NUMOFCHARPERINT];

		dev_vhdl = hwgraph_connectpt_get(dev);
		if ((contr = device_controller_num_get(dev_vhdl)) >= 0) {
			/*
			 * Get the "/hw/scsi_ctlr" vertex
			 */
			if ((sctl = hwgraph_path_to_dev("/hw/" EDGE_LBL_SCSI_CTLR)) == NODEV) {
				rc = hwgraph_path_add(hwgraph_root,
						      EDGE_LBL_SCSI_CTLR,
						      &sctl);
				ASSERT(rc == GRAPH_SUCCESS); rc = rc;
			}
			sprintf(buf,"%d",contr);
			if (hwgraph_traverse(sctl,
					     buf,
					     &to) == GRAPH_NOT_FOUND) {
				vertex_hdl_t	scsi_ctlr_vhdl;

				scsi_ctlr_vhdl = hwgraph_connectpt_get(dev);
				rc = hwgraph_edge_add(sctl, 
						      scsi_ctlr_vhdl, buf);
				hwgraph_vertex_unref(scsi_ctlr_vhdl);
			} else
				hwgraph_vertex_unref(to);
			
			(void) SCI_IOCTL(scsi_ctlr_info)(SCI_CTLR_VHDL(scsi_ctlr_info),
					  cmd, (void *) (uintptr_t) contr);
		}
		hwgraph_vertex_unref(sctl);
		hwgraph_vertex_unref(dev_vhdl);	
		return (0);
	}

	ha_ioctl = kmem_zalloc(sizeof(*ha_ioctl), KM_SLEEP);

	if (cmd > 10)
	{
		if (copyin((void *) arg, ha_ioctl, sizeof(*ha_ioctl)))
		{
			error = EFAULT;
			goto ioctl_done;
		}

#if _MIPS_SIM == _MIPS_SIM_ABI64
		/*
		 * 32 bit user prog needs to have address shifted,
		 * since copyin() will load addr into upper half of
		 * sb_addr.
		 */
		if (!ABI_IS_IRIX5_64(get_current_abi()))
			ha_ioctl->sb_addr >>= 32;
#endif
	}

	error = SCI_IOCTL(scsi_ctlr_info)(SCI_CTLR_VHDL(scsi_ctlr_info),
					  cmd, ha_ioctl);
ioctl_done:
	kern_free(ha_ioctl);
	return error;
}
