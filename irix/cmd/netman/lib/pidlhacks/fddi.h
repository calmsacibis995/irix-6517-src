
#define fddifid_control 3

struct fddi {
	ProtoStackFrame _psf;
	unsigned short pad;
	unsigned char status;
	unsigned char control;
	unsigned char dst[6];
	unsigned char src[6];
}; /* fddi */
