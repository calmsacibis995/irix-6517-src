#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>

#include <sys/SN/klconfig.h>
#include <sys/SN/error.h>
#include "../sn0_fru_analysis.h"

/*
 * global variables for easy testing
 */

#define MAX_NODES		10			/* max. # of nodes in the
							 * system
							 */
/* given an component index within a board this gets the pointer to
 * the component info
 */

#define COMP(_brd,_ndx) 		(_brd)->brd_compts[(_ndx)]


/* pointer to error info hanging off a component */
#define COMP_ERR_INFO(_comp_info)	(_comp_info->errinfo)


#define LINE_SIZE 90

/* These are to fix compilation errors */
#ifdef ROUTER
#undef ROUTER
#endif
#ifdef WIDGET_STATUS
#undef WIDGET_STATUS
#endif

#define ERR_INT_PEND		"ERR_INT_PEND"
#define ERR_STATUS0_A		"ERR_STATUS0_A"
#define ERR_STATUS0_B		"ERR_STATUS0_B"
#define ERR_STATUS1_A		"ERR_STATUS1_A"
#define ERR_STATUS1_B		"ERR_STATUS1_B"

#define DIR_ERROR		"DIR_ERROR"	
#define MEM_ERROR		"MEM_ERROR"	
#define PROTOCOL_ERROR		"PROTOCOL_ERROR"	
#define MISC_ERROR		"MISC_ERROR"	

#define IOCRB			"IOCRB_"
#define WIDGET_STATUS		"WIDGET_STATUS"
#define BTE0_STATUS		"BTE0_STATUS"
#define BTE1_STATUS		"BTE1_STATUS"
#define BTE0_SRC		"BTE0_SRC"
#define BTE1_SRC		"BTE1_SRC"
#define BTE0_DEST		"BTE0_DEST"
#define BTE1_DEST		"BTE1_DEST"

#define PIO_VECTOR_STATUS	"PIO_VECTOR_STATUS"
#define PORT_ERROR		"PORT_ERR"

#define CACHE_ERR		"CACHE_ERR"
#define CACHE_TAG0		"CACHE_TAG0"
#define CACHE_TAG1		"CACHE_TAG1"

#define PORT_0_STATUS		"PORT_0_STATUS"
#define PORT_1_STATUS		"PORT_1_STATUS"
#define PORT_2_STATUS		"PORT_2_STATUS"
#define PORT_3_STATUS		"PORT_3_STATUS"
#define PORT_4_STATUS		"PORT_4_STATUS"
#define PORT_5_STATUS		"PORT_5_STATUS"

#define WIDGET_0_STATUS		"WIDGET_0_STATUS"
#define ERROR_UPPER		"ERROR_UPPER"
#define ERROR_LOWER		"ERROR_LOWER"
#define ERROR_CMD		"ERROR_CMD"
#define LINK_8_STATUS		"LINK_8_STATUS"
#define LINK_9_STATUS		"LINK_9_STATUS"
#define LINK_A_STATUS		"LINK_A_STATUS"
#define LINK_B_STATUS		"LINK_B_STATUS"
#define LINK_C_STATUS		"LINK_C_STATUS"
#define LINK_D_STATUS		"LINK_D_STATUS"
#define LINK_E_STATUS		"LINK_E_STATUS"
#define LINK_F_STATUS		"LINK_F_STATUS"

/* for each widget */
#define ERR_UPPER		"ERR_UPPER"
#define ERR_LOWER		"ERR_LOWER"
#define ERR_CMD			"ERR_CMD"
#define AUX_ERR			"AUX_ERR"
#define RESP_UPPER		"RESP_UPPER"
#define RESP_LOWER		"RESP_LOWER"
#define RAM_PERR		"RAM_PERR"
#define PCI_UPPER		"PCI_UPPER"
#define PCI_LOWER		"PCI_LOWER"
#define INT_STATUS		"INT_STATUS"

#define NODE			"NODE"
#define BASEIO			"BASEIO"
#define WIDGET			"WIDGET"
#define XBOW			"XBOW"
#define CPU0			"CPU0"
#define CPU1			"CPU1"
#define ROUTER			"ROUTER"
#define HUB			"HUB"
#define PI			"PI"
#define MD			"MD"
#define IO			"IO"
#define NI			"NI"

#define NUM_NODES "NUM_NODES"


