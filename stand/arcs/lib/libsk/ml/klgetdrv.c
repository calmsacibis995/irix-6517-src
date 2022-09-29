#include <ctype.h>
#include <sys/types.h>
#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/promcfg.h>
#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <promgraph.h>
#include <pgdrv.h>
#include <arcs/folder.h>
#include <libsk.h>

extern graph_hdl_t 	prom_graph_hdl ;
extern vertex_hdl_t 	hw_vertex_hdl ;

int
get_dsk_part(char **remain)
{
	int	partnum = 0 ;
        char 	*p1, *p2 ;

        p1 = *remain ;
        if (!strncmp(p1, "partition/", 10)) {
                p2 = p1 + 10 ;
                if (isdigit(*(char *)(p2+1))) {
                        p1 = p2 + 2 ;
                        partnum = (*(char *)(p2+1) - '0') +
                                  ((*p2 - '0') * 10);
                }
                else if (isdigit(*(char *)p2)) {
                        p1 = p2 + 1 ;
                        partnum = *p2 - '0' ;
                }
        }

        *remain = p1 ;
        if (partnum > 15) {
                partnum = 0 ;
        }

	return partnum ;
}

#ifndef SN_PDI

prom_dev_info_t *
kl_get_dev_info(char *path, char **remain, IOBLOCK *iob)
{
	graph_error_t 		graph_err ;
	vertex_hdl_t 	dev_vhdl, ctlr_vhdl ;
        graph_edge_place_t      eplace = GRAPH_EDGE_PLACE_NONE ;
        char edge_name[32] ;
	prom_dev_info_t	*dev_info ;
	klinfo_t *kl_comp;
	COMPONENT 	* tmpac ;

	/*
	 * Look up the complete path. If partial path is
	 * is found we get error, but not GRAPH_NOT_FOUND.
	 */
        if ((graph_err = hwgraph_path_lookup(hw_vertex_hdl, path,
          			&dev_vhdl, remain)) != GRAPH_SUCCESS) {
		if (graph_err != GRAPH_NOT_FOUND) {
			return 0 ;
		}
	}

	/* We now have a handle. The rest of the path is in
	   remain. */

        if ((graph_err = graph_info_get_LBL(prom_graph_hdl, dev_vhdl,
             INFO_LBL_KLCFG_INFO, NULL,
		(arbitrary_info_t *)&kl_comp)) != GRAPH_SUCCESS) {
		return 0 ;
	}

	/* We have a handle that may be only a controller handle.
  	   This is not acceptable since the path should specify
	   a complete device path. */

	if (!(kl_comp->flags & KLINFO_DEVICE)) {
		return 0 ;
	}

	if ((graph_err = graph_info_get_LBL(prom_graph_hdl, 
	        dev_vhdl, INFO_LBL_DEV_INFO, 
		NULL, (arbitrary_info_t *)&dev_info))
		!= GRAPH_SUCCESS) {
		       return 0 ;
	}

	kl_comp = (klinfo_t *)(dev_info->kl_comp);
	iob->Unit       = kl_comp->physid ;
	iob->Controller = kl_comp->virtid ;

#if 0
	partnum = 0 ;
	if ((kl_comp->struct_type == KLSTRUCT_DISK) ||
	    (kl_comp->struct_type == KLSTRUCT_CDROM)) {
		p1 = *remain ;
		if (!strncmp(p1, "partition/", 10)) {
			p2 = p1 + 10 ;
			if (isdigit(*(char *)(p2+1))) {
				p1 = p2 + 2 ;
				partnum = (*(char *)(p2+1) - '0') + 
					  ((*p2 - '0') * 10);
			}
			else if (isdigit(*(char *)p2)) {
				p1 = p2 + 1 ;
				partnum = *p2 - '0' ;
			}
		}
		*remain = p1 ;
		if (partnum > 15) {
		    partnum = 0 ;
		}
	}
	iob->Partition = partnum ;
#endif

	iob->Partition = 0 ;

        if ((kl_comp->struct_type == KLSTRUCT_DISK) ||
            (kl_comp->struct_type == KLSTRUCT_CDROM))
		iob->Partition = get_dsk_part(remain) ;

	return (dev_info) ;
}
#endif /*!SN_PDI*/

#ifdef SN_PDI

extern vertex_hdl_t	snpdi_rvhdl ;

pd_dev_info_t *
snGetPathInfo(char *path, char **remain, IOBLOCK *iob)
{
	graph_error_t	ge ;
	klinfo_t	*k = NULL ;
	vertex_hdl_t	dv ;
	pd_dev_info_t 	*pdev ;

        ge = hwgraph_path_lookup(	snpdi_rvhdl, path,
                                	&dv, remain) ;
	if ((ge != GRAPH_SUCCESS) && (ge != GRAPH_NOT_FOUND)) {
		printf("No lookup, e = %d\n", ge) ;
                return NULL ;
        }
#if 0
	k = pg_get_lbl(dv, INFO_LBL_KLCFG_INFO) ;
	if (!k)
		return NULL ;
        if (!(k->flags & KLINFO_DEVICE))
		return NULL ;
#endif
	pdev = (pd_dev_info_t *) pg_get_lbl(dv, INFO_LBL_DEVICE) ;
	if (!pdev)
		return NULL ;

	/* XXX change this to getting info from pdev */

#if 0
        iob->Unit       = k->physid ;
        iob->Controller = k->virtid ;
        iob->Partition 	= get_dsk_part(k, remain) ;
#endif
        iob->Controller = pdev->pdev_pdp.pdp_ctrl ;
        iob->Unit 	= pdev->pdev_pdp.pdp_unit ;
        iob->Partition 	= get_dsk_part(remain) ;

        if (pdev->pdev_pdp.pdp_type & DEVICE_TYPE_BLOCK)
		if (path[strlen(path) -1] == '/')
			*remain = &path[strlen(path) -1] ;
	
	return pdev ;
}
#endif SN_PDI




