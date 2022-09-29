#ifndef _ESI_H_
#define _ESI_H_

/* ESI page numbers */
#define RECV_SUPPORTED_PAGES     0x0
#define RECV_ES_CONFIGURATION    0x1
#define RECV_ES_ENCL_STATUS      0x2
#define SEND_ES_ENCL_CONTROL     0x2

#define CONFIG_BUFLEN  0x1000
#define STATUS_BUFLEN  0x1000

typedef struct {
  u_char page_code;
  u_char rsvd1;
  ushort page_len;
  u_char *supp_pages;
} esi_pg0_struct_t;
typedef esi_pg0_struct_t *esi_pg0_t;

typedef struct {
  u_char page_code;
  u_char rsvd1;
  ushort page_len;
  u_int  gen_code;
  u_char glob_desc_len;
  u_char rsvd2;
  u_char num_types;
  u_char rsvd3;
  u_char enc_wwn[8];
  u_char enc_vid[8];
  u_char enc_pid[16];
  u_char enc_prl[4];
  u_char enc_phys_id;
} esi_pg1_struct_t;
typedef esi_pg1_struct_t *esi_pg1_t;

/* ESI element type definitions */
#define E_TYPE_DISK   0x01
#define E_TYPE_PS     0x02
#define E_TYPE_FAN    0x03
#define E_TYPE_UPS    0x0B
#define E_TYPE_LCC    0x80

typedef struct {
  u_char type_code;
  u_char elem_count;
  u_char ext:1, :7;
  u_char text_len;
} esi_pg1_type_desc_struct_t;
typedef esi_pg1_type_desc_struct_t *esi_pg1_type_desc_t;

typedef struct {
  u_char page_code;
  u_char esi_short:1,
         :3, 
         info:1,
         non_crit:1,
         crit:1,
         unrecov:1;
  ushort page_len;
  u_int  gen_code;
} esi_pg2_struct_t;
typedef esi_pg2_struct_t *esi_pg2_t;

#define E_ELEM_STS_UNSUPP   0x00 /* Element status detection not implement for element */
#define E_ELEM_STS_OK       0x01 /* Element is installed and OK */
#define E_ELEM_STS_CRIT     0x02 /* Critical element failure detected */
#define E_ELEM_STS_NONCRIT  0x03 /* Non-critical element failure detected */
#define E_ELEM_STS_UNRECOV  0x04 /* Unrecoverable element failure detected */
#define E_ELEM_STS_NOTINST  0x05 /* Element is detected to be not installed */
#define E_ELEM_STS_UNKNOWN  0x06 /* Element is detected to be not installed */
#define E_ELEM_STS_NOTAVAIL 0x07 /* Element installed but not powered up */

#define IS_E_ELEM_STS_OK(_sts_) \
  ((_sts_) == E_ELEM_STS_OK)
#define IS_E_ELEM_STS_FAILED(_sts_) \
  (((_sts_) == E_ELEM_STS_CRIT) || \
   ((_sts_) == E_ELEM_STS_NONCRIT) || \
   ((_sts_) == E_ELEM_STS_UNRECOV))
#define IS_E_ELEM_STS_NOT_PRESENT(_sts_) \
  (((_sts_) == E_ELEM_STS_NOTINST) || \
   ((_sts_) == E_ELEM_STS_NOTAVAIL))

typedef struct {
  u_char select:1,
         predfail:1,
         status:6;
  u_char elem_stat1;
  u_char elem_stat2;
  u_char elem_stat3;
} esi_pg2_gnrc_desc_struct_t;
typedef esi_pg2_gnrc_desc_struct_t *esi_pg2_gnrc_desc_t;

typedef struct {
  u_char select:1,
         predfail:1,
         status:6;
  u_char hard_address;
  u_char :1,
         do_not_remove:1,
         :1,
         swap:1,
         ready_to_insert:1,
         remove:1,
         identify:1,
         :1;
  u_char :1,
         sense_fault:1, 
         fault_reqstd:1,
         drive_off:1,
         enable_byp_A:1,
         enable_byp_B:1,
         byp_A_enabled:1,
         byp_B_enabled:1;
} esi_pg2_drv_desc_struct_t;
typedef esi_pg2_drv_desc_struct_t *esi_pg2_drv_desc_t;

typedef struct {
  u_char select:1,
         predfail:1,
         status:6;
  u_char hard_address;
  u_char :4,
         dc_over_voltage:1,
         dc_under_voltage:1,
         dc_over_current:1,
         :1;
  u_char :1,
         fail:1,
         rqsted_on:1,
         :1,
         overtemp_fail:1,
         temp_warning:1,
         ac_fail:1,
         dc_fail:1;
} esi_pg2_ps_desc_struct_t;
typedef esi_pg2_ps_desc_struct_t *esi_pg2_ps_desc_t;

typedef struct {
  u_char select:1,
         predfail:1,
         status:6;
  u_char rsvd1;
  u_char rsvd2;
  u_char :1,
         fail:1,
         rqsted_on:1,
         :2,
         speed_code:3;
} esi_pg2_fan_desc_struct_t;
typedef esi_pg2_fan_desc_struct_t *esi_pg2_fan_desc_t;

#define FAN_SPD_NORMAL 1

typedef struct {
  u_char select:1,
         predfail:1,
         status:6;
  u_char other_LCC_fault:1,
         other_LCC_inserted:1,
         :6;
  u_char rack_mounted:1,
         :1,
         exp_MC_fault:1,
         prim_MC_fault:1,
         :1,
         local_mode:1,
         exp_port_open:1,	/* Physically the last encl. on loop */
         exp_shunt_closed:1;	/* Logically the last encl. on loop */
  u_char :2,
         DAE_encl:1,		/* 0=DAE encl., 1=DPE encl. */
         LCC_slot:1,		/* 0=slot A, 1=slot B */
         frumon_rev:4;
} esi_pg2_lcc_desc_struct_t;
typedef esi_pg2_lcc_desc_struct_t *esi_pg2_lcc_desc_t;

#endif
