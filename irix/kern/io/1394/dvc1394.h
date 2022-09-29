#include <sys/poll.h>
#include <string.h>  /* To support sprintf for dvc1394open function */

typedef struct dvc1394_buf_s {
    long long frame[18000];     /* A dvc frame */
    char pad0[3456];            /* Pad to end of page */
    long long cycletimer;       /* The 1394 cycletimer of the first block */
    int valid;                  /* 1 if all the consistency checks passed */
    int ntsc;                   /* 1 if ntsc, zero otherwise */
    char pad1[4096-16];         /* Pad to end of page */
}dvc1394_buf_t;

/* Ioctl commands */

#define DVC1394_SET_CHANNEL		1
#define DVC1394_RELEASE_FRAME		2
#define DVC1394_GET_FRAME		3
#define DVC1394_GET_FILLED		4
#define DVC1394_GET_CHANNELGUIDE	5
#define DVC1394_GAIN_INTEREST           6
#define DVC1394_LOSE_INTEREST           7
#define DVC1394_SET_DEBUG_LEVEL		8
#define DVC1394_SET_DIRECTION		9
#define DVC1394_LOAD_FRAME		10
#define DVC1394_PUT_FRAME		11

typedef struct dvc1394_gain_lose_interest_s {
    long long Vendor_ID;
    long long Device_ID;
    long long begaddr;
    long long endaddr;
} dvc1394_gain_lose_interest_t;

typedef struct dvc1394_state_s {
    int            fd;
    dvc1394_buf_t *bufs;
    int            nbufs;
    int            mmapsize;
}dvc1394_state_t;

