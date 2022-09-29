/*
 * promgraph.h - Prototypes and definitions for the standalone kernel library
 *
 *
 * $Revision: 1.23 $
 */

#ifndef _PROMGRAPH_H
#define _PROMGRAPH_H

#include <libsk.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/promcfg.h>
#include <sys/nic.h>

extern int		pg_debug ;
#define PG_DPRINT       if (pg_debug) printf

int init_prom_graph(void) ;
int init_component_locator(void) ;
int init_board_graph(lboard_t *, int) ;
int init_prom_node_graph(int) ;

int init_ip27_graph(lboard_t *lbinfo, int node) ;
int init_baseio_graph(lboard_t *lbinfo, int node) ;
int init_xtalkbrd_graph(lboard_t *lbinfo, int node) ;
int init_midplane8_graph(lboard_t *lbinfo, int node) ;
int init_router2_graph(lboard_t *lbinfo, int node, pcfg_router_t *) ;
int init_graphics_graph(lboard_t *lbinfo, int node) ;
int init_tpu_graph(lboard_t *lbinfo, int node) ;
int init_gsn_graph(lboard_t *lbinfo, int node) ;
int add_link_routers() ;
int init_module_locator(void) ;
int add_routers(void) ;
int link_routers(void) ;
int link_node_boards(int) ;
int link_2_routers(int, int) ;
int link_2_boards(lboard_t *) ;
klinfo_t *init_device_graph(vertex_hdl_t devctlr_vhdl, int dev_type);

int link_device_to_graph(vertex_hdl_t, vertex_hdl_t, klinfo_t *, int (*)()) ;
int link_node_boards(int ) ;

void prom_install_devices(void) ;
extern int kl_scsi_install(vertex_hdl_t, vertex_hdl_t) ;
extern int find_hub_cfgind(int) ;
extern nasid_t find_router_nasid(int) ;
int vvisit_install(void *, vertex_hdl_t) ;
int vvisit_init(void *, vertex_hdl_t) ;
extern int inst_valid(int (*)()) ;
LONG kl_reg_drv_strat(prom_dev_info_t *, int (*)()) ;
void get_edge_name(klinfo_t *, char *, int) ;

int promgraph_setup_ip27_devices(lboard_t *, nasid_t, vertex_hdl_t *);
int promgraph_setup_xtalk_devices(lboard_t *, nasid_t, vertex_hdl_t *);

vertex_hdl_t create_device_graph(vertex_hdl_t, klinfo_t *, int(*)()) ;
vertex_hdl_t create_graph_path(vertex_hdl_t , char *, int errflg) ;

void kl_register_install(vertex_hdl_t, vertex_hdl_t, void (*)()) ;

int pg_setup_bridge_alias(void) ;
int pg_setup_compt_alias(vertex_hdl_t, lboard_t *, int) ;

int klcfg_get_nic_info(nic_data_t, char *) ;

/* Some of the labels used in the prom */

#define INFO_LBL_VERTEX_NAME	"sgi_vertex_name"
#define INFO_LBL_KLCFG_INFO 	"sgi_klcfg_info"
#define INFO_LBL_DEV_INFO 	"sgi_dev_info"
#define INFO_LBL_BRD_INFO 	"sgi_brd_info"
#define INFO_LBL_LINK 		"link"

#define EDGE_LBL_HARDWARE	"hw"
#define EDGE_LBL_SYSNAME        "SGI-IP27"
#define VERTEX_NAME_ROOT	"Root"
#define EDGE_LBL_ROOT_XWIDGET   "/xwidget"
#define EDGE_LBL_MASTER_BRIDGE  "master_bridge"
#define EDGE_LBL_MASTER_IOC3	"master_ioc3"
#define EDGE_LBL_MASTER_QL	"master_ql%d"

/* ---------------Stuff needed by modules and partitions. later this will to to partition.h */

struct mod_elem {
        char    slot_number ;
        struct mod_elem *next ;
} ;

struct mod_tab {
        char    module_id ;             /* Partition number */
        char    module_num ;            /* Number of brds */
        vertex_hdl_t    vhdl ;
        char module_name[16] ;    	/* Partition names */
        struct mod_elem * module_elements ;       /* Pointer to slot numbers list */
} ;

typedef struct inst_later {
        vertex_hdl_t            hw ;
        vertex_hdl_t            loc ;
        struct inst_later       *next;
        void                    (*install)();
} inst_info_t ;

#endif /* _PROMGRAPH_H */
