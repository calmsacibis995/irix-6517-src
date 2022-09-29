#ifndef _AGENT_SERVICES_H_
#define _AGENT_SERVICES_H_

/* GET services */
#define GET_CONFIG      1	/* Get list of channels */
#define GET_STATUS      2	/* Get status of enclosures for a channel */
#define GET_EVENT       3	/* Get next event */

/* SET services */
#define SET_DRV_REMOVE 11	/* Remove drive(s) from loop */
#define SET_DRV_INSERT 12	/* Introduce drive(s) onto loop */
#define SET_LED        13	/* Turn LED(s) on/off */

typedef struct {
  u_char fcid_bits[16];
} fcid_bitmap_struct_t;
typedef fcid_bitmap_struct_t *fcid_bitmap_t;
#define NFCIDBITS 8
#define FCID_SET(_bm, _n)   \
   ((_bm)->bm_bits[15-(_n)/NFCIDBITS] |= (u_char)  (1 << ((_n) % NFCIDBITS)))
#define FCID_CLR(_bm, _n)   \
   ((_bm)->bm_bits[15-(_n)/NFCIDBITS] &= (u_char) ~(1 << ((_n) % NFCIDBITS)))
#define FCID_ISSET(_bm, _n) \
   ((_bm)->bm_bits[15-(_n)/NFCIDBITS] &  (u_char)  (1 << ((_n) % NFCIDBITS)))

#define FCID_NEW() \
   (calloc(1, sizeof(fcid_bitmap_struct_t)))
#define FCID_FREE(_bm) \
   (free(_bm))
#define FCID_COPY(_bmsrc, _bmdst) \
   (bcopy(_bmsrc, _bmdst, sizeof(fcid_bitmap_struct_t)))

#endif

