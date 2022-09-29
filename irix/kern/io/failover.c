/*
 * FAILOVER module.
 *
 * Main functions:
 *    * Keep track of multipathed devices and paths to each.
 *    * Perform switch from a failed 'primary' path to one of
 *      'secondary' paths, and any device specific failover functions.
 *
 * Main entry points:
 *    * fo_scsi_device_update() - called during device discovery to
 *      update the failover data structures with the device being
 *      discovered.
 *    * fo_scsi_device_update_generic() - called by the user land
 *      program foupdate within in the foupdate init.d script just prior
 *      to calling xlv_assemble.  Initializes the kernel failover
 *      structures for generic devices.
 *    * fo_scsi_lun_remove() - called to cleanup failover structures
 *      following the removal of a LUN.
 *    * fo_scsi_device_switch() - called to perform switch from
 *      primary path to secondary path. The secondary path is
 *      returned. The paths must be vertex handles to scsi, char or
 *      block devices.
 *    * fo_scsi_device_pathcount() - called to determine the number of
 *      paths in a failover group. The path must be a vertex handle to
 *      scsi, char or block device.
 *    * fo_is_failover_candidate() - returns non-zero if the device is
 *      a candidate for * failover, else zero.
 *
 * Internal structures:
 *    * SCSI failover candidates list - this is a list of device types
 *      and pointers to device specific functions for performing generic
 *      tasks. Fields in each structure are:
 *       scsi_match_str1,
 *       scsi_match_str2,
 *       scsi_match_off1,
 *       scsi_match_off2 - used for determining whether a device is a
 *                         candidate for failover.
 *       scsi_validlun_func - device specific function used for
 *                            determining whether a LUN is actually valid.
 *       scsi_getname_func  - device specific function returning a
 *                            unique name across the name space of that device
 *                            type.
 *       scsi_cmpname_func  - device specific unique name comparison function.
 *       scsi_freename_func - device specific function for freeing
 *                            names returned by scsi_getname_func.
 *       scsi_sprintf_func  - device specific function returning string
 *                            representation of unique name.
 *       scsi_switch_func   - device specific function to perform failover.
 *       scsi_vh_coherency_policy - policy on maintaining coherency of
 *                                  partition vertices between the various paths
 *                                  of a failover group.
 *       scsi_foi;          - pointer to instance list for this device type.
 *   * SCSI failover instances - this is a list of devices found for
 *     the particular type previously determined to be candidates for
 *     failover. Fields in each structure are:
 *      foi_foc           - back pointer to candidate structure.
 *      foi_grp_name      - unique name of this failover group.
 *      foi_primary       - current primary path (index)
 *        foi_path_vhdl   - vertex handle of the LUN vertex of a path
 *        foi_path_status - status of the path (VALID, FAILED, etc.)
 * foi_next - pointer to next device of this type.  */

#ident "io/failover.c: $Revision: 1.29 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/conf.h>
#include <sys/graph.h>
#include <sys/iograph.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/scsi.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/invent.h>
#include <sys/dvh.h>
#include <sys/iobuf.h>
#include <sys/dksc.h>
#include <sys/failover.h>

#include <stdarg.h>
#include <string.h>

int	fo_devflag = D_MP;

#ifdef DEBUG
#define FO_DEBUG_LEVEL 1
int fo_debug = FO_DEBUG_LEVEL;
#endif

/*
 * List of free SCSI requests
 */
#define FO_SR_LIST_SIZE    4
#define FO_SR_CDB_SIZE     16
#define FO_SR_SENSE_SIZE   16
#define FO_SR_BUF_SIZE     256

struct fo_request {
  struct scsi_request *fr_srreq;
  sema_t              *fr_sema;
  u_char              *fr_command;
  u_char              *fr_sense;
  u_char              *fr_buffer;
  struct fo_request   *fr_next;
};
static struct fo_request *fr_free_list;
static sema_t            *fr_sema;
static sema_t            *fr_count_sema;

/*
 * Defined in master.d/failover
 */
extern uint fo_auto_failover_on_start;
extern uint fo_auto_failover_on_dev_deletion;

/*
 * Forward declarations for scsi HWG functions
 */
vertex_hdl_t scsi_disk_update(vertex_hdl_t, int);
vertex_hdl_t scsi_dev_add(vertex_hdl_t);

/*
 * Forward declarations for other local functions
 */
static struct fo_request *fo_get_fr(void);
static void fo_free_fr(struct fo_request *fr);
static int fo_split_pathname(char *, char *, char *);
static void fo_scsi_done(scsi_request_t *req);
static void fo_mirror_lun(vertex_hdl_t, vertex_hdl_t);
static int  fo_path_add (struct scsi_fo_instance *foi, vertex_hdl_t lun_vhdl, int is_viable_primary);
static void fo_remove_instance (struct scsi_candidate *foc, struct scsi_fo_instance *foi);
static int  fo_generic_switch_func(vertex_hdl_t, vertex_hdl_t);
static inventory_t *fo_hwgraph_inventory_get(vertex_hdl_t lun_vhdl);
static int clariion_k5_primary_func(struct scsi_candidate *, struct user_fo_generic_info *);
static int fo_inquiry_test(vertex_hdl_t);

/*
 * Forward declarations of DG specific functions
 */
struct dg_name {
  uint32_t dg_sig1;
  uint32_t dg_sig2;
  uint16_t dg_lun;
};
typedef struct dg_name *dg_name_t;
static int   dg_sauna_validlun_func(u_char *inq);
static int   dg_sauna_getname_func(vertex_hdl_t lun_vhld, scsi_name_t *ret_name);
static int   dg_sauna_cmpname_func(scsi_name_t, scsi_name_t);
static int   dg_sauna_freename_func(scsi_name_t ret_name);
static char *dg_sauna_sprintf_func(scsi_name_t name);
static int   dg_sauna_switch_func(vertex_hdl_t, vertex_hdl_t);

/*
 * CANDIDATES STRUCTURES: Define which type of devices will be candidates for failover
 */

#if 0	/* for reference */
struct scsi_candidate {
  char                    *scsi_match_str1;     /* First string used for matching */
  char                    *scsi_match_str2;     /* Second string used for matching */
  u_char                   scsi_match_off1;     /* Offset of 1st string in inventory */
  u_char                   scsi_match_off2;     /* Offset of 2nd string in inventory */
  int                    (*scsi_validlun_func)(u_char *);
  int                    (*scsi_getname_func)(vertex_hdl_t, scsi_name_t *);
  int                    (*scsi_cmpname_func)(scsi_name_t, scsi_name_t);
  int                    (*scsi_freename_func)(scsi_name_t);
  char                  *(*scsi_sprintf_func)(scsi_name_t);
  int                    (*scsi_switch_func)(vertex_hdl_t, vertex_hdl_t);
  int                    (*scsi_primary_func)(struct scsi_candidate *,
					      struct user_fo_generic_info *);
  int                      scsi_vh_coherency_policy;
  sema_t                  *scsi_foi_sema4;
  struct scsi_fo_instance *scsi_foi;
};
#endif

int fo_scsi_fo_instance_sz = sizeof(struct scsi_fo_instance);
int fo_scsi_candidate_sz = sizeof(struct scsi_candidate);

#define GENERIC_MATCH_1	"GENERIC"
#define GENERIC_MATCH_2	"DISK"

struct scsi_candidate fo_candidates[] = {
  {
    "SGI", "SAUNA",
    8, 36,
    dg_sauna_validlun_func,
    dg_sauna_getname_func, dg_sauna_cmpname_func, dg_sauna_freename_func, dg_sauna_sprintf_func,
    dg_sauna_switch_func,
    NULL,
    FO_VH_COH_POLICY_LAZY,
    NULL,
    NULL,
  },
  {
    "SGI", "K5",
    8, 36,
    dg_sauna_validlun_func,
    dg_sauna_getname_func, dg_sauna_cmpname_func, dg_sauna_freename_func, dg_sauna_sprintf_func,
    dg_sauna_switch_func,
    clariion_k5_primary_func,
    FO_VH_COH_POLICY_LAZY,
    NULL,
    NULL,
  },
  {
    GENERIC_MATCH_1, GENERIC_MATCH_2,
    0, 0,
    NULL,
    NULL, NULL, NULL, NULL,
    fo_generic_switch_func,
    NULL,
    FO_VH_COH_POLICY_LAZY,
    NULL,
    NULL
  },
  {	/* terminating entry */
    NULL, NULL,
    0, 0,
    NULL,
    NULL, NULL, NULL, NULL,
    NULL,
    NULL,
    0,
    NULL,
    NULL
  }
};

struct scsi_candidate *fo_candidate_generic=NULL;	/* pointer to generic foc */

/*
 * Local debug print function
 */
#ifdef DEBUG
static char *
sprint_hex(u_char ch)
{
  static char p[8];
  int         i;

  i = ((ch >> 4) & 0x0F);
  p[0] = (i > 9) ? (i - 10 + 'A') : (i + '0');
  i = (ch & 0x0F);
  p[1] = (i > 9) ? (i - 10 + 'A') : (i + '0');
  p[2] = '\0';
  return(p);
}
#endif

/*ARGSUSED*/
static void
DBG(int level, char *format, ...)
{
#ifdef DEBUG
  va_list ap;
  char    out_string[256];

  if (level <= fo_debug) {
    va_start(ap, format);
    vsprintf(out_string, format, ap);
    cmn_err(CE_DEBUG, out_string);
    va_end(ap);
  }
#endif
}

/*
 * Local debug print function for SCSI results
 */
