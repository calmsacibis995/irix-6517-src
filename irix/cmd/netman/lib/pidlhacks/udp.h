
#define udpfid_dport 2

struct udp {
	ProtoStackFrame _psf;
	unsigned short sport;
	unsigned short dport;
	unsigned short len;
	unsigned short sum;
}; /* udp */
