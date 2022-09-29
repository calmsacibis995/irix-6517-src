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
#include <sys/systeminfo.h>
#include <ksys/sthread.h>
#include <ksys/hwg.h>
#include <sys/SN/router.h>
#include <sys/SN/vector.h>
#include <sys/SN/agent.h>
#include <sys/SN/module.h>
#include <sys/SN/SN0/slotnum.h>
#include <sys/SN/SN0/sn0drv.h>
#include <sys/SN/SN0/hubstat.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/SN0/IP31.h>
#include <sys/nic.h>
#include <sys/hwperftypes.h>
#include <ksys/hwperf.h>
#include <ksys/elsc.h>
#include "sn0_private.h"

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


/* 128k of log + overhead. */
#define MAX_LOG_SIZE	(140 * 1024)

/* forward declarations */
void 	insert_log_sync(nasid_t nasid);
int 	get_prom_messages(cnodeid_t cnode, int all, char *buffer);
int 	rou_fence_op(sn0drv_fence_t *, router_info_t *) ;
int    	router_port_nic_info_get(router_info_t *rip, int port, nic_t *nic) ;
void 	router_port_map_get(router_info_t *, sn0drv_router_map_t *) ;
int	check_router(dev_t, router_info_t **, struct cred *, int *) ;
int	clear_promlog(cnodeid_t cnode, int init);

#define INFO_LBL_FENCE "_rou_fence"

int sn0drv_devflag = D_MP;

static char *log_sync = "Kernel log synchronized.\n";

/*
 * Input vertex must be one of:
 *
 *   /hw/module/.../hub
 *   /hw/module/.../router
 *   /hw/module/.../elsc
 */