/*ARGSUSED*/
static void
SCSI_DBG(int level, scsi_request_t *req, char *format, ...)
{
#ifdef DEBUG
  va_list ap;
  char    out_string[256];
  int     i;

  if (level <= fo_debug) {
    va_start(ap, format);
    vsprintf(out_string, format, ap);
    cmn_err(CE_DEBUG, out_string);
    va_end(ap);

    cmn_err(CE_DEBUG, "\tReturn status: 0x%x\n", req->sr_status);
    cmn_err(CE_DEBUG, "\tSCSI status: 0x%x\n", req->sr_scsi_status);
    cmn_err(CE_DEBUG, "\tCDB: ");
    for (i = 0; i < req->sr_cmdlen; ++i)
      cmn_err(CE_DEBUG, "%s ", sprint_hex(req->sr_command[i]));
    cmn_err(CE_DEBUG, "\n");
    if (req->sr_sensegotten > 0) {
      cmn_err(CE_DEBUG, "\tSCSI sense: ");
      for (i = 0; i < req->sr_sensegotten; ++i) {
	if ((i > 0) && ((i % 16) == 0))
	  cmn_err(CE_DEBUG, "\n\t            ");
	else
	  if ((i > 0) && ((i % 4) == 0))
	    cmn_err(CE_DEBUG, " ");
	cmn_err(CE_DEBUG, "%s ", sprint_hex(req->sr_sense[i]));
      }
      cmn_err(CE_DEBUG, "\n");
    }
  }
#endif
}

/*
 * FAILOVER module INIT function. Allocate needed structures.
 */
void
fo_init()
{
  u_char                *p;
  struct fo_request     *fr;
  int                    bufsize;
  int                    i;
  struct scsi_candidate *foc;

  /*
   * Allocate SR freelist sema4.
   */
  fr_sema = kern_calloc(1, sizeof(sema_t));
  init_sema(fr_sema, 1, "fo_sr", 0);
  fr_count_sema = kern_calloc(1, sizeof(sema_t));
  init_sema(fr_count_sema, FO_SR_LIST_SIZE, "fo_srcnt", 0);
  fr_free_list = NULL;

  /*
   * Allocate SRs, sense buffers, CDBs and data buffers
   */
  bufsize = FO_SR_LIST_SIZE * (sizeof(struct fo_request) +
			       sizeof(struct scsi_request) +
			       sizeof(sema_t) +
			       FO_SR_CDB_SIZE +
			       FO_SR_SENSE_SIZE +
			       FO_SR_BUF_SIZE);
  p = kern_calloc(1, bufsize);
  for (i = 0; i < FO_SR_LIST_SIZE; ++i) {
    fr = (struct fo_request *)p;
    p += sizeof(struct fo_request);

    fr->fr_srreq = (struct scsi_request *)p;
    p += sizeof(struct scsi_request);

    fr->fr_sema = (sema_t *)p;
    p += sizeof(sema_t);
    init_sema(fr->fr_sema, 0, "fo_sr", i);

    fr->fr_command = p;
    p += FO_SR_CDB_SIZE;

    fr->fr_sense = p;
    p += FO_SR_SENSE_SIZE;

    fr->fr_buffer = p;
    p += FO_SR_BUF_SIZE;

    if (fr_free_list)
      fr->fr_next = fr_free_list;
    fr_free_list = fr;
  }

  /*
   * Create instance list locks and do other initialization.
   */
  for (i = 0, foc = fo_candidates; foc->scsi_match_str1; ++i, ++foc) {
    foc->scsi_foi_sema4 = kern_calloc(1, sizeof(sema_t));
    init_sema(foc->scsi_foi_sema4, 1, "fo_foi", i);
    if (!fo_candidate_generic &&
	strcmp (foc->scsi_match_str1, GENERIC_MATCH_1) == 0 &&
	strcmp (foc->scsi_match_str2, GENERIC_MATCH_2) == 0)
    {
	fo_candidate_generic = foc;
    }
  }
}

/*
 * FAILOVER module START function. Perform auto-failover on those
 * degenerate cases where, e.g. in the case of Clariion, a LUN is
 * bound to a failed SP.
 */
void
fo_start()
{
  struct scsi_candidate   *foc;
  struct scsi_fo_instance *foi;
  int                      i;
  int                      rc, retry;
  u_char		   ctlr,targ,lun;
  scsi_lun_info_t         *lun_info;
  scsi_ctlr_info_t        *ctlr_info;
  struct fo_request       *fr;
  struct scsi_request     *req;
  u_char                  *cdb;
  u_char                  *inq;

  if (!fo_auto_failover_on_start)
    return;

  for (foc = fo_candidates; foc->scsi_match_str1; ++foc) {
    for (foi = foc->scsi_foi; foi; foi = foi->foi_next) {
      psema(foc->scsi_foi_sema4, PRIBIO);
      if (foi->foi_primary == FOI_NO_PRIMARY) {
	/*
	 * Found an instance with one or more valid paths but with no
	 * viable primary paths. Go through all valid paths and attempt
	 * to make then primaries by performing a failover switch. If
	 * still unsuccessful, then the first call to
	 * fo_scsi_device_switch for this instance will fail.
	 */
	for (i = 0; i < MAX_FO_PATHS; ++i) {
	  if (foi->foi_path[i].foi_path_status == FOI_PATHSTAT_VALID &&
	      foc->scsi_switch_func) {
	    lun_info = scsi_lun_info_get(foi->foi_path[i].foi_path_vhdl);
	    ctlr_info = STI_CTLR_INFO(SLI_TARG_INFO(lun_info));
	    ctlr     = SLI_ADAP(lun_info);
	    targ     = SLI_TARG(lun_info);
	    lun	     = SLI_LUN(lun_info);
	    DBG(2, "fo_start(%d,%d,%d) - initiating auto-failover\n", ctlr, targ, lun);
	    rc = (*foc->scsi_switch_func)(0, foi->foi_path[i].foi_path_vhdl);
	    if (rc == 0) {
	      DBG(2, "fo_start(%d,%d,%d) - auto-failover successfull\n", ctlr, targ, lun);
	      /*
	       * Successfully performed failover switch - make this
	       * path the primary and create appropriate HWG nodes.
	       */

	      foi->foi_primary = i;
	      /*
	       * Get the inquiry string and call scsi_device_update to
	       * create HWG nodes for this path.
	       */
	      inq = kern_calloc(1, SCSI_INQUIRY_LEN);
	      for (retry = 2; ;) {
		fr = fo_get_fr();
		ASSERT(fr);
		req = fr->fr_srreq;
		cdb = req->sr_command;
		cdb[0] = 0x12;	
		cdb[1] = cdb[2] = cdb[3] = cdb[5] = 0x0;
		cdb[4] = SCSI_INQUIRY_LEN;
		req->sr_lun_vhdl = foi->foi_path[i].foi_path_vhdl;
		req->sr_ctlr = ctlr;
		req->sr_target = targ;
		req->sr_lun = lun;
		req->sr_cmdlen = SC_CLASS0_SZ;
		req->sr_flags = SRF_DIR_IN | SRF_AEN_ACK;
		req->sr_timeout = 10;
		req->sr_buffer = inq;
		req->sr_buflen = SCSI_INQUIRY_LEN;
		req->sr_notify = fo_scsi_done;

		DBG(10, "fo_start: issuing inquiry command - fr 0x%x, req 0x%x\n", fr, req);
		ASSERT(req->sr_buffer);
		(*ctlr_info->sci_command)(req);
		psema(req->sr_dev, PRIBIO);
		DBG(10, "fo_start: command done, req 0x%x\n", req);
		if (req->sr_status == 0 && req->sr_scsi_status == 0	&& req->sr_sensegotten == 0)

/** BUG?? No free of fr **/
		  break;

		SCSI_DBG(1, req, "fo_start: Inquiry command failed: req 0x%x\n", req);
		if (retry-- == 0) {
		  rc = -1;
		  DBG(1, "fo_start(%d,%d,%d) - auto-failover update failed\n", ctlr, targ, lun);
		  goto update_done;
		}
		fo_free_fr(fr);
		break;
	      }
	      scsi_device_update(inq, foi->foi_path[i].foi_path_vhdl);
	    update_done:
	      kern_free(inq);
	    }
	    else {
	      DBG(1, "fo_start(%d,%d,%d) - auto-failover failed\n", ctlr, targ, lun);
	    }
	  }
	}
      }
      vsema(foc->scsi_foi_sema4);
    }
  }
}

static struct fo_request *
fo_get_fr(void) {
  struct fo_request *fr;

  psema(fr_count_sema, PRIBIO);
  psema(fr_sema, PRIBIO);

  fr = fr_free_list;
  fr_free_list = fr->fr_next;
  fr->fr_next = NULL;

  fr->fr_srreq->sr_command = fr->fr_command;
  fr->fr_srreq->sr_sense = fr->fr_sense;
  fr->fr_srreq->sr_senselen = FO_SR_SENSE_SIZE;
  fr->fr_srreq->sr_buffer = fr->fr_buffer;
  fr->fr_srreq->sr_dev = fr->fr_sema;

  vsema(fr_sema);
  return(fr);
}

static void
fo_free_fr(struct fo_request *fr)
{
  psema(fr_sema, PRIBIO);

  if (fr_free_list)
    fr->fr_next = fr_free_list;
  fr_free_list = fr;

  vsema(fr_sema);
  vsema(fr_count_sema);
}


/*
 * Clear the DK_FAILOVER flag in the dksoftc structure for the device, to stop
 * incoming requests from being returned with EIO.
 * This is necessary should we be returning to a previously failed path.
 *
 * Afterwards adjust the hwgraph inventory state for the device.
 */
