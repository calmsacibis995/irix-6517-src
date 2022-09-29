/*
 * Stubs for plexing code.
 */
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/bpqueue.h>

/* only the null class supported */
const int  xlvd_n_classes = 1;
extern void xlvd_install_routines(void);

int xlv_plexing_support_present = 0;
int xlv_next_config_request() { return (EPIPE); }
void *xlv_tab_create_block_map() { return (NULL); }

/* need to install the null class */
void xlv_start_daemons() { }

int xlv_mmap_write_init() { return (ENOSYS); }
void xlv_mmap_write_done () { ; }
void xlvd_queue_request() { ; }
int xlv_copy_plexes () { return (ENOSYS); }

void *fo_get_deferred() { return (0); }
int xlvd_queue_failover() { return (0); }
bp_queue_t 		xlvd_work_queue = {NULL};
bp_queue_t 		xlv_labd_request_queue = {NULL};

void xlv_configchange_notification() { ; }