int
sn0drv_attach(dev_t vertex)
{
	vertex_hdl_t router_dev;
	router_info_t *rip;
	void *check_elsc;
	hubstat_t *hsp;
	int rc;
	hubinfo_t hinfo;
	vertex_hdl_t node_vertex;

	RPRF(("sn0drv_attach: Attaching %v\n", vertex));

	/*
	 * Check if ELSC is being attached
	 */

	rc = hwgraph_info_get_LBL(vertex, INFO_LBL_ELSC,
				  (arbitrary_info_t *) &check_elsc);

	if (rc == GRAPH_SUCCESS) {
	    vertex_hdl_t elsc_dev;

	    RPRF(("sn0drv_attach: adding ELSC devices\n"));

	    /*
	     * NVRAM
	     */

	    rc = hwgraph_char_device_add(vertex, EDGE_LBL_NVRAM, "sn0drv_",
					 &elsc_dev);
	    if (rc != GRAPH_SUCCESS) {
		cmn_err(RCE_WARN,
			"sn0drv_attach: Can't add device %v, code %d",
			vertex, rc);
	    }

	    hwgfs_chmod(elsc_dev, 0644);

	    RPRF(("sn0drv_attach: Setting info %v to 0x%x\n",
		  elsc_dev, SN0DRV_ELSC_NVRAM_DEVICE));

	    device_info_set(elsc_dev,
			    (void *)(__psint_t)SN0DRV_ELSC_NVRAM_DEVICE);

	    /*
	     * Controller
	     */

	    rc = hwgraph_char_device_add(vertex, EDGE_LBL_CONTROLLER, "sn0drv_",
					 &elsc_dev);
	    if (rc != GRAPH_SUCCESS) {
		cmn_err(RCE_WARN,
			"sn0drv_attach: Can't add device %v, code %d",
			vertex, rc);
	    }

	    hwgfs_chmod(elsc_dev, 0644);

	    RPRF(("sn0drv_attach: Setting info %v to 0x%x\n",
		  elsc_dev, SN0DRV_ELSC_CONTROLLER_DEVICE));

	    device_info_set(elsc_dev,
			    (void *)(__psint_t)SN0DRV_ELSC_CONTROLLER_DEVICE);

	    return 0;
	}

	rc = hwgraph_char_device_add(vertex, EDGE_LBL_PERFMON, "sn0drv_",
				     &router_dev);
	if (rc != GRAPH_SUCCESS) {
		cmn_err(RCE_WARN,
			"sn0drv_attach: Can't add device %v, code %d",
			vertex, rc);
	}

	hwgfs_chmod(router_dev, 0644);

	/*
	 * Check if router is being attached
	 */

	rc = hwgraph_info_get_LBL(vertex, INFO_LBL_ROUTER_INFO,
		(arbitrary_info_t *)&rip);

	if (rc == GRAPH_SUCCESS) {
		rc = hwgraph_info_add_LBL(router_dev, INFO_LBL_MONDATA,
					  (arbitrary_info_t)rip);

		/* 
		 * Add another label for partition cfg 
		 * fence operations.
		 */
                rc = hwgraph_info_add_LBL(router_dev, INFO_LBL_FENCE,
                                          (arbitrary_info_t)rip);

		RPRF(("sn0drv_attach: Setting info %v to 0x%x\n",
		      router_dev, SN0DRV_ROUTER_DEVICE));

		device_info_set(router_dev,
				(void *)(__psint_t)SN0DRV_ROUTER_DEVICE);

		return 0;
	}

	/*
	 * Hub is being attached
	 */

	node_vertex = device_master_get(vertex);
	if (node_vertex == GRAPH_VERTEX_NONE)
	    rc = hwgraph_info_get_LBL(vertex, INFO_LBL_NODE_INFO, 
				      (arbitrary_info_t *)&hinfo);
	else
	    rc = hwgraph_info_get_LBL(node_vertex, INFO_LBL_NODE_INFO, 
				      (arbitrary_info_t *)&hinfo);

	if (rc == GRAPH_SUCCESS) {
		rc = hwgraph_info_get_LBL(vertex, INFO_LBL_HUB_INFO,
					  (arbitrary_info_t *)&hsp);

		if (rc == GRAPH_SUCCESS) {
			rc = hwgraph_info_add_LBL(router_dev, INFO_LBL_MONDATA,
						  (arbitrary_info_t)hsp);
		}
		
		rc = hwgraph_info_add_LBL(router_dev, INFO_LBL_MDPERF_DATA,
					  (arbitrary_info_t)NODEPDA_MDP_MON(hinfo->h_cnodeid));
		
		RPRF(("sn0drv_attach: Setting info %v to 0x%x\n",
		      router_dev, SN0DRV_HUB_DEVICE));

		device_info_set(router_dev,
				(void *)(__psint_t)SN0DRV_HUB_DEVICE);

		return 0;
	}

	cmn_err(RCE_WARN, "sn0drv_attach: Can't get label %v, code %d",
				vertex, rc);

	return 0;
}


/* ARGSUSED */
int
sn0drv_open(dev_t *devp, mode_t oflag, int otyp, cred_t *crp)
{
	if (!device_info_get(*devp))
		return ENODEV;
	else
		return 0;
}

/* ARGSUSED */
int
sn0drv_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
	return 0;
}