static void
fo_enable_access(vertex_hdl_t lun_vhdl, struct scsi_fo_instance *foi, int remove_primary)
{
	inventory_t	*pinv;

	if (!foi->foi_restrict)
		return;
	if (dk_clr_flags(lun_vhdl,DK_FAILOVER))
		DBG (2, "Unable to clear failover flag for new vhdl %d\n", lun_vhdl);
	pinv = fo_hwgraph_inventory_get (lun_vhdl);
	if (pinv)
	{
		if (remove_primary)
			pinv->inv_state &= ~(INV_ALTERNATE | INV_FAILED | INV_PRIMARY);
		else {
			pinv->inv_state &= ~(INV_ALTERNATE | INV_FAILED);
			pinv->inv_state |= INV_PRIMARY;
		}
	}
}

/*
 * Set the DK_FAILOVER flag in the dksoftc structure for the device, to cause
 * the queued requests to be dequeued and returned with EIO.  This should
 * result in a retry of those requests down the new path.
 * Also results in new requests to the device being rejected with EIO.
 *
 * Afterwards adjust the hwgraph inventory state for the device.
 */
static void
fo_restrict_access(vertex_hdl_t lun_vhdl, struct scsi_fo_instance *foi, int failed)
{
	inventory_t	*pinv;

	if (!foi->foi_restrict)
		return;
	if (dk_set_flags (lun_vhdl,DK_FAILOVER))
		DBG (2, "Unable to set failover flag for old vhdl %d\n", lun_vhdl);
	pinv = fo_hwgraph_inventory_get (lun_vhdl);
	if (pinv)
	{
		if (failed) {
			pinv->inv_state &= ~INV_PRIMARY;
			pinv->inv_state |= (INV_ALTERNATE | INV_FAILED);
		}
		else {
			pinv->inv_state &= ~(INV_PRIMARY | INV_FAILED);
			pinv->inv_state |= INV_ALTERNATE;
		}
	}
}


/*
 * FAILOVER module primary entry point. Called from
 * dkscioctl(). Accepts a user_fo_generic_info structure (from user land!)
 * and creates an instance structure for each entry within it.
 *
 * First, verify that lvhs in call not resident in a different group.
 *	If so,	if it's primary, fail the call.
 *		if it's alternate, remove from group.
 *
 * Second, see if group membership is changing.
 *	If removing primary, initiate failover, to new primary if possible, then remove.
 *	If removing alternate, remove from group.
 *
 * Third, update group.
 *	Add new members.
 *	If changing primary, failover to it.
 *
 * Exit status:
 *	0		- ok
 *	EINVAL		- user_fo_generic_info invalid
 *	EACCES		- specified path part of different group
 *	EBUSY		- tried to remove primary path from group
 *	ENOMEM		- no room in instance table (too many entries), some entries may have been added
 *	EBADF		- group name mismatch or foi points to wrong candidate table
 */

int
fo_scsi_device_update_generic (struct user_fo_generic_info *fgi)
{
	int			error=0;
	int			i;
	int			j;
	int			paths;
	int			found_match;
	int			is_viable_primary;
	int			is_new_instance=0;
	u_char			*inquiry[MAX_FO_PATHS];
	u_char			*inq;
	scsi_name_t		foi_name;
	vertex_hdl_t		lun_vhdl[MAX_FO_PATHS];
	scsi_lun_info_t		*lun_info;
	struct scsi_candidate	*foc;
	struct scsi_fo_instance	*foi;
	struct scsi_fo_instance	*match;
	struct scsi_fo_instance	*inst;

	for (foc = fo_candidates; foc->scsi_match_str1; ++foc)
	{
		if (!strncmp(foc->scsi_match_str1,
			     (char *) &fgi->fgi_inq_data[0][foc->scsi_match_off1],
			     strlen(foc->scsi_match_str1)) &&
	            !strncmp(foc->scsi_match_str2,
			     (char *) &fgi->fgi_inq_data[0][foc->scsi_match_off2],
			     strlen(foc->scsi_match_str2)))
		{
			psema(foc->scsi_foi_sema4, PRIBIO);	/* lock the list */
			if (foc->scsi_primary_func != NULL)
				error = (*foc->scsi_primary_func)(foc, fgi);
			else
				error = EINVAL;
			vsema(foc->scsi_foi_sema4);
			goto bail_out;
		}
	}

	/*
	** Find the generic failover candidate table entry.
	*/
	for (found_match=0, foc = fo_candidates;
	     foc->scsi_match_str1;
	     ++foc) {
		if ( strcmp (foc->scsi_match_str1, GENERIC_MATCH_1) == 0
		  && strcmp (foc->scsi_match_str2, GENERIC_MATCH_2) == 0) {

			found_match = 1;
			break;
		}
	}

	if (!found_match) {
		error = EBADF;
		goto bail_out;
	}

	/*
	** Extract group name from failover_generic_info
	** and save in buffer.
	*/

	foi_name = fgi->fgi_instance_name;
	if (strlen(foi_name) == 0) {
		error = EINVAL;
		goto bail_out;
	}

	/*
	** Validate input structure.
	**	Have to have at least one set of data.
	**	NULL terminates input.
	*/

	for (i=0; i<MAX_FO_PATHS; i++) {
		inquiry[i]  = &fgi->fgi_inq_data[i][0];
		lun_vhdl[i] = fgi->fgi_lun_vhdl[i];
		if (!lun_vhdl[i]) {
			if (i==0) {
				error = ENODEV;
				goto bail_out;
			}
			else break;
		}
	}
		
	/*
	** Set number of paths encountered within structure.
	*/

	paths = i;

	/*
	** Try and get the failover instance by comparing group names.
	**	Verify that all incoming lun_vhdls are
	**	not part of another instance.
	**
	**	Note, raid instances are not checked.
	*/

	psema(foc->scsi_foi_sema4, PRIBIO);	/* lock the list */

	inst = foc->scsi_foi;
	match = 0;

	while (foi=inst) {			/* spin down the instances */

		DBG(2,"comparing group %s to group %s\n",inst->foi_grp_name,foi_name);

		if (strcmp (inst->foi_grp_name,foi_name) != 0) {	/* wrong group */

			for (i=0; i<paths; i++) {
				for (j=0; j<MAX_FO_PATHS; j++) {
					if (inst->foi_path[j].foi_path_vhdl == lun_vhdl[i]) {
						error = EACCES;
						goto update_done;
					}
				}
			}
		}
		else {
			match = inst;	/* found foi! */
		}
		inst = inst->foi_next;
	}

	foi = match;

	/*
	** Try and get the failover instance from a lun_info.
	*/

	if (!foi) {
		for (foi=NULL,j=0; j<paths; j++) {

			lun_info = scsi_lun_info_get(lun_vhdl[j]);
			foi = SLI_FO_INFO(lun_info);
			if (foi) {
				break;
			}

		}
	}

	/*
	** Instance doesn't exist for any paths.
	**
	**	If this is the very first instance, allocate and set up.
	**	If not, scan the instance chain checking for conflicting
	**	group name.  Should there be a conflict, return EBUSY.
	*/


	if (!foi) {		/* instance doesn't exist, set one up */

		/*
		**
		*/

		if (!foc->scsi_foi) {	/* first instance */
			is_new_instance = 1;
			foi = kern_calloc(1, sizeof(struct scsi_fo_instance));
			foc->scsi_foi = foi;
			foi->foi_foc = foc;
			foi->foi_grp_name = kern_calloc(1,1+sizeof(fgi->fgi_instance_name));
			foi->foi_primary = FOI_NO_PRIMARY;
			foi->foi_restrict = 1;
			strcpy (foi->foi_grp_name,fgi->fgi_instance_name);
			for (i=0; i<MAX_FO_PATHS; i++) {
				foi->foi_path[i].foi_path_vhdl = 0;
				foi->foi_path[i].foi_path_status = FOI_PATHSTAT_INVALID;
			}
		}

		else {			/* Check for duplicate group */
			foi = foc->scsi_foi;
			while (foi) {
				if (strcmp(foi->foi_grp_name,foi_name) == 0) {	/* Oops.... */
					error = EBUSY;	/* no incoming member same as existing member. */
					goto update_done;
				}
				foi = foi->foi_next;
			}
			ASSERT (foi==NULL);

			/*
			** Allocate a new foi and link it into chain.
			** Initialize the foi.
			*/

			is_new_instance = 1;
			foi = kern_calloc(1, sizeof(struct scsi_fo_instance));
			foi->foi_next = foc->scsi_foi;
			foc->scsi_foi = foi;
			foi->foi_foc = foc;
			foi->foi_grp_name = kern_calloc(1,1+sizeof(fgi->fgi_instance_name));
			foi->foi_primary = FOI_NO_PRIMARY;
			foi->foi_restrict = 1;
			strcpy (foi->foi_grp_name,fgi->fgi_instance_name);

			for (i=0; i<MAX_FO_PATHS; i++) {
				foi->foi_path[i].foi_path_vhdl = 0;
				foi->foi_path[i].foi_path_status = FOI_PATHSTAT_INVALID;
			}
		}
	}

	else if (foi->foi_foc != foc) {	/* from wrong candidate table */
		error = EBADF;
		goto update_done;
	}

	/*
	** A few sanity checks before going TOO far.
	*/

	ASSERT (foi != NULL);
	ASSERT (foc->scsi_foi);


	/*
	** Make certain that the group name is the same and
	** that the instance points to the right candidate table.
	*/

	if (foi->foi_foc != foc || strcmp (foi->foi_grp_name,foi_name) != 0) {
		error = EBADF;
		goto update_done;
	}

	/*
	** Check to see if removing path from instance.
	*/
	for (i=0; i<MAX_FO_PATHS; i++) {
		found_match=0;
		for (j=0; j<paths; j++) {
			if (foi->foi_path[i].foi_path_vhdl == lun_vhdl[j]) {
				found_match=1;
				break;
			}
		}
		if (!found_match && foi->foi_path[i].foi_path_status != FOI_PATHSTAT_INVALID) {
			if (i == foi->foi_primary) {	/* Don't remove primary! */
				DBG(2,"attempt to remove primary!\n");
				error = EBUSY;
				goto update_done;
			}

			/*
			** Allow access to device and adjust inventory.
			*/
			fo_enable_access(foi->foi_path[i].foi_path_vhdl, foi, 1);

			/*
			** Mirror (restore) partitions to device.
			*/
			fo_mirror_lun ( foi->foi_path[foi->foi_primary].foi_path_vhdl,
					foi->foi_path[i].foi_path_vhdl);

			/*
			** Remove the path from failover instance.
			*/
			DBG(2,"marking path %d invalid, clearing path vhdl of %d\n",
				i,foi->foi_path[i].foi_path_vhdl);
			foi->foi_path[i].foi_path_status = FOI_PATHSTAT_INVALID;
			foi->foi_path[i].foi_path_vhdl = 0;
		}
	}

	/*
	** Check to see if adding path to instance.
	*/
	for (i=0; i<paths; i++) {
		found_match=0;
		for (j=0; j<MAX_FO_PATHS; j++) {
			if (foi->foi_path[j].foi_path_vhdl == lun_vhdl[i]) {
				found_match=1;
				break;
			}
		}
		if (!found_match) {
			/*
			** Add the path.
			*/
			inq = inquiry[i];
			is_viable_primary = ((inq[0] & 0x60) == 0);
			error = fo_path_add(foi, lun_vhdl[i], is_viable_primary);
			DBG (2,"fo_scsi_device_update_generic: called fo_path_add, "
				"foi 0x%x, rc (%d)\n",(void *)foi,error);
			if (error) {
				break;
			}
		}
	}


update_done:

	if (error && is_new_instance)
		fo_remove_instance(foc,foi);

	vsema(foc->scsi_foi_sema4);

bail_out:

	DBG (2,"fo_scsi_device_update_generic: exit. (%d)\n",error);
	return error;
}

