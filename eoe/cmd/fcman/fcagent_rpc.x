/*
 * PROTOCOL definitions for AGENT_GET_CONFIG	
 */
struct ch_cfg_t {
  u_int ch_id;
  u_int encl_id<>;
  u_int stat_gencode<>;
};
typedef struct ch_cfg_t cfg_t<>;
%#ifdef XDR_SHORTCUT
%#define encl_id_len encl_id.encl_id_len
%#define encl_id_val encl_id.encl_id_val
%#define stat_gencode_len stat_gencode.stat_gencode_len
%#define stat_gencode_val stat_gencode.stat_gencode_val
%#endif

/*
 * PROTOCOL definitions for AGENT_GET_STATUS	
 */
struct enc_addr_t {
  u_int ch_id;
  u_int encl_id;
};

typedef opaque drv_stat_t<>;
typedef opaque fan_stat_t<>;
typedef opaque ps_stat_t<>;
typedef opaque lcc_stat_t<>;

struct enc_stat_t {
  u_int      encl_id;
  u_int      encl_status;
  u_int      encl_phys_id;
  char       encl_vid[8];	/* Vendor ID */
  char       encl_pid[8];	/* Vendor ID */
  char       encl_prl[8];	/* Rev. Level */
  u_char     encl_wwn[8];	/* Unique WWN */
  char       encl_lccstr<>;	/* LCC text string */
  drv_stat_t drv<>;
  fan_stat_t fan<>;
  ps_stat_t  ps<>;
  lcc_stat_t lcc<>;
};
%#ifdef XDR_SHORTCUT
%#define encl_lccstr_len encl_lccstr.encl_lccstr_len
%#define encl_lccstr_val encl_lccstr.encl_lccstr_val
%#define drv_len drv.drv_len
%#define drv_val drv.drv_val
%#define fan_len fan.fan_len
%#define fan_val fan.fan_val
%#define ps_len  ps.ps_len
%#define ps_val  ps.ps_val
%#define lcc_len lcc.lcc_len
%#define lcc_val lcc.lcc_val
%#endif

/*
 * PROTOCOL definitions for AGENT_SET_DRV_[REMOVE,INSERT,LED]
 */
typedef opaque fcid_t<16>;
struct drive_op_t {
  u_int  ch_id;
  u_int  op;
  fcid_t fcid_bm;
};

%#ifndef _FCID_BM_
%#define _FCID_BM_
%typedef struct {
%  u_char fcid_bits[16];
%} fcid_bitmap_struct_t;
%typedef fcid_bitmap_struct_t *fcid_bitmap_t;
%#define NFCIDBITS 128
%#define FCID_SET(_bm, _n)   \
%   ((_bm)->fcid_bits[15-(_n)/8] |= (u_char)  (1 << ((_n) % 8)))
%#define FCID_CLR(_bm, _n)   \
%   ((_bm)->fcid_bits[15-(_n)/8] &= (u_char) ~(1 << ((_n) % 8)))
%#define FCID_ISSET(_bm, _n) \
%   ((_bm)->fcid_bits[15-(_n)/8] &  (u_char)  (1 << ((_n) % 8)))
%#define FCID_NEW() \
%   (calloc(1, sizeof(fcid_bitmap_struct_t)))
%#define FCID_FREE(_bm) \
%   (free(_bm))
%#define FCID_COPY(_bmsrc, _bmdst) \
%   (bcopy(_bmsrc, _bmdst, sizeof(fcid_bitmap_struct_t)))
%#endif

program AGENT_PROG {
  version AGENT_VERS {
    string       AGENT_GET_VERS(void) = 1;
    cfg_t        AGENT_GET_CONFIG(void) = 2;
    enc_stat_t   AGENT_GET_STATUS(enc_addr_t) = 3;
    int          AGENT_SET_DRV_REMOVE(drive_op_t) = 4;
    int          AGENT_SET_DRV_INSERT(drive_op_t) = 5;
    int          AGENT_SET_DRV_LED(drive_op_t) = 6;
    int          AGENT_SET_DRV_BYPASS(drive_op_t) = 7;
  } = 1;
} = 0x20455349;			/* 'E' 'S' 'I' */