/* ARGSUSED */
int
sn0drv_ioctl(dev_t dev, int cmd, void *arg, int flag, struct cred *cr,
	     int *rvalp)
{
	/* REFERENCED */
	vertex_hdl_t vhdl;
	int error = 0;
	router_info_t *rip;
	hubstat_t *hsp;
	int dev_type;
	int rc;
	__int64_t hist_type;
	char    *hub_nic_info;
	vertex_hdl_t	hubv;
	vhdl = dev_to_vhdl(dev);

	/*
	 * Check creds if we add anything that changes state.
	 */

	switch (cmd) {
		case SN0DRV_GET_ROUTERINFO:
			dev_type = (int)(__psint_t)device_info_get(dev);
			if (dev_type != SN0DRV_ROUTER_DEVICE)
				error = EINVAL;

			rc = hwgraph_info_get_LBL(dev, INFO_LBL_MONDATA,
				(arbitrary_info_t *)&rip);

			if ((rc != GRAPH_SUCCESS) || !rip) {
				error = EINVAL;
			} else if (copyout(rip, arg, sizeof(router_info_t))) {
				error = EFAULT;
			}
			break;

		case SN0DRV_GET_HUBINFO:
			dev_type = (int)(__psint_t)device_info_get(dev);
			if (dev_type != SN0DRV_HUB_DEVICE)
				error = EINVAL;

			rc = hwgraph_info_get_LBL(dev, INFO_LBL_MONDATA,
				(arbitrary_info_t *)&hsp);

			if ((rc != GRAPH_SUCCESS) || !hsp) {
				error = EINVAL;
			} else if (copyout(hsp, arg, sizeof(hubstat_t))) {
				error = EFAULT;
			}
			break;

		case SN0DRV_SET_HISTOGRAM_TYPE:
			hist_type = (__uint64_t)arg;
			dev_type = (int)(__psint_t)device_info_get(dev);

			if (dev_type != SN0DRV_ROUTER_DEVICE)
				error = EINVAL;
			/* non-debug kernels can only get utilization */
			if (!kdebug && (hist_type != RPPARM_HISTSEL_UTIL))
			    return EINVAL;

			rc = hwgraph_info_get_LBL(dev, INFO_LBL_MONDATA,
				(arbitrary_info_t *)&rip);
			if ((rc != GRAPH_SUCCESS) || !rip)  
				error = EINVAL;
			else {
				if ((hist_type < RPPARM_HISTSEL_AGE) ||
				    (hist_type > RPPARM_HISTSEL_DAMQ)) 
					return EINVAL;
				error = router_hist_reselect(rip, hist_type);
			}
			break;

		case SN0DRV_GET_INFOSIZE:
			dev_type = (int)(__psint_t)device_info_get(dev);
			if (dev_type == SN0DRV_ROUTER_DEVICE)
				*rvalp = sizeof(router_info_t);
			else if (dev_type == SN0DRV_HUB_DEVICE)
				*rvalp = sizeof(hubstat_t);
			else
				error = EINVAL;
			break;

		case SN0DRV_GET_FLASHLOGSIZE:
			*rvalp = MAX_LOG_SIZE;
			break;			

		case SN0DRV_GET_FLASHLOGDATA:
		case SN0DRV_GET_FLASHLOGALL:
			{
				char *buffer;
				int bytes;
				vertex_hdl_t master;
				vertex_hdl_t parent_node;
				cnodeid_t cnode;

				dev_type = (int)(__psint_t)device_info_get(dev);
				if (dev_type != SN0DRV_HUB_DEVICE)
					error = EINVAL;

				master = hwgraph_connectpt_get(dev);
				parent_node = hwgraph_connectpt_get(master);

				cnode = nodevertex_to_cnodeid(parent_node);

				if (cnode == INVALID_CNODEID) {
					error = EINVAL;
					break;
				}

				buffer = kmem_alloc(MAX_LOG_SIZE, 0);
				bytes = get_prom_messages(cnode,
					  (cmd == SN0DRV_GET_FLASHLOGALL),
					  buffer);
				if (copyout(buffer, arg, bytes)) {
					error = EFAULT;
				}

				kmem_free(buffer, MAX_LOG_SIZE);

				*rvalp = bytes;
			}
			break;

		case SN0DRV_CLEAR_LOG:
		case SN0DRV_INIT_LOG:
			{
				vertex_hdl_t master;
				vertex_hdl_t parent_node;
				cnodeid_t cnode;

				dev_type = (int)(__psint_t)device_info_get(dev);
				if (dev_type != SN0DRV_HUB_DEVICE)
					error = EINVAL;

				master = hwgraph_connectpt_get(dev);
				parent_node = hwgraph_connectpt_get(master);

				cnode = nodevertex_to_cnodeid(parent_node);

				if (cnode == INVALID_CNODEID) {
					error = EINVAL;
					break;
				}

				*rvalp = clear_promlog(cnode,
					(cmd == SN0DRV_INIT_LOG));
			}
			break;

		case SN0DRV_SET_FLASHSYNC:
			{
				vertex_hdl_t master;
				vertex_hdl_t parent_node;
				cnodeid_t cnode;
				nasid_t nasid;

				/* Make sure we're root here. */
				if (cr->cr_uid != 0) {
					error = EPERM;
					break;
				}

				dev_type = (int)(__psint_t)device_info_get(dev);
				if (dev_type != SN0DRV_HUB_DEVICE)
					error = EINVAL;

				master = hwgraph_connectpt_get(dev);
				parent_node = hwgraph_connectpt_get(master);

				cnode = nodevertex_to_cnodeid(parent_node);

				if (cnode == INVALID_CNODEID) {
					error = EINVAL;
					break;
				}

				nasid = COMPACT_TO_NASID_NODEID(cnode);

				if (nasid == INVALID_NASID) {
					error = EINVAL;
					break;
				}

				insert_log_sync(nasid);
			}
			break;

		case SN0DRV_MDPERF_ENABLE: 
		{
			md_perf_control_t ctrl;
			md_perf_monitor_t *mdp;

			dev_type = (int)(__psint_t)device_info_get(dev);
			
			if (!_CAP_ABLE(CAP_DEVICE_MGT))
			    return EPERM;

			if (dev_type != SN0DRV_HUB_DEVICE)
			    return EINVAL;
			
			if (copyin(arg, &ctrl, sizeof(md_perf_control_t)))
			    return EFAULT;

			rc = hwgraph_info_get_LBL(dev, INFO_LBL_MDPERF_DATA,
						  (arbitrary_info_t *)&mdp);
			if (rc != GRAPH_SUCCESS)
			    return EINVAL;
			
			return md_perf_node_enable(&ctrl, mdp->mpm_cnode, rvalp);
		}
		case SN0DRV_MDPERF_DISABLE:
		{
			md_perf_monitor_t *mdp;
			dev_type = (int)(__psint_t)device_info_get(dev);

			if (!_CAP_ABLE(CAP_DEVICE_MGT))
			    return EPERM;
			
			if (dev_type != SN0DRV_HUB_DEVICE)
			    return EINVAL;
			
			rc = hwgraph_info_get_LBL(dev, INFO_LBL_MDPERF_DATA,
						  (arbitrary_info_t *)&mdp);
			if (rc != GRAPH_SUCCESS)
			    return EINVAL;
			
			return md_perf_node_disable(mdp->mpm_cnode);
		}
		case SN0DRV_MDPERF_GET_COUNT:
		{
			md_perf_monitor_t *mdp;
			md_perf_values_t val;

			dev_type = (int)(__psint_t)device_info_get(dev);
			if (dev_type != SN0DRV_HUB_DEVICE)
			    error = EINVAL;

			rc = hwgraph_info_get_LBL(dev, INFO_LBL_MDPERF_DATA,
						  (arbitrary_info_t *)&mdp);
			if (rc != GRAPH_SUCCESS)
			    return EINVAL;
			
			error = md_perf_get_node_count(mdp->mpm_cnode, &val, 
						       rvalp);
			if (error) return error;

			/* We don't copy the entire structure for
                           backward compatibilty */
			if (copyout(val.mpv_count, arg, 
				    sizeof(md_perf_reg_t)*MD_PERF_SETS*MD_PERF_COUNTERS))
			  error = EFAULT;
			
			break;
		}
			
		case SN0DRV_MDPERF_GET_CTRL:
		{
			md_perf_monitor_t *mdp;
			md_perf_control_t ctrl;

			dev_type = (int)(__psint_t)device_info_get(dev);
			if (dev_type != SN0DRV_HUB_DEVICE)
			    error = EINVAL;

			rc = hwgraph_info_get_LBL(dev, INFO_LBL_MDPERF_DATA,
						  (arbitrary_info_t *)&mdp);
			if (rc != GRAPH_SUCCESS)
			    return EINVAL;
			
			error = md_perf_get_node_ctrl(mdp->mpm_cnode, &ctrl, 
						      rvalp);
			if (error) return error;

			if (copyout(&ctrl, arg, sizeof(md_perf_control_t)))
			    error = EFAULT;
			break;
		}

		case SN0DRV_GET_SBEINFO:
		{
			vertex_hdl_t master;
			vertex_hdl_t parent_node;
			cnodeid_t cnode;
		        int bank_counts[8], i;

			dev_type = (int)(__psint_t)device_info_get(dev);
			if (dev_type != SN0DRV_HUB_DEVICE)
				error = EINVAL;

			if (cr->cr_uid != 0) {
				error = EPERM;
				break;
			}

			master = hwgraph_connectpt_get(dev);
			parent_node = hwgraph_connectpt_get(master);
			cnode = nodevertex_to_cnodeid(parent_node);

			if (cnode == INVALID_CNODEID) {
				error = EINVAL;
				break;
			}

			i = 8;

			memerror_get_stats(cnode, bank_counts, &i);

			while (i < 8)
				bank_counts[i++] = -1;

			if (copyout(bank_counts, arg, sizeof(bank_counts)))
				error = EFAULT;

			break;
		}

		case SN0DRV_ELSC_NVRAM:
		{
			cpu_cookie_t	was_running;
			vertex_hdl_t	elsc_vhdl;
			vertex_hdl_t	mod_vhdl;
			sn0drv_nvop_t	nvop;
			module_t       *m;
			int		s;

			dev_type = (int)(__psint_t)device_info_get(dev);
			if (dev_type != SN0DRV_ELSC_NVRAM_DEVICE) {
				error = EINVAL;
				break;
			}

			if (cr->cr_uid != 0) {
				error = EPERM;
				break;
			}

			if (copyin(arg, &nvop, sizeof (nvop))) {
				error = EFAULT;
				break;
			}

			elsc_vhdl = hwgraph_connectpt_get(dev);
			ASSERT(elsc_vhdl);

			mod_vhdl = hwgraph_connectpt_get(elsc_vhdl);
			ASSERT(mod_vhdl);

			m = (module_t *) hwgraph_fastinfo_get(mod_vhdl);
			ASSERT(m);

			was_running = setmustrun(master_procid);
			s = mutex_spinlock(&m->elsclock);

			if (nvop.flags & SN0DRV_NVOP_READ)
				rc = elsc_nvram_read(&m->elsc, nvop.addr,
						     &nvop.data, 1);
			else if (nvop.flags & SN0DRV_NVOP_WRITE)
				rc = elsc_nvram_write(&m->elsc, nvop.addr,
						      &nvop.data, 1);
			else
				rc = -1;

			mutex_spinunlock(&m->elsclock, s);
			restoremustrun(was_running);

			if (rc) {
				error = EIO;
				break;
			}

			if (copyout(&nvop, arg, sizeof (nvop)))
				error = EFAULT;

			break;
		}

                case SN0DRV_ROU_FENCE_OP:
                {
                        cpu_cookie_t    	was_running;
                        sn0drv_fence_t   	fi ;

                        dev_type = (int)(__psint_t)device_info_get(dev);
                        if (dev_type != SN0DRV_ROUTER_DEVICE) {
                                error = EINVAL;
				break ;
			}

                        if (cr->cr_uid != 0) {
                                error = EPERM;
                                break;
                        }

                        rc = hwgraph_info_get_LBL(dev, INFO_LBL_FENCE,
                                (arbitrary_info_t *)&rip);

                        if ((rc != GRAPH_SUCCESS) || !rip) {
                                error = EINVAL;
                                break ;
                        }

                        if (copyin(arg, &fi, sizeof (fi))) {
                                error = EFAULT;
                                break;
                        }

			was_running = setmustrun(cnodetocpu(rip->ri_cnode));

                        rc = rou_fence_op(&fi, rip) ;

			restoremustrun(was_running);

                        if (rc < 0) {
                                error = EIO;
                                break;
                        }

                        if (copyout(&fi, arg, sizeof (fi)))
                                error = EFAULT;

                        break;
                }

		case SN0DRV_ROU_PORT_MAP:
		{
			sn0drv_router_map_t map ;

			if (check_router(dev, &rip, cr, &error) < 0)
				break ;

			router_port_map_get(rip, &map) ;

			if (copyout(&map, arg, sizeof(map)) < 0)
				error = EFAULT ;
				
			break ;
		}

		case SN0DRV_ROU_NIC_GET:
		{
			nic_t		nic ;
			char 		*nicp ;
			vertex_hdl_t	rou_vhdl ;
			char 		*p ;

			if (check_router(dev, &rip, cr, &error) < 0)
				break ;

			rou_vhdl = hwgraph_connectpt_get(dev);

        		rc = hwgraph_info_get_LBL(rou_vhdl, "_nic",
                                (arbitrary_info_t *)&nicp);

        		if ((rc != GRAPH_SUCCESS) || !(nicp)) {
                		error = EINVAL;
                		break ;
        		}

			p = strstr(nicp, "Laser:") ;
			if (p) {
				p+= 6 ;
				nic = strtoull(p, NULL, 16) ;
			}

			if (copyout(&nic, arg, sizeof(nic)) < 0)
				error = EFAULT ;
				
			break ;
		}

                case SN0DRV_ROU_PORT_NIC_GET:
		/*
		 * Return: EIO if port disabled
		 *		  unable to do a setmustrun
		 *		  nic read fails
		 *		  others ...
		 *	   0   if hub is present.
		 */
                {
                        nic_t           nic = 0 ;
			int		port ;

                        if (check_router(dev, &rip, cr, &error) < 0)
                                break ;

			if (copyin(arg, &nic, sizeof(nic_t)) < 0) {
				error = EFAULT ;
				break ;
			}

			port = nic ;
                        if (router_port_nic_info_get(rip, port, &nic) < 0) {
				error = EIO ;
				break ;
			}

                        if (copyout(&nic, arg, sizeof(nic_t)) < 0)
                                error = EFAULT;

                        break;
                }

		case SN0DRV_ELSC_COMMAND:
		{
			cpu_cookie_t	was_running;
			vertex_hdl_t	elsc_vhdl;
			vertex_hdl_t	mod_vhdl;
			module_t       *m;
			int		s;

			dev_type = (int)(__psint_t)device_info_get(dev);
			if (dev_type != SN0DRV_ELSC_CONTROLLER_DEVICE) {
				error = EINVAL;
				break;
			}

			if (cr->cr_uid != 0) {
				error = EPERM;
				break;
			}

			elsc_vhdl = hwgraph_connectpt_get(dev);
			ASSERT(elsc_vhdl);

			mod_vhdl = hwgraph_connectpt_get(elsc_vhdl);
			ASSERT(mod_vhdl);

			m = (module_t *) hwgraph_fastinfo_get(mod_vhdl);
			ASSERT(m);

			was_running = setmustrun(master_procid);
			s = mutex_spinlock(&m->elsclock);

			if (copyin(arg, m->elsc.cmd, SN0DRV_CMD_BUF_LEN))
				error = EFAULT;
			else if (elsc_command(&m->elsc, 0))
				error = EIO;
			else if (copyout(m->elsc.resp, arg,
					 SN0DRV_CMD_BUF_LEN))
				error = EFAULT;

			mutex_spinunlock(&m->elsclock, s);
			restoremustrun(was_running);

			break;
		}

		case SN0DRV_GET_PIMM_PSC:
			dev_type = (int)(__psint_t)device_info_get(dev);
			if (dev_type != SN0DRV_HUB_DEVICE)
				error = EINVAL;

			rc = hwgraph_info_get_LBL(dev, INFO_LBL_MONDATA,
				(arbitrary_info_t *)&hsp);

			if ((rc != GRAPH_SUCCESS) || !hsp) {
				error = EINVAL;
			} else
			{
				hubv = cnodeid_to_vertex(hsp->hs_cnode);
        			hub_nic_info = nic_hub_vertex_info(hubv);
				if (!hub_nic_info)
				{
					*rvalp = PIMM_NOT_PRESENT;
				}
        			else if (!strstr(hub_nic_info, "IP31"))
				{
					*rvalp = PIMM_NOT_PRESENT;
				}
				else
					*rvalp = PIMM_PSC(hsp->hs_nasid);
			}
			break;

		default:
			error =  EINVAL;
			break;
	}

	return error;
}

