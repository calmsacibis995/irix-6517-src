#ident "stubs/failoverstubs.c: $Revision: 1.6 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/conf.h>
#include <sys/graph.h>
#include <sys/iograph.h>
#include <sys/kmem.h>
#include <sys/iograph.h>
#include <sys/param.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/scsi.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/failover.h>

struct scsi_candidate fo_candidates[] = {
  {
    NULL, NULL,
    0, 0,
    NULL, 
    NULL,
    NULL
  }
};

void fo_init()
{
  return;
}

/* ARGSUSED */
void fo_scsi_device_update(u_char *inv, vertex_hdl_t lun_vhdl)
{
  return;
}

/* ARGSUSED */
void fo_scsi_lun_remove(vertex_hdl_t lun_vhdl)
{
  return;
}

/* ARGSUSED */
int fo_scsi_device_switch(vertex_hdl_t primary_vhdl, vertex_hdl_t *secondary_vhdl)
{
  return(FO_SWITCH_FAIL);
}

/* ARGSUSED */
int fo_scsi_device_switch_new(vertex_hdl_t primary_vhdl, vertex_hdl_t *secondary_vhdl, int tresspass)
{
  return(FO_SWITCH_FAIL);
}

/* ARGSUSED */
int 
fo_scsi_device_pathcount(vertex_hdl_t vhdl)
{
  return(0);
}

/* ARGSUSED */
int 
fo_is_failover_candidate(u_char *inv, vertex_hdl_t lun_vhdl)
{
  return(0);
}

/* ARGSUSED */
int 
fo_scsi_device_update_generic(struct user_fo_generic_info *fgi)
{
	return(0);
}