typedef struct err_state_s {
	int hub_pi;
	hub_pierr_t pi_values;

	int hub_md;
	hub_mderr_t md_values;

	int hub_io;
	hub_ioerr_t io_values;

	int hub_ni;
	hub_nierr_t ni_values;

	int cpu0;
	cache_err_t cpu0_values;

	int cpu1;
	cache_err_t cpu1_values;

	int router;
	hubreg_t router_values[6];

	int xbow;
	xbow_error_t xbow_values;

	int widget[8];
	bridge_error_t widget_values[8];

	int baseio;
	bridge_error_t baseio_values;

} err_state_t;

kl_config_hdr_t				g_node[MAX_NODES];
confidence_t				g_io_conf;
confidence_t				g_legonet_conf;
confidence_t				g_xbow_conf;
int					g_ce_valid;
int					no_xbow;

/* For memory & cache err testing */
int 					nasid = 0;
int 					force_bad_ecc = 0;
int					full_fru_analysis = 1;

int line_num = 0;
int num_nodes;
extern kf_hint_t		node_hint[MAXCPUS / 2];

/* Dummy routine to return the current statically assigned nasid
 * For memory & cache error testing
 */
nasid_t
get_nasid()
{
	return nasid;
}
void
error(char *string, ...) {
	va_list ap;

	printf("Error line %d: ",line_num);

	va_start(ap, string);
	vprintf(string, ap);
	va_end(ap);

	printf("\n");
	exit(-1);
}


char*
get_line(char *line, FILE *input_file) {

	while(line = fgets(line,LINE_SIZE,input_file)) {
		line_num++;

		if ((line[0] != '#') && (line[0] != '\n'))
			break;
	}

	if (line) {
		int len = strlen(line);

		if (line[len - 1] == '\n')
			line[len - 1] = '\0';
	}

	return line;
}

int
get_num_nodes(FILE *input_file) {
	char line[LINE_SIZE];
	int num;

	while((get_line(line,input_file)) && 
	      strncmp(line,NUM_NODES,strlen(NUM_NODES))) {}

	num = atoi(line + strlen(NUM_NODES) + 1);

	if (num > 0) {
		while((get_line(line,input_file)) &&  strncmp(line,NODE,
							      strlen(NODE))) {}
		
	}
	else
		error("must have at least one node");

	return num;
}



hubreg_t*
get_pi_reg(err_state_t *state, char *token) {

	if (!strcmp(token,ERR_INT_PEND)) {
		return &state->pi_values.hp_err_int_pend;
	}
	else if (!strcmp(token,ERR_STATUS0_A)) {
		return &state->pi_values.hp_err_sts0[0];
	}
	else if (!strcmp(token,ERR_STATUS0_B)) {
		return &state->pi_values.hp_err_sts0[1];
	}
	else if (!strcmp(token,ERR_STATUS1_A)) {
		return &state->pi_values.hp_err_sts1[0];
	}
	else if (!strcmp(token,ERR_STATUS1_B)) {
		return &state->pi_values.hp_err_sts1[1];
	}
		
	return 0;
}

hubreg_t*
get_md_reg(err_state_t *state, char *token) {

	if (!strcmp(token,DIR_ERROR)) {
		return &state->md_values.hm_dir_err;
	}
	else if (!strcmp(token,MEM_ERROR)) {
		return &state->md_values.hm_mem_err;
	}
	else if (!strcmp(token,PROTOCOL_ERROR)) {
		return &state->md_values.hm_proto_err;
	}
	else if (!strcmp(token,MISC_ERROR)) {
		return &state->md_values.hm_misc_err;
	}

	return 0;
}

hubreg_t*
get_io_reg(err_state_t *state, char *token) {

	if (!strncmp(token,IOCRB,strlen(IOCRB))) {
		long crb_num;
		
		token = strtok(token,"_");
		token = strtok(0,"_");
		
		crb_num = strtol(token,0,16);
		if ((crb_num < 0) || (crb_num > IIO_NUM_CRBS))
			error("bad IOCRB number");
		
		return &state->io_values.hi_crb_entA[crb_num];
	}
	else if (!strcmp(token,WIDGET_STATUS)) {
		return &state->io_values.hi_wstat;
	}
	else if (!strcmp(token,BTE0_STATUS)) {
		return &state->io_values.hi_bte0_sts;
	}
	else if (!strcmp(token,BTE1_STATUS)) {
		return &state->io_values.hi_bte1_sts;
	}
	else if (!strcmp(token,BTE0_SRC)) {
		return &state->io_values.hi_bte0_src;
	}
	else if (!strcmp(token,BTE1_SRC)) {
		return &state->io_values.hi_bte1_src;
	}
	else if (!strcmp(token,BTE0_DEST)) {
		return &state->io_values.hi_bte0_dst;
	}
	else if (!strcmp(token,BTE1_DEST)) {
		return &state->io_values.hi_bte1_dst;
	}

	return 0;
}