int
clear_promlog(cnodeid_t cnode, int init)
{
	promlog_t       l;
	nasid_t         nasid = COMPACT_TO_NASID_NODEID(cnode);

	if (ip27log_setup(&l, nasid,
		(init ? IP27LOG_DO_INIT : IP27LOG_DO_CLEAR)) < 0)
		return 1;

	return 0;
}

int
get_prom_messages(cnodeid_t cnode, int all, char *buffer)
{
	promlog_t l;
	int r;
	vertex_hdl_t node_vertex = NODEPDA(cnode)->node_vertex;
	char key[16];
	char value[PROMLOG_VALUE_MAX];
	nasid_t nasid = COMPACT_TO_NASID_NODEID(cnode);
	int index = 0;

	buffer[0] = '\0';

	if (ip27log_setup(&l, nasid, 0) < 0) {
		sprintf(buffer, "Flash log error: %v", node_vertex);
		return strlen(buffer);
	}

	if (all) {
		r = promlog_first(&l, PROMLOG_TYPE_LOG);

	} else {

		r = promlog_last(&l);

		if (r < 0) {
			return 1;
		}

		/*
		 * First back up to the last entry we synched.
		 */
		while (r >= 0) {
			if ((r = promlog_prev(&l, PROMLOG_TYPE_LOG)) < 0) {
				break;
			}

			if ((r = promlog_get(&l, key, value)) < 0) {
				break;
			}

			if (!strncmp(value, log_sync, 15)) {
				break;
			}

		}

		promlog_next(&l, PROMLOG_TYPE_LOG);

	}

	if ((index + PROMLOG_KEY_MAX + PROMLOG_VALUE_MAX + 20) > MAX_LOG_SIZE) {
		cmn_err(CE_NOTE, "Flash log is too long: %v", node_vertex);
		return 1;
	}

	while ((r = promlog_get(&l, key, value)) >= 0) {
		sprintf(buffer + index, "%c %s: %s",
			'A' + l.ent.cpu_num, key, value);
		index += strlen(buffer + index);
		promlog_next(&l, PROMLOG_TYPE_LOG);
	}

	return index;
}


