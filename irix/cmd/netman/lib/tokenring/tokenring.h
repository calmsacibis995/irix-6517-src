
#define tokenringfid_ftype 5
#define tokenringfid_dst 8
#define tokenringfid_src 9

struct tokenring {
	ProtoStackFrame _psf;
	unsigned char prior:3;
	unsigned char tk:1;
	unsigned char mon:1;
	unsigned char resv:3;
	unsigned char ftype:2;
	unsigned char res:2;
	unsigned char ctrl:4;
	unsigned char dst[6];
	unsigned char src[6];
	unsigned char srb:3;
	unsigned char arb:3;
	unsigned char nb:3;
	unsigned char len:5;
	unsigned char dir:1;
	unsigned char lf:3;
	unsigned char resb:4;
	struct tokenringroutedes *rd;
}; /* tokenring */