static void
fo_remove_instance (struct scsi_candidate *foc, struct scsi_fo_instance *foi)
{

	/*
	** Unlink and free the specified instance from the
	** candidate table.
	*/

	void (*free_func)();
	struct scsi_fo_instance *prev, *next;

	if (!foc || !foi) return;

	free_func = (void (*)())foc->scsi_freename_func;
	if (!free_func) free_func = (void (*)())kern_free;

	prev = foc->scsi_foi;

	if (prev == foi) {	/* first one */
		foc->scsi_foi = foi->foi_next;
		free_func (foi->foi_grp_name);
		kern_free (foi);
	}
	else {
		next = prev->foi_next;
		while (next && next != foi) {
			prev = next;
			next = next->foi_next;
		}
		if (next == foi) {	/* found it */
			prev->foi_next = foi->foi_next;
			free_func (foi->foi_grp_name);
			kern_free (foi);
		}
	}
}

static int
fo_path_add (struct scsi_fo_instance *foi, vertex_hdl_t lun_vhdl, int is_viable_primary)
{
	int i, error;
	char path_name[MAXDEVNAME];
	scsi_lun_info_t		*lun_info;

	/*
	 * Find an unused path slot
	 */
	
	error = 0;

	for (i=0; ; ++i) {

		if (i == MAX_FO_PATHS) {

			bzero(path_name, MAXDEVNAME);

			ASSERT (hwgraph_vertex_name_get(lun_vhdl, path_name, MAXDEVNAME)
			    == GRAPH_SUCCESS);

			cmn_err(CE_WARN, "%s: Failover group alternate path count exceeds %d,"
					 " lun not part of failover group.\n",
					 path_name, MAX_FO_PATHS);

			error = ENOMEM;
			break;
		}
	
		if (foi->foi_path[i].foi_path_status == FOI_PATHSTAT_INVALID) {

			foi->foi_path[i].foi_path_vhdl = lun_vhdl;
			foi->foi_path[i].foi_path_status = FOI_PATHSTAT_VALID;
			lun_info = scsi_lun_info_get(lun_vhdl);
			SLI_FO_INFO(lun_info) = foi;

			if (foi->foi_primary == FOI_NO_PRIMARY) {
				if (is_viable_primary) {
					DBG (2,"PRIMARY is %d\n",lun_vhdl);
					foi->foi_primary = i;
					fo_enable_access(lun_vhdl, foi, 0);
				}
			}
			else if (foi->foi_foc == fo_candidate_generic)
				fo_restrict_access(lun_vhdl, foi, 0);
			break;
		}
	}

	return error;
}

/*
 * FAILOVER module primary entry point. Called from
 * scsi_device_update(). Determines if a device is a candidate for
 * failover and if so creates an instance structure for it.
 */
void
fo_scsi_device_update(u_char *inq, vertex_hdl_t lun_vhdl)
{
  int                      found_match=0;
  struct scsi_candidate   *foc;
  struct scsi_fo_instance *foi;
  int                      i;
  scsi_name_t              foi_name;
  u_char		   ctlr,targ,lun;
  scsi_lun_info_t         *lun_info;
  char                     path_name[MAXDEVNAME];
  int                      rc;
  int                      is_viable_primary;

  lun_info = scsi_lun_info_get(lun_vhdl);
  ctlr     = SLI_ADAP(lun_info);
  targ	   = SLI_TARG(lun_info);
  lun	   = SLI_LUN(lun_info);

  /*
   * Is this a viable primary path ?
   */
  is_viable_primary = ((inq[0] & 0x60) == 0);

  /*
   * If current LUN already has a pointer to the failover instance
   *   struct, then we've previously encountered this device, and it
   *   should be part of a failover instance.
   * If no primary has been found yet, and this lun has become eligible
   *   to be a primary, make it primary.
   * If it is a primary, then enable access, else restrict access.  It
   *   is unnecessary to do this when added to the foi, since this code
   *   will be executed on the second successful scsi_info.  Since the
   *   first will result in the device being created, ioconfig will force
   *   at least a second in setting ctlr number.
   */
  foi = SLI_FO_INFO(lun_info);
  if (foi) {
    for (i = 0; i < MAX_FO_PATHS; ++i)
      if (foi->foi_path[i].foi_path_vhdl == lun_vhdl)
	break;
    if (i == MAX_FO_PATHS)
      cmn_err(CE_PANIC, "fo_scsi_device_update: Path not found: foi 0x%x, lun_vhdl %d\n", foi, lun_vhdl);
    if (foi->foi_primary == FOI_NO_PRIMARY && is_viable_primary) {
      /*
       * Make this LUN the primary path
       */
      foi->foi_primary = i;
    }
    if (foi->foi_primary == i)
      fo_enable_access(lun_vhdl, foi, 0);
    else
      fo_restrict_access(lun_vhdl, foi, 0);
    return;
  }

  /*
   * Walk down candidates list looking for a match
   */
  for (foc = fo_candidates; foc->scsi_match_str1; ++foc) {
    if (strncmp(foc->scsi_match_str1,
		(char *)(inq+foc->scsi_match_off1),
		strlen(foc->scsi_match_str1)) == 0 &&
	strncmp(foc->scsi_match_str2,
		(char *)(inq+foc->scsi_match_off2),
		strlen(foc->scsi_match_str2)) == 0) {
      found_match = 1;
      break;
    }
  }
  if (!found_match)
    return;

  /*
   * Make sure the current LUN is valid, i.e. is bound, or is not
   * bound but does exist.
   */
  ASSERT(foc->scsi_validlun_func);
  if (!(*foc->scsi_validlun_func)(inq)) {
    DBG(2, "fo_scsi_device_update(%d,%d,%d) - invalid LUN\n", ctlr, targ, lun);
    return;
  }

  ASSERT(foc->scsi_getname_func);
  if ((*foc->scsi_getname_func)(lun_vhdl, &foi_name)) {
    DBG(1, "fo_scsi_device_update(%d,%d,%d) - scsi_getname_func failed\n", ctlr, targ, lun);
    return;
  }

  /*
   * Get pathname for debug
   */
  rc = hwgraph_vertex_name_get(lun_vhdl, path_name, MAXDEVNAME);
  ASSERT(rc == GRAPH_SUCCESS);

  /*
   * Lock instance list and walk down list of instances looking for a
   * match. If we match the name against an existing instance, we add
   * to the group, otherwise we create a new instance.
   */
  psema(foc->scsi_foi_sema4, PRIBIO);
  if (foc->scsi_foi) {
    for (foi = foc->scsi_foi; foi; foi = foi->foi_next) {
      ASSERT(foc->scsi_cmpname_func);
      if ((rc = (*foc->scsi_cmpname_func)(foi->foi_grp_name, foi_name)) != FO_NAMECMP_FAIL) {
#ifdef DEBUG
	/*
	 * Got a match -- make sure it's not a duplicate
	 */
	for (i = 0; i < MAX_FO_PATHS; ++i) {
	  if (foi->foi_path[i].foi_path_vhdl == lun_vhdl)
	    ASSERT(foi->foi_path[i].foi_path_vhdl != lun_vhdl);
	}
#endif
	DBG(10, "fo_scsi_device_update(%d,%d,%d) -- %s %s - %s\n",
	        ctlr, targ, lun,
	        path_name,
	        (*foc->scsi_sprintf_func)(foi_name),
	        (rc == FO_NAMECMP_SUCCESS) ? "IS MATCH": "IS REPLACE MATCH");
	
	/*
	 * Find an unused path slot
	 */
	for (i = 0; ; ++i) {
	  if (i == MAX_FO_PATHS) {
		  char path_name[MAXDEVNAME];
		  /* REFERENCED */
		  int  rc;
		
		  bzero(path_name, MAXDEVNAME);
		  rc = hwgraph_vertex_name_get(lun_vhdl, path_name, MAXDEVNAME);
		  ASSERT(rc == GRAPH_SUCCESS);
		  cmn_err(CE_WARN, "%s: Failover group alternate path count exceeds %d,"
				   " lun not part of failover group.\n",
			  path_name, MAX_FO_PATHS);
		  goto update_done;
          }
	  if (foi->foi_path[i].foi_path_status == FOI_PATHSTAT_INVALID) {
	    foi->foi_path[i].foi_path_vhdl = lun_vhdl;
	    foi->foi_path[i].foi_path_status = FOI_PATHSTAT_VALID;
	    SLI_FO_INFO(lun_info) = foi;
	    if (is_viable_primary) {
		    if (foi->foi_primary == FOI_NO_PRIMARY)
			    foi->foi_primary = i;
	    }


	    /*
	     * If it's a replace match, replace the old name with the new
	     */
	    if (rc == FO_NAMECMP_SUCCESS) {
	      (*foc->scsi_freename_func)(foi_name);
	    }
	    else {
	      (*foc->scsi_freename_func)(foi->foi_grp_name);
	      foi->foi_grp_name = foi_name;
	    }
	    goto update_done;
	  }
	}
      }
    } /* for (foi) */
  }

  /*
   * No match found or List is empty
   */
  DBG(10, "fo_scsi_device_update(%d,%d,%d) -- %s %s - NEW\n",
	  ctlr, targ, lun,
	  path_name,
	  (*foc->scsi_sprintf_func)(foi_name));
  foi = kern_calloc(1, sizeof(struct scsi_fo_instance));
  if (foc->scsi_foi)
    foi->foi_next = foc->scsi_foi;
  foc->scsi_foi = foi;
  foi->foi_foc = foc;
  foi->foi_grp_name = foi_name;
  if (is_viable_primary)
	foi->foi_primary = 0;
  else
	foi->foi_primary = FOI_NO_PRIMARY;
  foi->foi_path[0].foi_path_vhdl = lun_vhdl;
  foi->foi_path[0].foi_path_status = FOI_PATHSTAT_VALID;
  SLI_FO_INFO(lun_info) = foi;

 update_done:
  vsema(foc->scsi_foi_sema4);
  return;
}