void
insert_log_sync(nasid_t nasid)
{
	ip27log_printf_nasid(nasid, IP27LOG_INFO, log_sync);
}

#define ROUTER_OP_READ	1
#define ROUTER_OP_WRITE	2

int
check_router(dev_t dev, router_info_t **ripp, struct cred *cr, int *errp)
{
	int 	dev_type ;
	int 	rc ;

        dev_type = (int)(__psint_t)device_info_get(dev);
        if (dev_type != SN0DRV_ROUTER_DEVICE) {
                *errp = EINVAL;
                return -1 ;
        }

        if (cr->cr_uid != 0) {
                *errp = EPERM;
                return -1;
        }

        rc = hwgraph_info_get_LBL(dev, INFO_LBL_FENCE,
                                (arbitrary_info_t *)ripp);

        if ((rc != GRAPH_SUCCESS) || !(*ripp)) {
                *errp = EINVAL;
                return -1 ;
        }

	return 0 ;
}

int
do_reg_op(int flag, router_info_t *rip, int reg, router_reg_t *val)
{
	int 	rc = 0 ;

	if (flag == ROUTER_OP_READ) {
		rc = router_reg_read(rip, reg, val) ;
		if (rc) {
			printf("router fence read failed: %s, err %d <%s>\n",
				rip->ri_name, rc, net_errmsg(rc)) ;
			return rc ;
		}
	}
	else if (flag == ROUTER_OP_WRITE) {
		rc = router_reg_write(rip, reg, *val) ;
		if (rc) {
			printf("router fence write failed: %s, err %d <%s>\n",
				rip->ri_name, rc, net_errmsg(rc)) ;
			return rc ;
		}
	}
	return rc ;
}