hubreg_t*
get_ni_reg(err_state_t *state, char * token) {

	if (!strcmp(token,PIO_VECTOR_STATUS)) {
		return &state->ni_values.hn_vec_sts;
	}
	else if (!strcmp(token,PORT_ERROR)) {
		return &state->ni_values.hn_port_err;
	}

	return 0;

}

void
read_hub_reg(err_state_t *state, char *section) {
	char * token;
	char *value;
	hubreg_t *reg;

	token = strtok(0," ");

	if (!token)
		error("expected register name");

	/* we have to do this here in case get_io_reg needs to use strtok */
	value = strtok(0," ");

	if (!value)
		error("expected register value");

	if (!strcmp(section,PI)) {
		state->hub_pi = 1;
		reg = get_pi_reg(state,token);
	}
	else if (!strcmp(section,MD)) {
		state->hub_md = 1;
		reg = get_md_reg(state,token);
	}
	else if (!strcmp(section,IO)) {
		state->hub_io = 1;
		reg = get_io_reg(state,token);
	}
	else if (!strcmp(section,NI)) {
		state->hub_ni = 1;
		reg = get_ni_reg(state,token);
	}
	else
		error("bad HUB section name");

	if (!reg)
		error("bad HUB %s register name",section);
	
	*reg = (hubreg_t)strtoull(value,0,0);
}

void
read_cpu_reg(int cpu_num, err_state_t *state) {
	char * token;
	uint *reg = 0;
	__uint64_t *tag = 0;
	
	if (cpu_num == 0)
		state->cpu0 = 1;
	else
		state->cpu1 = 1;

	token = strtok(0," ");

	if (!token)
		error("expected register name");

	if (!strcmp(token,CACHE_ERR)) {
		if (cpu_num == 0)
			reg = &state->cpu0_values.ce_cache_err;
		else
			reg = &state->cpu1_values.ce_cache_err;

		token = strtok(0," ");
		if (!token)
			error("expected register value");


	}



	if (reg)
		*reg = (uint)strtoull(token,0,0);

	if (!strcmp(token,CACHE_TAG0)) {
		if (cpu_num == 0)
			tag = &(state->cpu0_values.ce_tags[0]);
		else
			tag = &(state->cpu1_values.ce_tags[0]);
		token = strtok(0," ");
		if (!token)
			error("expected register value");
	}

	if (tag)
		*tag = (__uint64_t)strtoull(token,0,0);

	if (!strcmp(token,CACHE_TAG1)) {
		if (cpu_num == 0)
			tag = &(state->cpu0_values.ce_tags[1]);
		else
			tag = &(state->cpu1_values.ce_tags[1]);
		token = strtok(0," ");
		if (!token)
			error("expected register value");
	}

	if (tag)
		*tag = (__uint64_t)strtoull(token,0,0);

	if (!reg && !tag)
		error("bad CPU register name");


}

void
read_router_reg(err_state_t *state) {
	char * token;
	hubreg_t *reg = 0;
	int port_num;

	state->router = 1;


	token = strtok(0,"_");

	if (strcmp(token,"PORT"))
		error("expected router register");
		
	token = strtok(0,"_");
	port_num = atoi(token);

	if ((port_num < 0) || (port_num > 5))
		error("%d is not a valid router port",port_num);
		
	token = strtok(0," ");

	if (!token)
		error("expected router register");

	if (!strcmp(token,"STATUS")) {
		reg = &state->router_values[port_num];
	}

	if (!reg)
		error("bad ROUTER register name");

	token = strtok(0," ");

	if (!token)
		error("expected register value");
		
	*reg = (hubreg_t)strtoull(token,0,0);

}