/*
 * FAILOVER module primary entry point. Called when a LUN vertex is
 * about to be removed. If this path is the primary, and
 * fo_auto_failover_on_dev_deletion is set, a failover is initiated.
 */
void
fo_scsi_lun_remove(vertex_hdl_t lun_vhdl)
{
  int                      i, j;
  int			   pathcount;
  struct scsi_fo_instance *foi;
  struct scsi_candidate   *foc;
  scsi_lun_info_t         *lun_info;
  int                      rc;
  vertex_hdl_t             scsi_vhdl, s_scsi_vhdl;
  /* REFERENCED */
  graph_error_t		   rv;

  lun_info = scsi_lun_info_get(lun_vhdl);
  ASSERT(lun_info);
  foi = SLI_FO_INFO(lun_info);
  foc = foi->foi_foc;
  /*
   * Find this LUN
   */
  psema(foc->scsi_foi_sema4, PRIBIO);
  for (i = 0; i < MAX_FO_PATHS; ++i) {
    if (foi->foi_path[i].foi_path_vhdl == lun_vhdl)
      goto lun_found;
  }
  cmn_err(CE_ALERT, "fo_scsi_lun_remove: LUN not found %v\n", lun_vhdl);
  vsema(foc->scsi_foi_sema4);
  return;

 lun_found:
  pathcount = 0;
  for (j = 0; j < MAX_FO_PATHS; ++j) {
	  if (foi->foi_path[j].foi_path_status == FOI_PATHSTAT_INVALID)
		  continue;		/* WAS BREAK, WHICH SKIPPED CHECKING ALL POSSIBLE PATHS */
	  ++pathcount;
  }
  /*
   * Failover removed LUN only if it's the current primary,
   * fo_auto_failover_on_dev_deletion is set and a secondary path
   * exists
   */
  if ((i == foi->foi_primary) && fo_auto_failover_on_dev_deletion && (pathcount > 1)) {
    vsema(foc->scsi_foi_sema4);
    rv = hwgraph_traverse(lun_vhdl,
			  EDGE_LBL_SCSI,
			  &scsi_vhdl);
    ASSERT((rv == GRAPH_SUCCESS) &&
	   (scsi_vhdl != GRAPH_VERTEX_NONE));
    rc = fo_scsi_device_switch(scsi_vhdl, &s_scsi_vhdl);
    psema(foc->scsi_foi_sema4, PRIBIO);
    if (rc)
      cmn_err(CE_ALERT, "fo_scsi_lun_remove(): failover failed on %v: %d\n", scsi_vhdl, rc);
    hwgraph_vertex_unref(scsi_vhdl);
  }
  else {
	  /*
	   * If we've just removed the primary, but not done a
	   * failover, then we need to mark this group as "without
	   * primary".
	   */
	  if (i == foi->foi_primary)
		  foi->foi_primary = FOI_NO_PRIMARY;
  }
  foi->foi_path[i].foi_path_status = FOI_PATHSTAT_INVALID;
  foi->foi_path[i].foi_path_vhdl = 0;
  vsema(foc->scsi_foi_sema4);
  return;
}

static inventory_t *
fo_hwgraph_inventory_get(vertex_hdl_t lun_vhdl)
{

	invplace_t	 place;
	inventory_t	*pinv = NULL;
	vertex_hdl_t	 disk_vhdl;

	place.invplace_vhdl = GRAPH_VERTEX_NONE;

	if (hwgraph_traverse(lun_vhdl, EDGE_LBL_DISK, &disk_vhdl) == GRAPH_SUCCESS) {
		if (hwgraph_inventory_get_next(disk_vhdl, &place, &pinv) != GRAPH_SUCCESS)
			pinv = NULL;

		/*
		 * hwgraph_vertex_unref() is safe to call here (or at least
		 * as safe as calling after we return), because vertex removal
		 * is dependent on reference counts in the SCSI layer, not
		 * hwgraph reference counts.  Thus, the hwgraph reference
		 * counts only protect hwgfs and related uses, not SCSI
		 * access to the vertices.
		 */
		hwgraph_vertex_unref(disk_vhdl);
	}

	/*
	 * Look for INV_DISK inventory entry.
	 */
	while (pinv != NULL && pinv->inv_class != INV_DISK)
		pinv = pinv->inv_next;
	return pinv;
}

int
fo_scsi_device_switch(vertex_hdl_t p_vhdl, vertex_hdl_t *s_vhdl)
{
	return fo_scsi_device_switch_new(p_vhdl, s_vhdl, 0);
}

/*
 * FAILOVER module primary entry point. Called with a primary path
 * (scsi, char or block HWG vertex), performs failover and returns the
 * corresponding secondary path.
 *
 * DEVICE SEMANTICS: In most cases, the vhdl passed in corresponds to
 * that of the current primary path.  In those cases where the
 * failover group in question does *not* currently have a primary
 * path, the next available secondary path is picked instead.
 */