int
do_router_op(rou_regop_t *regop, router_info_t *rip, int reg)
{
	router_reg_t	tmp ;
	int		rc  ;

	switch (regop->op) {
		case FENCE_READ:
			rc = do_reg_op(ROUTER_OP_READ, rip, reg, &regop->reg) ;
			break ;

		case FENCE_DWRITE:
			rc = do_reg_op(ROUTER_OP_WRITE, rip, reg, &regop->reg) ;
			break ;

		case FENCE_AND_WRITE:
			rc = do_reg_op(ROUTER_OP_READ, rip, reg, &tmp) ;
			tmp &= regop->reg ;
			rc = do_reg_op(ROUTER_OP_WRITE, rip, reg, &tmp) ;
			regop->reg = tmp ;
			break ;

		case FENCE_OR_WRITE:
			rc = do_reg_op(ROUTER_OP_READ, rip, reg, &tmp) ;
			tmp |= regop->reg ;
			rc = do_reg_op(ROUTER_OP_WRITE, rip, reg, &tmp) ;
			regop->reg = tmp ;
			break ;

		default: 
			rc = 0 ;
			break ;
	}
	return rc ;
}

int
rou_fence_op(sn0drv_fence_t *req, router_info_t *rip) 
{
        int             port ;
	int		rc   ;

	rc = do_router_op(&req->local_blk, rip, RR_PROT_CONF) ;
	if (rc < 0) goto done ;
        for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
                if (!(rip->ri_portmask & (1 << (port - 1)))) {
                        continue ;
                }

		rc = do_router_op(&req->ports[port-1], 
					rip, RR_RESET_MASK(port)) ;
		if (rc < 0) goto done ;
	}

