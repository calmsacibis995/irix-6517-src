/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.3 $"

/* 
 * pglib.c - some library routines for building hwgraph in prom.
 */

#include <sys/types.h>
#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <promgraph.h>
#include <libsc.h>
#include <pgdrv.h>

#ifdef SN_PDI

extern graph_hdl_t      pg_hdl 		;
extern vertex_hdl_t	snpdi_rvhdl 	;

#define REPORT_ERR	1

/* #define PG_DEBUG */
int     pg_debug;

void 
pg_create_ghdl(graph_hdl_t *ghdl, char *name)
{
	graph_attr_t 	ga ;
	graph_error_t	ge ;

	bzero(&ga, sizeof(ga)) ;
        ga.ga_name = name ;
        ga.ga_separator = '/' ;
        ga.ga_num_index = 64 ;
        ga.ga_reserved_places = 32 ;

	ge = graph_create(&ga, ghdl, 0) ;
	if (ge != GRAPH_SUCCESS) {
		printf("PROM HW GRAPH create err %d\n", ge) ;
		return ;
	}
}

void  	*
pg_get_lbl(vertex_hdl_t vhdl, char *label)
{
	graph_error_t           gerr ;
	void 			*k ;

        gerr = graph_info_get_LBL(pg_hdl, vhdl,
                                       label, NULL,
                                       (arbitrary_info_t *)&k) ;
	if (gerr == GRAPH_SUCCESS) {
		return k ;
	}
	return NULL ;

}

int
pg_add_lbl(vertex_hdl_t vhdl, char *label, void *info)
{
	graph_error_t gerr ;

	gerr = graph_info_add_LBL(	pg_hdl, vhdl,
                              		label, NULL,
                              		(arbitrary_info_t) info) ;
	return gerr ;
}

vertex_hdl_t
pg_edge_add(vertex_hdl_t src, vertex_hdl_t dst, char *ename)
{
	graph_error_t       graph_err ;

	graph_err = graph_edge_add(pg_hdl, src, dst, ename) ;
	if (graph_err != GRAPH_SUCCESS) {
        	printf("pg_edge_add %s err %d \n", ename, graph_err) ;
        	return GRAPH_VERTEX_NONE ;
	}
	return dst ;
}

void
pg_edge_add_path(char *path, vertex_hdl_t dst_v, char *new_edge)
{
	char                *remain = NULL ;
	vertex_hdl_t        src_v = GRAPH_VERTEX_NONE ;

	if ((!path) || (!new_edge) || (!(*path)) || (!(*new_edge)))
		return ;

	hwgraph_path_lookup(snpdi_rvhdl, path, &src_v, &remain) ;
	if (src_v == GRAPH_VERTEX_NONE)
		return ;

	pg_edge_add(src_v, dst_v, new_edge) ;
}

/*
 * Create a set of nodes and link them up with the given
 * path name.
 * If errflg is set report any error, else if an edge already
 * exists, dont report error but return the existing edge as 
 * if it was just now created.
 */

vertex_hdl_t
create_graph_path(vertex_hdl_t start_vhdl, char *path, int errflg)
{
	vertex_hdl_t 	v1, v2 ;
	graph_error_t	graph_err ;
	char		tmp_buf[32], *next ;

	if (!(*path)) 
		return GRAPH_VERTEX_NONE ; /* No path */

	v2 = start_vhdl ;

	if (*path == '/')
		path++ ;

	PG_DPRINT("crgfpth: path is %s\n", path);
	do {

		if (v2 == GRAPH_VERTEX_NONE)
	    		return GRAPH_VERTEX_NONE ;

		/* get the next component in path */

		if ((next = index(path, '/'))!=0) {
	    		strncpy(tmp_buf, path, next-path) ;
	    		tmp_buf[next-path] = 0 ;
	    		path = next+1 ;
		}
		else 
	    		strcpy(tmp_buf, path) ;

		PG_DPRINT("crgfpth: subpath is %s\n", tmp_buf);		

		/* check if the edge already exists */

        	graph_err = graph_edge_get(	pg_hdl,
                                		v2, tmp_buf, &v1) ; 
		if (graph_err != GRAPH_SUCCESS) {
            		graph_err = graph_vertex_create(pg_hdl, &v1) ;
            		if (graph_err != GRAPH_SUCCESS) {
                		printf("crgfpth: %s create err %d\n", 
					path, graph_err) ;
                		return GRAPH_VERTEX_NONE ;
            		}

	    		/* link new vertex to parent vhdl */

	    		v1 = pg_edge_add(v2, v1, tmp_buf) ;
		}
		v2 = v1 ;	/* the new vhdl is now the parent vhdl */
    	}
    	while (next) ;

    	return (v1) ;
}

vertex_hdl_t
create_graph(char *path)
{
	char 	 	*remain = NULL ;
	vertex_hdl_t	vhdl = GRAPH_VERTEX_NONE ;

	if (!path)
        	return GRAPH_VERTEX_NONE ;

	hwgraph_path_lookup(snpdi_rvhdl, path,
                                        &vhdl, &remain) ;
#if 0
	PG_DPRINT("create_graph: %s, %d, %s\n", path, vhdl, remain) ;
#endif
	if (vhdl == GRAPH_VERTEX_NONE)
		return GRAPH_VERTEX_NONE ;

	if (remain == NULL)
		return vhdl ;

	return (create_graph_path(vhdl, remain, REPORT_ERR)) ;
}
#endif SN_PDI


#include <sys/SN/klconfig.h>
#include <sys/SN/gda.h>
#include <kllibc.h>

#define MAX_BRDS_PER_MODULE		16

int num_modules ;
struct mod_tab module_table[MAX_MODULES] ;

void
init_module_table(void)
{
	int 		i, j, found ;
        cnodeid_t 	cnode ;
	nasid_t		nasid ;
        lboard_t 	*lbptr ;
    	gda_t *gdap;

    	gdap = (gda_t *)GDA_ADDR(get_nasid());
	if (gdap->g_magic != GDA_MAGIC) {
		printf("init_module_table: Invalid GDA MAGIC\n") ;
		return ;
	}

	bzero(&module_table[0], sizeof(module_table)) ;

	/* element 0 is not used and should be filled with 0 */

	module_table[0].module_id = 0 ; 
	module_table[0].module_num = 0 ;

        for (cnode = 0, i = 1; cnode < MAX_COMPACT_NODES; cnode ++) {
                nasid = gdap->g_nasidtable[cnode];
                if (nasid == INVALID_NASID)
                        continue;
                lbptr = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid),
                                  KLTYPE_IP27);
                if (!lbptr) {
                    printf("init_module_table: Error - No IP27 on nasid %d!\n",
                                nasid);
                        continue;
                }

		found = 0 ;

		for(j=1;j<=i;j++)
			if (module_table[j].module_id == lbptr->brd_module) {
				found = 1 ;
				break ;
			}

		if (found) {
			found = 0 ;
			continue ;
		}

		module_table[i++].module_id = lbptr->brd_module ;
	}
	num_modules = i-1 ;

	module_table[1].module_num = MAX_BRDS_PER_MODULE ;

}