int
fo_scsi_device_switch_new(vertex_hdl_t p_vhdl, vertex_hdl_t *s_vhdl, int tresspass)
{
	int                      rc = FO_SWITCH_SUCCESS;
	int			 ret;
	char                     path_name[MAXDEVNAME];
	char                     p_base_name[MAXDEVNAME],
	s_base_name[MAXDEVNAME],
	tail_name[MAXDEVNAME];
	int                      i, j;
	int			 num_paths;
	vertex_hdl_t             tmp_vhdl;
	vertex_hdl_t             p_lun_vhdl, s_lun_vhdl;
	scsi_lun_info_t         *lun_info;
	struct scsi_candidate   *foc;
	struct scsi_fo_instance *foi;

	/*
	 * Find the lun_vhdl given the current path. Check that path is a
	 * char/block or a scsi device
	 */
	ret = hwgraph_traverse(p_vhdl, "../"EDGE_LBL_SCSI, &tmp_vhdl);
	if (ret == GRAPH_SUCCESS)
	{
		i = scsi_dev_info_get(tmp_vhdl) != NULL;
		hwgraph_vertex_unref(tmp_vhdl);
		if (i)
		{
			DBG(10, "fo_scsi_device_switch: %d is a hwg-scsi vertex\n", p_vhdl);
			goto switch_1;
		}
	}
	ret = hwgraph_traverse(p_vhdl, "../"EDGE_LBL_BLOCK, &tmp_vhdl);
	if (ret == GRAPH_SUCCESS)
	{
		hwgraph_vertex_unref(tmp_vhdl);
		DBG(10, "fo_scsi_device_switch: %d is a hwg-char/block vertex\n", p_vhdl);
		goto switch_1;
	}
	ret = hwgraph_traverse(p_vhdl, "../"EDGE_LBL_CHAR, &tmp_vhdl);
	if (ret == GRAPH_SUCCESS)
	{
		hwgraph_vertex_unref(tmp_vhdl);
		DBG(10, "fo_scsi_device_switch: %d is a hwg-char/block vertex\n", p_vhdl);
		goto switch_1;
	}
	cmn_err(CE_DEBUG, "fo_scsi_device_switch: %d is not a hwg-scsi/char/block vertex\n", p_vhdl);
	return(FO_SWITCH_INVALID_PATH);

switch_1:
	ret = hwgraph_vertex_name_get(p_vhdl, path_name, MAXDEVNAME);
	if (ret != GRAPH_SUCCESS)
	{
		cmn_err(CE_DEBUG, "fo_scsi_device_switch: hwgraph_vertex_name_get failed for vhdl %d\n",p_vhdl);
		return FO_SWITCH_INVALID_PATH;
	}
	DBG(10, "fo_scsi_device_switch: p_path = %s, p_vhdl = %d\n", path_name, p_vhdl);

	if ( fo_split_pathname(path_name, p_base_name, tail_name) < 0)
	{
		cmn_err(CE_DEBUG, "fo_scsi_device_switch: %s: failover not supported.\n",path_name);
		return FO_SWITCH_INVALID_PATH;
	}

	p_lun_vhdl = hwgraph_path_to_vertex(p_base_name);
	if (p_lun_vhdl == GRAPH_VERTEX_NONE)
	{
		cmn_err(CE_DEBUG, "fo_scsi_device_switch: %s: no lun vhdl.\n",path_name);
		return FO_SWITCH_INVALID_PATH;
	}
	hwgraph_vertex_unref(p_lun_vhdl);

	DBG(10, "fo_scsi_device_switch: p_base_path = %s, p_lun_vhdl = %d\n", p_base_name, p_lun_vhdl);

	/*
	 * Get the failover info for the primary lun_vhdl
	 */
	lun_info = scsi_lun_info_get(p_lun_vhdl);
	if (!lun_info)
	{
		cmn_err(CE_DEBUG, "fo_scsi_device_switch: %s: no lun info.\n",path_name);
		return FO_SWITCH_INVALID_PATH;
	}

	foi = SLI_FO_INFO(lun_info);
	if (foi == NULL)
	{
		DBG(10, "fo_scsi_device_switch: No failover info associated with lun vertex %d\n", p_lun_vhdl);
		return(FO_SWITCH_FAIL);
	}
	foc = foi->foi_foc;

	psema(foc->scsi_foi_sema4, PRIBIO);

	/*
	 * If only one path and it's currently the primary, return
	 * degenerate case.
	 */
	for (i = 0, num_paths = 0; i < MAX_FO_PATHS; ++i)
	{
		if (foi->foi_path[i].foi_path_status != FOI_PATHSTAT_INVALID)
			++num_paths;
	}

	/*
	 * Check if failover has already happened. If the current primary
	 * path is equal to p_lun_vhdl or there currently is no primary,
	 * then failover has not been performed yet.
	 */
	if (foi->foi_primary == FOI_NO_PRIMARY || foi->foi_path[foi->foi_primary].foi_path_vhdl == p_lun_vhdl)
	{
		int	 unclun_pend = 1;
		u_char	 path_stat;
		int	 original_primary;

		original_primary = foi->foi_primary;
		if (foi->foi_primary == FOI_NO_PRIMARY)
			i = 0;
		else {
			i = foi->foi_primary + 1;
			if (i == MAX_FO_PATHS)
				i = 0;
			foi->foi_primary = FOI_NO_PRIMARY;
		}
		j = 0;

		/*
		 * Find an unfailed secondary path and try to make it primary. If no
		 * path works return FO_SWITCH_PATHS_EXHAUSTED.
		 *
		 * If there is a device specific switch routine for the new path,
		 * try to do an Inquiry first, to see if the switch can be avoided,
		 * unless the tresspass variable is set, in which case we want to
		 * take the first path.
		 * If Inquiry's have been done to all paths, and only unconnected luns were
		 * found, then call the device specific switch routine for the new path.
		 */
		do
		{
			path_stat = foi->foi_path[i].foi_path_status;
			s_lun_vhdl = foi->foi_path[i].foi_path_vhdl;
			DBG(3, "fo_scsi_device_switch: trying path %d status %d\n\tpath %v\n",
			    i, path_stat, s_lun_vhdl);
			if (path_stat == FOI_PATHSTAT_VALID) {
				if (!foc->scsi_switch_func)
					foi->foi_primary = i;
				else if (!tresspass) {
					int	inqtest;
					inqtest = fo_inquiry_test(s_lun_vhdl);
					if (inqtest == 0) {
						/*
						 * If the original path is the only one that does
						 * not require tresspass, but there are other paths
						 * that could be tresspassed, then try the tresspass
						 * first.
						 * A side effect of a successful inquiry test is
						 * that the primary is set (in fo_scsi_device_update),
						 * so we need not set it, but we need to set it to
						 * no primary if we don't want an early exit.
						 */
						if (original_primary == i) {
							if (num_paths == 1)
								rc = FO_SWITCH_ONEPATHONLY;
							if (j < MAX_FO_PATHS*(unclun_pend-1))
								foi->foi_primary = FOI_NO_PRIMARY;
						}
					}
					else if (inqtest == 0x20) {
						foi->foi_path[i].foi_path_status = FOI_PATHSTAT_UNCLUN;
						unclun_pend = 2;
					} else
						foi->foi_path[i].foi_path_status = FOI_PATHSTAT_FAILED;
				}
				else
					goto try_tresspass;
			}
			else if (path_stat == FOI_PATHSTAT_UNCLUN) {
try_tresspass:
				if ((*foc->scsi_switch_func)(p_lun_vhdl, s_lun_vhdl) == 0)
					foi->foi_primary = i;
			}
			i++;
			if (i == MAX_FO_PATHS)
				i = 0;
			j++;
		}
		while (foi->foi_primary == FOI_NO_PRIMARY && (j < MAX_FO_PATHS*unclun_pend));

		if (foi->foi_primary == FOI_NO_PRIMARY) {
			/*
			 * No valid paths found or all paths have failed.
			 * Make all failed paths valid and keep primary set to what it was on entry.
			 */
			rc = FO_SWITCH_PATHS_EXHAUSTED;
			for (i = 0; i < MAX_FO_PATHS; ++i) {
				if (foi->foi_path[i].foi_path_status != FOI_PATHSTAT_INVALID)
					foi->foi_path[i].foi_path_status = FOI_PATHSTAT_VALID;
			}
			goto switch_done;
		}
	}
	else
	{
		s_lun_vhdl = foi->foi_path[foi->foi_primary].foi_path_vhdl;
	}
	ret = hwgraph_vertex_name_get(s_lun_vhdl, s_base_name, MAXDEVNAME);
	ASSERT(ret == GRAPH_SUCCESS);
	DBG(10, "fo_scsi_device_switch: s_base_path = %s, s_lun_vhdl = %d\n", s_base_name, s_lun_vhdl);

	/* reset 'failed' paths to valid after a good switch */
	for (i = 0; i < MAX_FO_PATHS; ++i)
		if (foi->foi_path[i].foi_path_status != FOI_PATHSTAT_INVALID)
			foi->foi_path[i].foi_path_status = FOI_PATHSTAT_VALID;

	/*
	 * [Possibly] create disk and partition vertices for secondary
	 * path. Mirror those of the primary path
	 */
	if (foc->scsi_vh_coherency_policy == FO_VH_COH_POLICY_LAZY)
		fo_mirror_lun(p_lun_vhdl, s_lun_vhdl);

	if (p_lun_vhdl != s_lun_vhdl)
	{
		fo_enable_access(s_lun_vhdl, foi, 0);
		fo_restrict_access(p_lun_vhdl, foi, 1);
	}

	strcat(s_base_name, tail_name);
	*s_vhdl = hwgraph_path_to_vertex(s_base_name);
	hwgraph_vertex_unref(*s_vhdl);
	ASSERT(*s_vhdl != GRAPH_VERTEX_NONE);
	DBG(10, "fo_scsi_device_switch: s_path = %s, s_vhdl = %d\n", s_base_name, *s_vhdl);

switch_done:
	vsema(foc->scsi_foi_sema4);
	return(rc);
}

/*
 * FAILOVER module primary entry point. Called with a path (scsi, char
 * or block HWG vertex) and returns the number of paths in that
 * failover group.
 */
