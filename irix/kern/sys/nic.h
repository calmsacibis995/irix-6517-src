#ifndef __NIC_H__
#define __NIC_H__

/*
 * nic.h
 *
 *      >>> Dallas number-in-can (NIC) public functions & defines. <<<
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include <sys/types.h>

#define MCR_DATA(x)			((int) ((x) & 1))
#define MCR_DONE(x)			((x) & 2)
#define MCR_PACK(pulse, sample)		((pulse) << 10 | (sample) << 2)

typedef __psunsigned_t	nic_data_t;

typedef int		
nic_access_f(nic_data_t data,
	     int pulse, int sample, int delay);

typedef nic_access_f   *nic_access_t;

typedef struct nic_vmce_s      *nic_vmce_t;
typedef void			nic_vmc_func(vertex_hdl_t v);

/*
 * PRIVATE data for Dallas NIC
 */

typedef struct nic_state_t {
    nic_access_t	access;
    nic_data_t		data;
    int			last_disc;
    int			done;
    int			bit_index;
    int			disc_marker;
    uchar_t		bits[64];
} nic_state_t;

/*
 * Public interface for Dallas NIC
 *
 *
 *   Access Routine
 *
 *   nic_setup requires an access routine that pulses the NIC line for a
 *   specified duration, samples the NIC line after a specified duration,
 *   then delays for a third specified duration (for precharge).
 *
 *   This general scheme allows us to access NICs through any medium
 *   (e.g. hub regs, bridge regs, vector writes, system ctlr commands).
 *
 *   The access routine should return the sample value 0 or 1, or if an
 *   error occurs, return a negative error code.  Negative error codes from
 *   the access routine will abort the NIC operation and be propagated
 *   through out of the top-level NIC call.
 */

#define NIC_OK			0
#define NIC_DONE		1
#define NIC_FAIL		2
#define NIC_BAD_CRC		3
#define NIC_NOT_PRESENT		4
#define NIC_REDIR_LOOP		5
#define NIC_PARAM		6
#define NIC_NOMEM		7

#ifdef SN
__uint64_t nic_get_phase_bits(void);
#endif

extern int nic_setup(nic_state_t *ns,
		     nic_access_t access,
		     nic_data_t data);

extern int nic_next(nic_state_t *ns,
		    char *serial,
		    char *family,
		    char *crc);

extern int nic_read_one_page(nic_state_t *ns,
			     char *family,
			     char *serial,
			     char *crc,
			     int start,
			     uchar_t *redirect,
			     uchar_t *byte);

extern int nic_read_mfg(nic_state_t *ns,
			char *family,
			char *serial,
			char *crc,
			uchar_t *pageA,
			uchar_t *pageB);

extern int nic_info_get(nic_access_t access,
			nic_data_t data,
			char *info);

extern int nic_item_info_get(char *buf, char *item, char **item_info);

#ifndef _STANDALONE
nic_access_f	nic_access_mcr32;

extern char *nic_vertex_info_get(vertex_hdl_t v);

extern char *nic_vertex_info_set(nic_access_t access,
				 nic_data_t data, 
				 vertex_hdl_t v);

extern int nic_vertex_info_match(vertex_hdl_t vertex,
				 char *name);

extern char *nic_bridge_vertex_info(vertex_hdl_t vertex,
				    nic_data_t	data);
extern char *nic_hq4_vertex_info(vertex_hdl_t vertex,
				 nic_data_t data);
extern char *nic_ioc3_vertex_info(vertex_hdl_t vertex,
				    nic_data_t	data,
				    __int32_t *gpcr_s);

#if SN
extern char *nic_hub_vertex_info(vertex_hdl_t vertex);
#endif

extern nic_vmce_t	nic_vmc_add(char *, nic_vmc_func *);
extern void		nic_vmc_del(nic_vmce_t);

#else
#if SN0
int klcfg_get_nic_info(nic_data_t, char *);
#else
int cfg_get_nic_info(nic_data_t, char *);
#endif	/* SN0 */
#endif	/* _STANDALONE */

#ifdef _STANDALONE
#if SN
extern int get_hub_nic_info(nic_data_t, char *) ;
#endif
#endif

#endif /* !__NIC_H__ */
