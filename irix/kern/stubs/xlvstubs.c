/*
 * xlv stubs
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/buf.h>
#include <sys/xlv_lock.h>
#include <sys/errno.h>


void			*xlv_tab = NULL;
void			*xlv_tab_vol = NULL;
xlv_lock_t		 xlv_io_lock[1];

int xlv_tab_set() { return (ENOPKG); }

int xlv_boot_disk() { return (0); }

int xlv_attr_get_cursor() { return (ENOSYS); }
int xlv_attr_get() { return (ENOSYS); }
int xlv_attr_set() { return (ENOSYS); }
int xlv_get_subvolumes() { return ENOSYS; }
int xlv_file_to_disks() { return ENOSYS; }

void xlv_icrash() { }