int
fo_scsi_device_pathcount(vertex_hdl_t vhdl)
{
  int                      rc;
  char                     path_name[MAXDEVNAME];
  char                     base_name[MAXDEVNAME],
                           tail_name[MAXDEVNAME];
  int                      i;
  vertex_hdl_t             tmp_vhdl;
  vertex_hdl_t             lun_vhdl;
  scsi_lun_info_t         *lun_info;
  struct scsi_fo_instance *foi;
  int                      pathcount;

  /*
   * Find the lun_vhdl given the current path. Check that path is a
   * char/block or a scsi device
   */
  rc = hwgraph_traverse(vhdl, "../"EDGE_LBL_SCSI, &tmp_vhdl);
  if (rc == GRAPH_SUCCESS) {
    hwgraph_vertex_unref(tmp_vhdl);
    DBG(10, "fo_scsi_device_pathcount: %d is a hwg-scsi vertex\n", vhdl);
    goto pathcount_1;
  }
  rc = hwgraph_traverse(vhdl, "../"EDGE_LBL_BLOCK, &tmp_vhdl);
  if (rc == GRAPH_SUCCESS) {
    hwgraph_vertex_unref(tmp_vhdl);
    DBG(10, "fo_scsi_device_pathcount: %d is a hwg-char/block vertex\n", vhdl);
    goto pathcount_1;
  }
  rc = hwgraph_traverse(vhdl, "../"EDGE_LBL_CHAR, &tmp_vhdl);
  if (rc == GRAPH_SUCCESS) {
    hwgraph_vertex_unref(tmp_vhdl);
    DBG(10, "fo_scsi_device_pathcount: %d is a hwg-char/block vertex\n", vhdl);
    goto pathcount_1;
  }
  cmn_err(CE_DEBUG, "fo_scsi_device_pathcount: %d is not a hwg-scsi/char/block vertex\n", vhdl);
  return(FO_PATHCOUNT_FAIL);

 pathcount_1:

  rc = hwgraph_vertex_name_get(vhdl, path_name, MAXDEVNAME);
  if (rc != GRAPH_SUCCESS) {
	cmn_err(CE_DEBUG, "fo_scsi_device_pathcount: hwgraph_vertex_name_get failed for vhdl %d\n",vhdl);
	return FO_PATHCOUNT_FAIL;
  }

  DBG(10, "fo_scsi_device_pathcount: path = %s, vhdl = %d\n", path_name, vhdl);
  if ( fo_split_pathname(path_name, base_name, tail_name) < 0) {
	cmn_err(CE_DEBUG, "fo_scsi_device_pathcount: %s: failover not supported.\n",path_name);
	return FO_PATHCOUNT_FAIL;
  }

  lun_vhdl = hwgraph_path_to_vertex(base_name);
  if (lun_vhdl == GRAPH_VERTEX_NONE) {
	cmn_err(CE_DEBUG, "fo_scsi_device_pathcount: %s: no lun vhdl.\n",path_name);
	return FO_PATHCOUNT_FAIL;
  }

  DBG(10, "fo_scsi_device_pathcount: base_path = %s, lun_vhdl = %d\n", base_name, lun_vhdl);

  /*
   * Get the failover info for the lun_vhdl
   */
  lun_info = scsi_lun_info_get(lun_vhdl);
  if (!lun_info) {
	cmn_err(CE_DEBUG, "fo_scsi_device_pathcount: %s: no lun info.\n",path_name);
	return FO_PATHCOUNT_FAIL;
  }

  foi = SLI_FO_INFO(lun_info);
  if (foi == NULL) {
    DBG(10, "fo_scsi_device_pathcount: No failover info associated with lun vertex %d\n",
	lun_vhdl);
    return(FO_PATHCOUNT_FAIL);
  }
  pathcount = 0;
  for (i = 0; i < MAX_FO_PATHS; ++i) {
    if (foi->foi_path[i].foi_path_status == FOI_PATHSTAT_INVALID)
      continue;		/* WAS BREAK, WHICH RESULTED IN MISSING PATHS */
    ++pathcount;
  }
  return(pathcount);
}

/*
 * FAILOVER module primary entry point. Returns non-zero if the device is
 * a candidate for failover, else zero.
 */
/* ARGSUSED */
int
fo_is_failover_candidate(u_char *inq, vertex_hdl_t lun_vhdl)
{
  int                      found_match=0;
  struct scsi_candidate   *foc;

  /*
   * Walk down candidates list looking for a match
   */
  for (foc = fo_candidates; foc->scsi_match_str1; ++foc) {
    if (strncmp(foc->scsi_match_str1,
		(char *)(inq+foc->scsi_match_off1),
		strlen(foc->scsi_match_str1)) == 0 &&
	strncmp(foc->scsi_match_str2,
		(char *)(inq+foc->scsi_match_off2),
		strlen(foc->scsi_match_str2)) == 0) {
      found_match = 1;
      break;
    }
  }
  return(found_match);
}

/*
 * Splits a HWG path name into a base path name (ending at the LUN
 * vertex) and a tail path name (everything past the LUN vertex).
 *	Returns -1 on encountering an invalid hwgraph path,
 *	zero otherwise.
 */
static int
fo_split_pathname(char *in_path, char *base_path, char *tail_path)
{
  register char *p, *p1, *p2;

  p = strstr(in_path, "/" EDGE_LBL_LUN "/");

  if (p == NULL) return -1;

  p += strlen("/" EDGE_LBL_LUN "/");
  while (*p != '/')
    ++p;
  strcpy(tail_path, p);
  p1 = in_path;
  p2 = base_path;
  while (p1 < p)
    *p2++ = *p1++;
  *p2 = '\0';
  return 0;
}


/*
 * Given primary and secondary LUN vertices, mirrors scsi and
 * partition vertices hanging off the primary onto the secondary.
 */
static void
fo_mirror_lun(vertex_hdl_t p_lun_vhdl, vertex_hdl_t s_lun_vhdl)
{
  vertex_hdl_t p_scsi_vhdl, s_scsi_vhdl;
  vertex_hdl_t p_disk_vhdl, s_disk_vhdl;
  u_char       part;
  int          rc;

  /*
   * If SCSI vertex exists, mirror it
   */
  rc = hwgraph_traverse(p_lun_vhdl, EDGE_LBL_SCSI, &p_scsi_vhdl);
  if (rc == GRAPH_SUCCESS) {
    hwgraph_vertex_unref(p_scsi_vhdl);
    rc = hwgraph_traverse(s_lun_vhdl, EDGE_LBL_SCSI, &s_scsi_vhdl);
    if (rc == GRAPH_NOT_FOUND)
      scsi_dev_add(s_lun_vhdl);
    else if (rc == GRAPH_SUCCESS)
      hwgraph_vertex_unref(s_scsi_vhdl);
  }

  /*
   * If disk vertices exist, mirror them.
   *    B_TRUE indicates that the alias entries should be created.
   */
  rc = hwgraph_traverse(p_lun_vhdl, EDGE_LBL_DISK, &p_disk_vhdl);
  if (rc == GRAPH_SUCCESS) {
    hwgraph_vertex_unref(p_disk_vhdl);

    rc = hwgraph_traverse(s_lun_vhdl, EDGE_LBL_DISK, &s_disk_vhdl);
    if (rc == GRAPH_NOT_FOUND)
      s_disk_vhdl = scsi_disk_vertex_add(s_lun_vhdl, INV_SCSIDRIVE);
    else if (rc == GRAPH_SUCCESS)
      hwgraph_vertex_unref(s_disk_vhdl);

    if (rc == GRAPH_NOT_FOUND || rc == GRAPH_SUCCESS) {
      dk_base_namespace_add(s_disk_vhdl, B_TRUE);
      for (part = 0; part < DK_MAX_PART; ++part) {
	vertex_hdl_t	p_part_vhdl, s_part_vhdl;

	if (scsi_part_vertex_exist(p_disk_vhdl, part, &p_part_vhdl) &&
	   !scsi_part_vertex_exist(s_disk_vhdl, part, &s_part_vhdl))
	  (void) scsi_part_vertex_add(s_disk_vhdl, part, B_TRUE);
      }
    }
  }
}

static void
fo_scsi_done(scsi_request_t *req)
{
  vsema(req->sr_dev);
}

static int
dg_sauna_validlun_func(u_char *inq)
{
  /*
   * On DG RAID, a LUN can be in one of three states:
   *                   Peripheral Qualifier   Device-Type Modifier
   *  Doesn't Exist              1                       0
   *  Not Assigned               1                       > 0
   *  Assigned                   0                       > 0
   *
   *  Return: 0, lun invalid
   *          1, lun valid
   */
  return( ! ( ( (inq[0]&0xE0) == 0x20 ) && ( (inq[1] & 0x7F) == 0) ) );
/*              peripheral_qualifier == 001b (target capabile of supporting device, but not currently connected */
/*              device_type_modifier == 00000b (direct access device) */
}

static int
dg_sauna_getname_func(vertex_hdl_t lun_vhdl, scsi_name_t *ret_name)
{
  scsi_lun_info_t     *lun_info;
  scsi_ctlr_info_t    *ctlr_info;
  struct fo_request   *fr;
  struct scsi_request *req;
  u_char              *cdb;
  u_char              *buf;
  uint32_t             sig, psig;
  struct dg_name      *name;
  int                  retry;
  int                  retval = 0;

#define PG25_LEN            20	    /* header plus 16 bytes of data */
#define DG_MODE_SENSE_TO    (10*HZ)
#define DG_MAX_RETRIES      5	    /* Maximum number of retries */

  lun_info = scsi_lun_info_get(lun_vhdl);
  ctlr_info = STI_CTLR_INFO(SLI_TARG_INFO(lun_info));

  /*
   * Do a MODE SENSE command of page 0x25 to get the signature and
   * peer signature.
   */
  for (retry = DG_MAX_RETRIES; ;) {
    fr = fo_get_fr();
    ASSERT(fr);
    req = fr->fr_srreq;
    cdb = req->sr_command;
    buf = req->sr_buffer;
    cdb[0] = 0x1A;		/* Mode sense of page 0x25 */
    cdb[1] = 0x08;		/* Disable return of block descriptor */
    cdb[2] = 0x25;
    cdb[4] = PG25_LEN;
    req->sr_lun_vhdl = lun_vhdl;
    req->sr_ctlr = SLI_ADAP(lun_info);
    req->sr_target = SLI_TARG(lun_info);
    req->sr_lun = SLI_LUN(lun_info);
    req->sr_cmdlen = SC_CLASS0_SZ;
    req->sr_flags = SRF_DIR_IN | SRF_AEN_ACK;
    req->sr_timeout = DG_MODE_SENSE_TO;
    req->sr_buflen = PG25_LEN;
    req->sr_notify = fo_scsi_done;

    DBG(10, "dg_sauna_getname_func: issuing mode sense command - fr 0x%x, req 0x%x\n", fr, req);
    ASSERT(req->sr_buffer);
    (*ctlr_info->sci_command)(req);
    psema(req->sr_dev, PRIBIO);
    DBG(10, "dg_sauna_getname_func: command done, req 0x%x\n", req);
    if (req->sr_status == 0 && req->sr_scsi_status == 0	&& req->sr_sensegotten == 0)
      break;
    SCSI_DBG(2, req, "dg_sauna_getname_func: Command failed: req 0x%x\n", req);
    if (retry-- == 0) {
      retval = -1;
      goto getname_done;
    }
    fo_free_fr(fr);
  }

  name = kern_calloc(1, sizeof(struct dg_name));
  sig = *(uint32_t *)(buf + 8);
  psig = *(uint32_t *)(buf + 12);
  ASSERT(sig != psig);
  if (sig < psig) {
    name->dg_sig1 = sig;
    name->dg_sig2 = psig;
  }
  else {
    name->dg_sig1 = psig;
    name->dg_sig2 = sig;
  }
  name->dg_lun = SLI_LUN(lun_info);
  *(dg_name_t *)ret_name = name;

 getname_done:
  fo_free_fr(fr);

  return(retval);
}

