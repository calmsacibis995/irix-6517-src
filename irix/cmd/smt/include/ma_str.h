#ifndef MA_STR_H
#define MA_STR_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.11 $
 */

extern char *ma_pcm_flag_str(u_long, char *);
extern char *ma_pc_signal_str(u_long, int, char *);

extern char *ma_mac_state_str(enum mac_states, int);
extern char *ma_pc_type_str(enum fddi_pc_type);
extern char *ma_st_type_str(enum fddi_st_type);
extern char *ma_pcm_tgt_str(enum pcm_tgt);
extern char *ma_pcm_state_str(enum pcm_state);
extern char *ma_rt_ls_str(enum rt_ls);
extern char *ma_rt_ls_str2(enum rt_ls);
extern char *ma_pcm_ls_str(enum pcm_ls);
extern char *ma_fc_str(u_char);
extern char *ma_ft_str(u_char);
extern char *ma_rc_str(u_long);
extern char *ma_mac_bits_str(u_char, char*);
extern char *ma_mac_fc_str(u_char, char*);

#endif