void
read_xbow_reg(err_state_t *state) {
	char * token;
	char *value;
	xbowreg_t *reg = 0;

	state->xbow = 1;

		
	token = strtok(0," ");

	if (!token)
		error("expected xbow register");

	value = strtok(0," ");

	if (!value)
		error("expected register value");

		
	if (!strcmp(token,WIDGET_0_STATUS)) {
		reg = &state->xbow_values.xb_status;
	}
	else if (!strcmp(token,ERROR_UPPER)) {
		reg = &state->xbow_values.xb_err_upper;
	}
	else if (!strcmp(token,ERROR_LOWER)) {
		reg = &state->xbow_values.xb_err_lower;
	}
	else if (!strcmp(token,ERROR_CMD)) {
		reg = &state->xbow_values.xb_err_cmd;
	}
	else if (!strncmp(token,"LINK_",5)) {
		long link_num;

		token = strtok(token,"_");
		token = strtok(0,"_");
			
		/* subtract 8 since indexed 0 to 8, but file has 8 to F */
		link_num = strtol(token,0,16) - 8;

		if ((link_num < 0) || (link_num > 8))
			error("%d is not a valid link",link_num);

		token = strtok(0," ");

		if (!strcmp(token,"AUX_STATUS"))
			reg = &state->xbow_values.xb_link_aux_status[link_num];
		else if (!strcmp(token,"STATUS"))
			reg = &state->xbow_values.xb_link_status[link_num];
	}

	if (!reg)
		error("bad XBOW register name");
		
	*reg = (xbowreg_t)strtoull(value,0,0);
}

bridgereg_t*
get_bridge_reg(bridge_error_t *err, char *token) {
	bridgereg_t *reg = 0;

	if (!strcmp(token,ERR_UPPER)) {
		reg = &err->br_err_upper;
	}
	else if (!strcmp(token,ERR_LOWER)) {
		reg = &err->br_err_lower;
	}
	else if (!strcmp(token,ERR_CMD)) {
		reg = &err->br_err_cmd;
	}
	else if (!strcmp(token,AUX_ERR)) {
		reg = &err->br_aux_err;
	}
	else if (!strcmp(token,RESP_UPPER)) {
		reg = &err->br_resp_upper;
	}
	else if (!strcmp(token,RESP_LOWER)) {
		reg = &err->br_resp_lower;
	}
	else if (!strcmp(token,RAM_PERR)) {
		reg = &err->br_ram_perr;
	}
	else if (!strcmp(token,PCI_UPPER)) {
		reg = &err->br_pci_upper;
	}
	else if (!strcmp(token,PCI_LOWER)) {
		reg = &err->br_pci_lower;
	}
	else if (!strcmp(token,INT_STATUS)) {
		reg = &err->br_int_status;
	}

	if (!reg)
		error("bad BRIDGE register name");

	return reg;
}

void
read_widget_reg(err_state_t *state) {
	int widget_num;
	char * token;
	bridgereg_t *reg = 0;


	token = strtok(0,"_");

	/* subtract 1 becuse they're in the file 1 to 8, indexed 0 to 7 */
	widget_num = atoi(token) - 1;

	if ((widget_num < 0) || (widget_num > 7))
		error("%d is an invalid widget", widget_num);

	state->widget[widget_num] = 1;
		
	token = strtok(0," ");

	if (!token)
		error("expected bridge register");

	reg = get_bridge_reg(&state->widget_values[widget_num],token);

	token = strtok(0," ");

	if (!token)
		error("expected register value");
	
	*reg = (bridgereg_t)strtoull(token,0,0);

}


void
read_node_err_state(err_state_t *state, FILE *input_file) {
	char line[LINE_SIZE];
	char *token;

	while (get_line(line,input_file)) {

		if (line[0] == '\0')
			continue;

		token = strtok(line,"_");
		if (!strncmp(NODE,token,strlen(NODE))) {
			return;
		}
		else if (!strcmp(HUB,token)) {
			token = strtok(0,"_");

			read_hub_reg(state, token);
		}
		else if (!strcmp(CPU0,token)) {
			read_cpu_reg(0,state);
			
		}
		else if (!strcmp(CPU1,token)) {
			read_cpu_reg(1,state);
			
		}
		else if (!strcmp(ROUTER,token)) {
			read_router_reg(state);
		}
		else if (!strcmp(XBOW,token)) {
			read_xbow_reg(state);
		}
		else if (!strcmp(WIDGET,token)) {
			read_widget_reg(state);
		}
		else if (!strcmp(BASEIO,token)) {
			bridgereg_t *reg = 0;

			state->baseio = 1;

			token = strtok(0," ");

			if (!token)
				error("expected register name");

			reg = get_bridge_reg(&state->baseio_values,token);

			
			token = strtok(0," ");

			if (!token)
				error("expected register value");
			
			*reg = (bridgereg_t)strtoull(token,0,0);
		}
		else {
			error("Invalid syntax");
		}
	}

}



