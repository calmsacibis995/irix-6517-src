
#define ipfid_p 8

struct ip {
	ProtoStackFrame _psf;
	unsigned char v:4;
	unsigned char hl:4;
	unsigned char tos;
	unsigned short len;
	unsigned short id;
	unsigned short off;
	unsigned char ttl;
	unsigned char p;
	unsigned short sum;
	unsigned long src;
	unsigned long dst;
}; /* ip */