static int
dg_sauna_cmpname_func(scsi_name_t name1, scsi_name_t name2)
{
  register dg_name_t p1 = name1;
  register dg_name_t p2 = name2;

  if (p1->dg_lun == p2->dg_lun) {
    if ((p1->dg_sig1 == p2->dg_sig1) &&
	(p1->dg_sig2 == p2->dg_sig2))
      return(FO_NAMECMP_SUCCESS);
    else
    if ((p1->dg_sig1 == p2->dg_sig1 && p1->dg_sig1 != 0) ||
	(p1->dg_sig1 == p2->dg_sig2 && p1->dg_sig1 != 0) ||
	(p1->dg_sig2 == p2->dg_sig1 && p1->dg_sig2 != 0) ||
	(p1->dg_sig2 == p2->dg_sig2 && p1->dg_sig2 != 0))
      return(FO_NAMECMP_REPLACE);
    else
      return(FO_NAMECMP_FAIL);
  }
  else
    return(FO_NAMECMP_FAIL);
}

static int
dg_sauna_freename_func(scsi_name_t ret_name)
{
  kern_free(ret_name);
  return(0);
}

static char *
dg_sauna_sprintf_func(scsi_name_t name)
{
  static char ret_str[32];
  dg_name_t p = name;

  sprintf(ret_str, "%08w32x:%08w32x:%02d", p->dg_sig1, p->dg_sig2, p->dg_lun);
  return(ret_str);
}

/* ARGSUSED2 */
static int
dg_sauna_switch_func(vertex_hdl_t p_lun_vhdl, vertex_hdl_t s_lun_vhdl)
{
  scsi_lun_info_t     *lun_info;
  scsi_ctlr_info_t    *ctlr_info;
  struct fo_request   *fr;
  struct scsi_request *req;
  u_char              *cdb;
  u_char              *buf;
  int                  retry;
  int                  retval = 0;
  int                  err;

#define PG22_LEN            8	    /* header plus 4 bytes of data */
#define DG_INQUIRY_TO       (10*HZ) /* Time interval between retries */
#define DG_MAX_RETRIES      5	    /* Maximum number of retries */
#define DG_TRESPASS_TO	    (60*HZ)

  lun_info = scsi_lun_info_get(s_lun_vhdl);
  ctlr_info = STI_CTLR_INFO(SLI_TARG_INFO(lun_info));

  for (retry = DG_MAX_RETRIES; ; ) {
    if ((err = (SLI_ALLOC(lun_info))(s_lun_vhdl, 0, NULL)) == SCSIALLOCOK)
      break;
    DBG(1, "dg_sauna_switch_func: scsi_alloc(%d,%d,%d) failed: err = %d\n",
	SLI_ADAP(lun_info), SLI_TARG(lun_info), SLI_LUN(lun_info), err);
    if (retry-- == 0)
      return(-1);
    /*
     * the delay of 1000 results in a delay of 10 seconds.  The original
     * comment specified a delay of 1 mSec.  That comment was wrong.  I'm
     * uncertain if the delay value is wrong or not.  I'm not going to
     * change it at this point in the release.  mdr
     */
    delay(1000);
  }

  /*
   * Do the TRESPASS command.
   */
  for (retry = DG_MAX_RETRIES; ;) {
    fr = fo_get_fr();
    ASSERT(fr);
    req = fr->fr_srreq;
    cdb = req->sr_command;
    buf = req->sr_buffer;
    cdb[0] = 0x15;		/* Mode select of page 0x22 */
    cdb[4] = PG22_LEN;
    req->sr_lun_vhdl = s_lun_vhdl;
    req->sr_ctlr = SLI_ADAP(lun_info);
    req->sr_target = SLI_TARG(lun_info);
    req->sr_lun = SLI_LUN(lun_info);
    req->sr_cmdlen = SC_CLASS0_SZ;
    req->sr_flags = SRF_AEN_ACK;
    req->sr_timeout = 60 * HZ;
    req->sr_buflen = PG22_LEN;
    req->sr_notify = fo_scsi_done;
    bzero(buf, PG22_LEN);
    buf[4] = 0x22;		/* Trespass page */
    buf[5] = 0x02;		/* Length */
    buf[6] = 0x01;		/* Trespass only this LUN */
    buf[7] = SLI_LUN(lun_info);	/* LUN number */

    DBG(10, "dg_sauna_switch_func: issuing trespass command - fr 0x%x, req 0x%x\n", fr, req);
    ASSERT(req->sr_buffer);
    (*ctlr_info->sci_command)(req);
    DBG(10, "dg_sauna_switch_func: command done, req 0x%x\n", req);
    psema(req->sr_dev, PRIBIO);
    if (req->sr_status == 0 && req->sr_scsi_status == 0	&& req->sr_sensegotten == 0)
      break;
    SCSI_DBG(2, req, "dg_sauna_switch_func: Command failed: req 0x%x\n", req);
    if (retry-- == 0) {
      retval = -1;
      goto switch_done;
    }
    fo_free_fr(fr);
  }

 switch_done:
  fo_free_fr(fr);
  SLI_FREE(lun_info)(s_lun_vhdl, NULL);

  return(retval);
}

/*
 * return:	EBADF if the chosen primary lun isn't viable (unconnected lun)
 *		EINVAL if there is no foi for the chosen lun
 */
static int
clariion_k5_primary_func(struct scsi_candidate *foc, struct user_fo_generic_info *fgi)
{
	struct scsi_fo_instance	*foi;
	vertex_hdl_t		 lun_vhdl;
	int			 i, j;

	if (fgi->fgi_inq_data[0][0] & 0x60)
		return EBADF;

	/*
	 * Look through foi's to find one with this lun vhdl
	 */
	lun_vhdl = fgi->fgi_lun_vhdl[0];
	DBG (3, "Attempting to mark %d as primary in its failover group\n", lun_vhdl);
	foi = foc->scsi_foi;
	while (foi != NULL)
	{
		for (i = 0; i < MAX_FO_PATHS; i++)
		{
			if (foi->foi_path[i].foi_path_vhdl == lun_vhdl &&
			    foi->foi_path[i].foi_path_status == FOI_PATHSTAT_VALID)
				goto found_foi;
		}
		foi = foi->foi_next;
	}
	return EINVAL;

found_foi:
	foi->foi_restrict = 1;
	DBG(3, "Found %d in foi 0x%x, path %d\n", lun_vhdl, foi, i);
	for (j = 0; j < MAX_FO_PATHS; j++)
	{
		if (j == i || foi->foi_path[j].foi_path_status == FOI_PATHSTAT_INVALID)
			continue;
		fo_restrict_access(foi->foi_path[j].foi_path_vhdl, foi, 0);
	}
	fo_enable_access(lun_vhdl, foi, 0);
	foi->foi_primary = i;
	return 0;
}


static int
fo_generic_switch_func (vertex_hdl_t p_lun_vhdl, vertex_hdl_t s_lun_vhdl)
{
  scsi_lun_info_t     *lun_info;
  int                  retry;
  int                  retval = 0;
  int                  err;

  p_lun_vhdl = p_lun_vhdl;	/* remove compiler remark */

  lun_info = scsi_lun_info_get(s_lun_vhdl);

  for (retry = 5; ; ) {
    if ((err = (SLI_ALLOC(lun_info))(s_lun_vhdl, 0, NULL)) == SCSIALLOCOK)
      break;
    DBG(1, "fo_generic_switch_func: scsi_alloc(%d,%d,%d) failed: err = %d\n",
	SLI_ADAP(lun_info), SLI_TARG(lun_info), SLI_LUN(lun_info), err);
    if (retry-- == 0)
      return(-1);
    delay(100);		/* Delay 1 sec */
  }

  SLI_FREE(lun_info)(s_lun_vhdl, NULL);

  return(retval);
}


static int
fo_inquiry_test(vertex_hdl_t s_lun_vhdl)
{
	struct scsi_target_info	*info;
	scsi_lun_info_t		*lun_info;

	lun_info = scsi_lun_info_get(s_lun_vhdl);
	info = SLI_INQ(lun_info)(s_lun_vhdl);

	DBG(1, "fo_inquiry_test: scsi_alloc(%d,%d,%d) returned 0x%x (0x%x)\n",
	    SLI_ADAP(lun_info), SLI_TARG(lun_info), SLI_LUN(lun_info), info,
	    info ? info->si_inq[0] : -1);

	if (info != SCSIDRIVER_NULL)
	      return (int) (uint) info->si_inq[0];

	return -1;
}
