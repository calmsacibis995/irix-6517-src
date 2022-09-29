 /*
 *	Block.h
 *
 *	Description:
 *		Class definition for Block
 *
 *	History:
 *      rogerc      04/11/91    Created
 */

#ifndef _Block_
#define _Block_

#include <sys/types.h>
#include <sys/time.h>
#include <rpc/rpc.h>
#include <netinet/in.h>
extern "C" {
#include "testcd_prot.h"
}

#define CDROM_BLKSIZE 2048
typedef char * strptr;

class Block {
public:
    Block(char *devscsi = "/dev/scsi/sc0d7l0");
    Block(unsigned long blk, char *devscsi = "/dev/scsi/sc0d7l0");
    Block(unsigned long blk, int num, char *devscsi = "/dev/scsi/sc0d7l0");
    ~Block() { delete buf; }
    int read(unsigned long blk, int num = 1);
    char &operator[] (int index) { return buf[index]; }
    char *operator+ (int num) { return buf + num; }
    operator char*() { return (buf); }
    operator void*() { return ((void *)buf); }
private:
    void init(char *devscsi);
    char *buf;
    int numBlocks;
    static CLIENT *client;
    static struct timeval timeout;
    static dev_t dev;
};

extern int blockSize;

#endif
