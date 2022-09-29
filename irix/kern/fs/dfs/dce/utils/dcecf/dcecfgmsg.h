/* Generated from ../../../../../../src/dce/utils/dcecf/cfg.sams on 1999-11-07-04:40:29.000 */
/* Do not edit! */
#if	!defined(_DCE_DCECFGMSG_)
#define _DCE_DCECFGMSG_
#define dce_cf_st_ok                    0x00000000
#define dce_cf_e_no_match               0x0000ff01
#define dce_cf_e_no_mem                 0x0000ff02
#define dce_cf_e_file_open              0x0000ff03

#define smallest_cfg_message_id         0x00000000
#define biggest_cfg_message_id          0x0000ff03

#if	defined(_DCE_MSG_H)
extern dce_msg_table_t dce_cfg_g_table[3];
#endif	/* defined(_DCE_MSG_H) */
#endif	/* !defined(_DCE_DCECFGMSG_) */
