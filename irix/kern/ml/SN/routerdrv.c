#include <sys/types.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/immu.h>
#include <sys/kmem.h>
#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <sys/errno.h>
#include <sys/runq.h>
#include <sys/nodepda.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <ksys/sthread.h>
#include <ksys/hwg.h>
#include <sys/SN/router.h>
#include <sys/SN/vector.h>
#include <sys/SN/agent.h>
#include <sys/SN/slotnum.h>
#include <sys/SN/routerdrv.h>
#include <sys/nic.h>

#ifdef ROUTER_DEBUG
#define RPRF(x)         printf x
#define RCE_WARN        CE_PANIC
#else
#define RPRF(x)
#define RCE_WARN        CE_WARN
#endif

/****************************************************************************
 *
 * Router driver code
 *
 ****************************************************************************/

int routerdrv_devflag = D_MP;

int
routerdrv_attach(dev_t vertex)
{
	vertex_hdl_t router_dev;
	router_info_t *rip;
	hubstat_t *hsp;
	int rc;

	RPRF(("routerdrv_attach: Attaching %v\n", vertex));

	rc = hwgraph_char_device_add(vertex, EDGE_LBL_PERFMON, "routerdrv_",
				     &router_dev);
	if (rc != GRAPH_SUCCESS) {
		cmn_err(RCE_WARN, "routerdrv_attach: Can't add device %v, code %d",
				vertex, rc);
	}

	hwgfs_chmod(router_dev, 0644);

	rc = hwgraph_info_get_LBL(vertex, INFO_LBL_ROUTER_INFO,
		(arbitrary_info_t *)&rip);

	if (rc == GRAPH_SUCCESS) {
		rc = hwgraph_info_add_LBL(router_dev, INFO_LBL_MONDATA,
					  (arbitrary_info_t)rip);

		RPRF(("routerdrv_attach: Setting info %v to 0x%x\n",
		      router_dev, RTRDRV_ROUTER_DEVICE));

		device_info_set(router_dev,
				(void *)(__psint_t)RTRDRV_ROUTER_DEVICE);

		return 0;
	}

	rc = hwgraph_info_get_LBL(vertex, INFO_LBL_HUB_INFO,
		(arbitrary_info_t *)&hsp);

	if (rc == GRAPH_SUCCESS) {
		rc = hwgraph_info_add_LBL(router_dev, INFO_LBL_MONDATA,
					  (arbitrary_info_t)hsp);

		RPRF(("routerdrv_attach: Setting info %v to 0x%x\n",
		      router_dev, RTRDRV_HUB_DEVICE));

		device_info_set(router_dev,
				(void *)(__psint_t)RTRDRV_HUB_DEVICE);

		return 0;
	}

	cmn_err(RCE_WARN, "routerdrv_attach: Can't get label %v, code %d",
				vertex, rc);

	return 0;
}


/* ARGSUSED */
int
routerdrv_open(dev_t *devp, mode_t oflag, int otyp, cred_t *crp)
{
	if (!device_info_get(*devp))
		return ENODEV;
	else
		return 0;
}

/* ARGSUSED */
int
routerdrv_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
	return 0;
}


/* ARGSUSED */
int
routerdrv_ioctl(dev_t dev, int cmd, void *arg, int flag, struct cred *cr,
	     int *rvalp)
{
	/* REFERENCED */
	vertex_hdl_t vhdl;
	int error = 0;
	router_info_t *rip;
	hubstat_t *hsp;
	int dev_type;
	int rc;

	vhdl = dev_to_vhdl(dev);

	/*
	 * Check creds if we add anything that changes state.
	 */

	switch (cmd) {
		case ROUTERINFO_GETINFO:
			dev_type = (int)(__psint_t)device_info_get(dev);
			if (dev_type != RTRDRV_ROUTER_DEVICE)
				error = EINVAL;

			rc = hwgraph_info_get_LBL(dev, INFO_LBL_MONDATA,
				(arbitrary_info_t *)&rip);

			if ((rc != GRAPH_SUCCESS) || !rip) {
				error = EINVAL;
			} else if (copyout(rip, (void *)arg,
				    	   sizeof(router_info_t))) {
				error = EFAULT;
			}
			break;

		case HUBINFO_GETINFO:
			dev_type = (int)(__psint_t)device_info_get(dev);
			if (dev_type != RTRDRV_HUB_DEVICE)
				error = EINVAL;

			rc = hwgraph_info_get_LBL(dev, INFO_LBL_MONDATA,
				(arbitrary_info_t *)&hsp);

			if ((rc != GRAPH_SUCCESS) || !hsp) {
				error = EINVAL;
			} else if (copyout(hsp, (void *)arg,
				   	   sizeof(hubstat_t))) {
				error = EFAULT;
			}
			break;

		case GETSTRUCTSIZE:
			dev_type = (int)(__psint_t)device_info_get(dev);
			if (dev_type == RTRDRV_ROUTER_DEVICE)
				*rvalp = sizeof(router_info_t);
			else if (dev_type == RTRDRV_HUB_DEVICE)
				*rvalp = sizeof(hubstat_t);
			else
				error = EINVAL;
			break;

		default:
			error =  EINVAL;
			break;
	}

	return error;
}

