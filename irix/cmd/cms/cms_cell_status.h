#include "cms_base.h"
#include "cms_message.h"
#include "cms_info.h"

typedef struct cms_cell_status_s {
	cms_info_t	cms_info[MAX_CELLS];
	char		cms_init;
	char		cms_cell_alive[MAX_CELLS];
	char		cms_link_status[MAX_CELLS][MAX_CELLS];
} cms_cell_status_t;

#define	cms_get_link_status(src, dest)	\
			(cms_cell_status->cms_link_status[(src)][(dest)])
#define	cms_get_cell_status(x)	\
			(cms_cell_status->cms_cell_alive[(x)])
#define	cms_set_cell_status(cell, val) \
		(cms_cell_status->cms_cell_alive[(cell)] = (val))
#define	cms_set_link_status(src, dest, val) \
		(cms_cell_status->cms_link_status[(src)][(dest)] = (val))
	
extern	cms_cell_status_t	*cms_cell_status;

extern void	cms_inject_failure(void);
extern void 	cms_print_cell_status(void);