kl_config_hdr_t
create_klconfig_node(FILE *input_file, int node_num) {
	int i;
	err_state_t state;

	/* header */
	kl_config_hdr_t          *node_header;
	/* boards */
	lboard_t		*ip27_board,
		                *router_board,
	                        *io6_board,
	                        *midplane;

	/* components */
	klinfo_t		*cpu0,
	                        *cpu1,
	                        *hub,
	                        *router,
	                        *xbow,
	                        *bridge;

	/* error info for components */
	klcpu_err_t		*klcpu0_err,
	                        *klcpu1_err;
	klhub_err_t		*klhub_err;
	klrouter_err_t	        *klrou_err;
	klbridge_err_t          *klbri_err;
	klxbow_err_t            *klxbow_err;
    
	hub_error_t		*hub_err;


	bzero(&state,sizeof(err_state_t));

	/* read this node's error state from the file */
	read_node_err_state(&state,input_file);


	/* create header */
	node_header = (kl_config_hdr_t*)calloc(1,sizeof(kl_config_hdr_t));

	/* create board structures */
	ip27_board	= (lboard_t *)calloc(1,sizeof(lboard_t));
	router_board	= (lboard_t *)calloc(1,sizeof(lboard_t));
	io6_board       = (lboard_t *)calloc(1,sizeof(lboard_t));
	midplane        = (lboard_t *)calloc(1,sizeof(lboard_t));
	
	/* create component structures */
	cpu0		= (klinfo_t *)calloc(1,sizeof(klinfo_t));	
	cpu1		= (klinfo_t *)calloc(1,sizeof(klinfo_t));	
	hub 		= (klinfo_t *)calloc(1,sizeof(klinfo_t));	
	router		= (klinfo_t *)calloc(1,sizeof(klinfo_t));
	bridge          = (klinfo_t *)calloc(1,sizeof(klinfo_t));
	xbow            = (klinfo_t *)calloc(1,sizeof(klxbow_t));
	
	/* create error structures */
	klcpu0_err	= (klcpu_err_t *)calloc(1,sizeof(klcpu_err_t));
	klcpu1_err	= (klcpu_err_t *)calloc(1,sizeof(klcpu_err_t));
	klhub_err	= (klhub_err_t *)calloc(1,sizeof(klhub_err_t));
	klrou_err	= (klrouter_err_t *)calloc(1,sizeof(klrouter_err_t));
	klbri_err       = (klbridge_err_t *)calloc(1,sizeof(klbridge_err_t));
	klxbow_err      = (klxbow_err_t *)calloc(1,sizeof(klxbow_err_t));
	
	hub_err		= HUB_ERROR_STRUCT(klhub_err,1);

	/* fill in error values if set in the file */
	if (state.hub_pi) {
		bcopy(&state.pi_values,&(hub_err->hb_pi),sizeof(hub_pierr_t));
	}
	if (state.hub_md) {
		bcopy(&state.md_values,&(hub_err->hb_md),sizeof(hub_mderr_t));
	}
	if (state.hub_io) {
		bcopy(&state.io_values,&(hub_err->hb_io),sizeof(hub_ioerr_t));
	}
	if (state.hub_ni) {
		bcopy(&state.ni_values,&(hub_err->hb_ni),sizeof(hub_nierr_t));
	}
	if (state.cpu0) {
		/* assumes that if it was written out it must be valid, this may
		be a bad assumption */
		klcpu0_err->ce_valid = 1;
		g_ce_valid = 1;
		bcopy(&state.cpu0_values,&(klcpu0_err->ce_cache_err_dmp),
		      sizeof(cache_err_t));
	}
	if (state.cpu1) {
		/* assumes that if it was written out it must be valid, this may
		be a bad assumption */
		klcpu1_err->ce_valid = 1;
		g_ce_valid = 1;
		bcopy(&state.cpu1_values,&(klcpu1_err->ce_cache_err_dmp),
		      sizeof(cache_err_t));
	}
	if (state.router) {
		bcopy(&state.router_values,&klrou_err->re_status_error,
		      sizeof(hubreg_t[6]));
	}
	if (state.xbow) {
		bcopy(&state.xbow_values,&klxbow_err->xe_dmp,sizeof(xbow_error_t));
	}
	else if (state.baseio) {
		bcopy(&state.baseio_values,&(klbri_err->be_dmp),
		      sizeof(bridge_error_t));
	}
	/* widget errors are taken care of below */
	
	
	/* add IP27 to node's board list */
	node_header->ch_board_info	= (klconf_off_t)ip27_board;

	/* fill in IP27 board structs */
	ip27_board->struct_type 		= LOCAL_BOARD;
	ip27_board->brd_type 			= KLTYPE_IP27;
	ip27_board->brd_module			= node_num/4 + 1;
	ip27_board->brd_slot			= node_num % 4;
	KLCF_NUM_COMPS(ip27_board)		= 3;  /* cpu0/1,hub for now */
	

	cpu0->struct_type			= KLSTRUCT_CPU;
	cpu0->virtid				= 0;	/* cpuA */
	/* add error info to cpu struct */
	COMP_ERR_INFO(cpu0)			= (klconf_off_t)klcpu0_err;

	cpu1->struct_type			= KLSTRUCT_CPU;
	cpu1->virtid				= 1;	/* cpuB */
	/* add error info to cpu struct */
	COMP_ERR_INFO(cpu1)			= (klconf_off_t)klcpu1_err;

	hub->struct_type			= KLSTRUCT_HUB;
	/* hang the hub's error info off the hub component */
	COMP_ERR_INFO(hub)			= (klconf_off_t)klhub_err;

	/* hang the cpu0,cpu1,hub as components off the ip27board */
	COMP(ip27_board,IP27_CPU0_INDEX) 	= (klconf_off_t)cpu0;
	COMP(ip27_board,IP27_CPU1_INDEX) 	= (klconf_off_t)cpu1;
	COMP(ip27_board,IP27_HUB_INDEX)		= (klconf_off_t)hub;

	/* add router to list of boards */
	ip27_board->brd_next			= (klconf_off_t)router_board;

	/* fill in router info */
	router_board->struct_type		= LOCAL_BOARD;
	router_board->brd_type			= KLTYPE_ROUTER2;
	KLCF_NUM_COMPS(router_board)		= 1;  /* 1 router for now */
	
	router->struct_type			= KLSTRUCT_ROU;
	/* hang the router's error info off the router component */
	COMP_ERR_INFO(router)			= (klconf_off_t)klrou_err;
	/* hang the router component off the router board */
	COMP(router_board,0)			= (klconf_off_t)router;


	if (!state.xbow) {
		/* if there is no xbow add a BASEIO instead */
	    router_board->brd_next = (klconf_off_t)io6_board;
	   
	    io6_board->struct_type                  = LOCAL_BOARD;
	    io6_board->brd_type                     = KLTYPE_BASEIO;
	    if (node_num < 2)
	      io6_board->brd_slot = 1;
	    else
	      io6_board->brd_slot = 7;
	    KLCF_NUM_COMPS(io6_board)               = 1;
	    
	    bridge->struct_type                     = KLSTRUCT_BRI;
	    
	    COMP_ERR_INFO(bridge)                   = (klconf_off_t)klbri_err;
	    
	    COMP(io6_board,0)                       = (klconf_off_t)bridge;
	}
	else {
		lboard_t *cur_board = 0;

		router_board->brd_next              = (klconf_off_t)midplane;
		midplane->struct_type               = LOCAL_BOARD;
		midplane->brd_type                  = KLTYPE_MIDPLANE8;
		KLCF_NUM_COMPS(midplane)            = 1;
		
		xbow->struct_type                   = KLSTRUCT_XBOW;
		COMP_ERR_INFO(xbow)                 = (klconf_off_t)klxbow_err;
		
		COMP(midplane,0)                    = (klconf_off_t)xbow;

		/* create and populate the widgets */
		for (i = 0; i < 0x10; i++) {

			/* only lower ports are used, but macros subtract the 
			base */
			if (i > 0x7) {
				((klxbow_t*)xbow)->xbow_port_info[i].port_offset = 0;
				((klxbow_t*)xbow)->xbow_port_info[i].port_flag = 0;
				continue;
			}
			
			if (i == 0x1) {
				/* this slot should attach to the master hub */

				/* add 0x7 because macros subtract it */
				((klxbow_t*)xbow)->xbow_master_hub_link = i + 0x7;
				((klxbow_t*)xbow)->xbow_port_info[i].port_offset = (klconf_off_t)ip27_board;
				((klxbow_t*)xbow)->xbow_port_info[i].port_flag = XBOW_PORT_ENABLE | XBOW_PORT_HUB;
				continue;
			}
				
			/* initialize widget's error states */
			io6_board = (lboard_t *)calloc(1,sizeof(lboard_t));
			bridge = (klinfo_t *)calloc(1,sizeof(klinfo_t));
			klbri_err = (klbridge_err_t *)calloc(1,
							     sizeof(klbridge_err_t));
			/* fill in widget error state */
			if (state.widget[i]) {
				bcopy(&state.widget_values[i],&(klbri_err->be_dmp),
				      sizeof(bridge_error_t));
			}

			if (cur_board)
				cur_board->brd_next = (klconf_off_t)io6_board;
			else
				midplane->brd_next = (klconf_off_t)io6_board;

			cur_board = io6_board;
			if (node_num < 2)
			  cur_board->brd_slot = i + 1;
			else
			  cur_board->brd_slot = i + 7;

#ifdef USE_MIXED_IO
			if (i == 0)
			  cur_board->brd_type = KLTYPE_4CHSCSI;
			else if (i == 2)
			  cur_board->brd_type = KLTYPE_ETHERNET;
			else if (i == 3)
			  cur_board->brd_type = KLTYPE_FDDI;
			else if (i == 4)
			  cur_board->brd_type = KLTYPE_HAROLD;
			else if (i == 5)
			  cur_board->brd_type = KLTYPE_FC;
			else if (i == 6)
			  cur_board->brd_type = KLTYPE_LINC;
			else
			  cur_board->brd_type = KLTYPE_BASEIO;
#else
			cur_board->brd_type = KLTYPE_BASEIO;
#endif

			((klxbow_t*)xbow)->xbow_port_info[i].port_offset = 
				(klconf_off_t)io6_board;
			((klxbow_t*)xbow)->xbow_port_info[i].port_flag = 
				XBOW_PORT_ENABLE | XBOW_PORT_IO;

			io6_board->struct_type              = LOCAL_BOARD;
			KLCF_NUM_COMPS(io6_board)           = 1; /*just the bridge*/
			
			bridge->struct_type                 = KLSTRUCT_BRI;
			/* add bridge error info to the bridge struct */
			COMP_ERR_INFO(bridge)               = (klconf_off_t)klbri_err;
			/* add bridge to the io6 board */
			COMP(io6_board,0)                   = (klconf_off_t)bridge;
			
		}
	}

	return *node_header;
}


