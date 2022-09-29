
extern char *in_buf;
extern char *out_buf;
extern int  in_next;
extern int  out_next;
extern int  in_size;
extern int  out_size;
extern uint  in_checksum;
extern uint  out_checksum;

extern int compress(void);
extern int decompress(void);
extern void setup_compress(void);

#define CHECKSUM

typedef struct prom_lzw_hdr {
        __uint32_t      lzw_magic;
        __uint32_t      lzw_real_length;
        __uint32_t      lzw_real_cksum;
        __uint32_t      lzw_padding;
} prom_lzw_hdr_t;

#define LZW_MAGIC       0x5f4c5a57              /* '_LZW' */

