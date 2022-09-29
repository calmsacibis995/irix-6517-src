#ifndef _AGENT_STRUCTS_H_
#define _AGENT_STRUCTS_H_

#include <sys/types.h>
#include <time.h>
#include <ulocks.h>

#include "esi.h"

#define STS_INVALID      255
#define STS_OK           0
#define STS_OFF          1
#define STS_FAILED       2
#define STS_NOT_PRESENT  3
#define STS_BYPASSED     4
#define STS_PEER_FAILED  5

typedef struct {
  u_char status;
  u_char last_rptd_status;
} ps_e_struct_t;
typedef ps_e_struct_t *ps_e_t;

typedef struct {
  u_char status;
  u_char last_rptd_status;
} fan_e_struct_t;
typedef fan_e_struct_t *fan_e_t;

typedef struct {
  u_char             status;
  u_char             last_rptd_status;
  u_char             phys_id;
  unsigned long long wwn;
  u_char             connect_port;		/* Thru which FC-AL port is this connected (0 or 1) */
  u_char             drv_asserting_fault;
  u_char             enc_asserting_fault;
  u_char             drv_asserting_bypass;
  u_char             enc_asserting_bypass;
} drv_e_struct_t;
typedef drv_e_struct_t *drv_e_t;

typedef struct {
  u_char             status;
  u_char             last_rptd_status;
  u_char             connect_port;		/* Thru which FC-AL port is this connected (0 or 1) */
  u_char             peer_present;
  u_char             peer_failed;
  u_char             position;	                /* 0 = vertical, 1 = horizontal */
  u_char             local_mode;
  u_char             exp_port_open;             /* 1 = last physical enclosure on loop */
  u_char             exp_shunt_closed;          /* 1 = last logical enclosure on loop  */
} lcc_e_struct_t;
typedef lcc_e_struct_t *lcc_e_t;

typedef struct encl_e {
  u_char                      phys_id;
  u_char                      status;
  u_char                      esidrv[2];	/* Primary and secondary 8067 access drive */
  u_char                      connect_port;	/* Thru which FC-AL port is this connected (0 or 1) */
  u_int                       config_gen_code;	/* Configuration generation code */
  u_int                       status_gen_code;	/* Status generation code */
  char                        vid[8];		/* Vendor ID */
  char                        pid[8];		/* Product ID */
  char                        prl[8];		/* Rev. Level */
  unsigned long long          wwn;              /* Unique WWN */
  char                       *lcc_str;          /* LCC string */
  u_int                       lcc_str_len;      /* Length of LCC string */
  u_char                      type_count;	/* Number of type descriptors */
  esi_pg1_type_desc_struct_t *types;            /* Low level type descriptors */
  u_char                      drv_count;	/* Number of drives in box */
  u_char                      ps_count;		/* Number of power-supplies in box */
  u_char                      fan_count;	/* Number of fans in box */
  u_char                      lcc_count;	/* Number of FCC cards in box */
  drv_e_struct_t             *drv;		/* Array of drive status structs */
  ps_e_struct_t              *ps;		/* Array of PS structs */
  fan_e_struct_t             *fan;		/* Array of fan status structs */
  lcc_e_struct_t             *lcc;		/* Array of LCC status structs */
  /* ------------------------------------------ */
  u_int                       stat_blen_alloc;  /* Allocated size of status buffer */
  u_int                       stat_blen_actual; /* Actual size of status buffer */
  esi_pg2_t                   stat_buf;         /* Buffer used for status/controller */
  time_t                      read_gen_code;
  u_int                       check_gen_code;
  ulock_t                     lock;             /* Access lock */
} encl_e_struct_t;
typedef encl_e_struct_t *encl_e_t;

typedef struct encl_ref {
  struct encl_e   *encl;
  struct encl_ref *next;
} encl_ref_struct_t;
typedef encl_ref_struct_t *encl_ref_t;

typedef struct channel_e {
  uint             id;
  u_char           type;
  encl_ref_t       encl_ref_head, encl_ref_tail;
  struct channel_e *next;
} channel_e_struct_t;
typedef channel_e_struct_t *channel_e_t;

#endif