int
main(int argc, char** argv) {
	int i;
	FILE *input_file;
	kf_analysis_t	curr_analysis;


	if (argc != 2) {
		printf("This program takes a file as an argument\n");
		exit(-1);
	}

	input_file = fopen(argv[1],"r");
	if (!input_file) {
		printf("Error opening file %s\n",argv[1]);
		exit(-1);
	}
      
	for(i = 0 ; i < MAXCPUS / 2 ; i++)
		node_hint[i].kh_hint_type = -1;

	num_nodes = get_num_nodes(input_file);

	for (i = 0; i < num_nodes; i++) {
		g_node[i] = create_klconfig_node(input_file,i);
	}

	fclose(input_file);

	kf_print	= (void (*)(char*,...)) printf;

	kf_print("\n\n*FRU ANALYSIS BEGIN\n");

	for (i = 0; i < num_nodes; i++) {
		/* PASS 1 */
		if (kf_node_analyze(i,NULL) != KF_SUCCESS)
			error("node %d analysis failed\n",i);
	}

	kf_analysis_init(&curr_analysis);
	kf_analysis_tab_init();

	for (i = 0; i < num_nodes; i++) {
		/* PASS 2 */
		if (kf_node_analyze(i,&curr_analysis) != KF_SUCCESS)
			error("node %d analysis failed\n",i);
	}
	
#ifdef FRUTEST_DEBUG_OUTPUT
	kf_analysis_tab_print(1);
#else
	kf_analysis_tab_print(MAX_GUESSES);
#endif

	return 0;
}
