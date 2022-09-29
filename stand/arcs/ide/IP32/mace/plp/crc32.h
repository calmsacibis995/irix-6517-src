#ifndef __CRC32_H__
#define __CRC32_H__

void BuildCrcTable();
unsigned long Crc32(unsigned char *buf, int len);

#endif
