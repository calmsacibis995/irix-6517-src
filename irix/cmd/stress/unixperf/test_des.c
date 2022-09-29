
/* unixperf - an x11perf-style Unix performance benchmark */

/* test_des.c tests the speed of DES encryption and decryption. */

#include "unixperf.h"
#include "test_des.h"

typedef int keysched[32];
extern void fsetkey(unsigned char *key, keysched *ks);
extern void fencrypt(unsigned char *block, int decrypt, keysched *ks);

static unsigned char key[8] = { 0x12, 0x48, 0x62, 0x48, 0x62, 0x48, 0x62, 0x48 };
static keysched ks;

static unsigned char data[5][8] = {
  { 0xf0, 0xe1, 0xd2, 0xc3, 0xb4, 0xa5, 0x96, 0x87 },
  { 0x81, 0x28, 0x01, 0xda, 0xcb, 0xe9, 0x81, 0x03 },
  { 0xd8, 0x88, 0x3b, 0x2c, 0x4a, 0x7c, 0x61, 0xdd },
  { 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10 },
  { 0xec, 0xa8, 0x64, 0x20, 0x13, 0x57, 0x9b, 0xdf },
};

/* These are the resulting blocks from encoding the data above
   with the key above.  This is just used to verify that the
   encryption is correct on initialization. */
static unsigned char coded[5][8] = {
  { 0xf4, 0x68, 0x28, 0x65, 0x37, 0x6f, 0x93, 0xea },
  { 0x08, 0xbb, 0x81, 0x5f, 0x7f, 0x49, 0x69, 0xf6 },
  { 0x8c, 0x6b, 0x75, 0x4e, 0x09, 0x24, 0xb5, 0x22 },
  { 0x55, 0x12, 0xf0, 0x82, 0x0f, 0xae, 0xa8, 0xfa },
  { 0xcf, 0x10, 0x91, 0xa7, 0xd2, 0xb0, 0x3a, 0x6c },
};

Bool
InitDesCrypt(Version version)
{
  unsigned char tmp[8];
  int i, j;

  fsetkey(key, &ks);
  for (i=0; i<5; i++) {
    for(j=0; j<8; j++)
      tmp[j] = data[i][j];
    /* See if DES encryption gives us the expected coded value. */
    fencrypt(tmp, 0, &ks);
    for(j=0; j<8; j++) {
      if (tmp[j] != coded[i][j]) {
        printf("DES encryption algorithm failed to operate correctly.\n");
        return FALSE;
      }
    }
    /* Now try if decryption gets back to the original. */
    fencrypt(tmp, 1, &ks);
    for(j=0; j<8; j++) {
      if (tmp[j] != data[i][j]) {
        printf("DES encryption algorithm failed to operate correctly.\n");
        return FALSE;
      }
    }
  }
  return TRUE;
}

unsigned int
DoDesCrypt(void)
{
  int i;

  for(i=100;i>0;i--) {
    fencrypt(data[0], 0, &ks);
    fencrypt(data[1], 0, &ks);
    fencrypt(data[2], 0, &ks);
    fencrypt(data[3], 0, &ks);
    fencrypt(data[4], 0, &ks);

    fencrypt(data[0], 1, &ks);
    fencrypt(data[1], 1, &ks);
    fencrypt(data[2], 1, &ks);
    fencrypt(data[3], 1, &ks);
    fencrypt(data[4], 1, &ks);
  }
  return 500;
}
