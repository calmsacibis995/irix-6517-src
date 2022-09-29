
#define tcpfid_sport 1
#define tcpfid_dport 2
#define tcpfid_urp 9

struct tcp {
	ProtoStackFrame _psf;
	unsigned short sport;
	unsigned short dport;
	unsigned long seq;
	unsigned long ack;
	unsigned char off;
	unsigned char flags;
	unsigned short win;
	unsigned short sum;
	unsigned short urp;
}; /* tcp */
