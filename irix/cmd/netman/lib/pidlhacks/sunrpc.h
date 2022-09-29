/* A pun for include/protocols/sunrpc.h's struct sunrpcframe */
struct sunrpc {
	ProtoStackFrame _psf;
	unsigned long prog;
	unsigned long vers;
	unsigned long proc;
	unsigned long direction;
	int procoff;
	unsigned long morefrags;
};