done:
        return rc ;
}

void
router_port_map_get(router_info_t *rip, sn0drv_router_map_t *map)
{
        int             port ;
        cpu_cookie_t    c    ;
	router_reg_t	reg  ;
	net_vec_t	vector ;

        for (port = 1; port <= MAX_ROUTER_PORTS; port++)
		map->type[port-1] = -1 ;

        c = setmustrun(cnodetocpu(rip->ri_cnode)) ;
        if (cnodeid() != rip->ri_cnode)
		goto pmap_done ;

        for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
                if (!(rip->ri_portmask & (1 << (port - 1))))
                        continue ;

        	vector = vector_concat(rip->ri_vector, port);

        	if (hub_vector_read(	rip->ri_cnode, vector,
					rip->ri_writeid, RR_STATUS_REV_ID, 
					&reg) < 0) {
                        continue ;
                }

        	map->type[port-1] = reg & RSRI_CHIPID_MASK ;
	}

pmap_done:
	restoremustrun(c) ;

	return ;
}

/*
 * Returns : 	-1 if port is disabled, 
 * 		if it is not a ROUTER
 *           	or setmustrun or read nic 
 * 		did not succeed.
 */

int
router_port_nic_info_get(router_info_t *rip, int port, nic_t *nicp)
{
        net_vec_t               vector;
        cpu_cookie_t            c;
	router_reg_t		reg ;
	int			rv = -1 ;

        if (!(rip->ri_portmask & (1 << (port - 1))))
		return -1 ;

	c = setmustrun(cnodetocpu(rip->ri_cnode)) ;
	if (cnodeid() != rip->ri_cnode)
		goto pnic_done ;

        vector = vector_concat(rip->ri_vector, port);

        if (hub_vector_read(    rip->ri_cnode, vector,
                                rip->ri_writeid, RR_STATUS_REV_ID,
                                &reg) < 0) {
		goto pnic_done ;
        }

        if ((reg & RSRI_CHIPID_MASK) != 1) /* not a ROUTER */
		goto pnic_done ;

        if (router_nic_get(vector, nicp) < 0)
		goto pnic_done ;

	rv = 0 ;

pnic_done:
	restoremustrun(c) ;

        return(rv);
}


