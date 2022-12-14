/* prom color bitmap: sysstart */
#define sysstart_1_color	VeryLightGray	/* 213 213 213 */
unsigned short sysstart_1_bits[] = {
0x0200, 
0x0200, 
0x0000, 
0x0000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x8000, 
0x4000, 
0x3ff8, 
};
#define sysstart_2_color	BLACK2	/* 85 85 85 */
unsigned short sysstart_2_bits[] = {
0x0200, 
0x0200, 
0x7af8, 
0x800c, 
0x8004, 
0x8884, 
0x8884, 
0xb064, 
0x8004, 
0x8004, 
0x8004, 
0xa024, 
0x8004, 
0x8884, 
0x8004, 
0x8884, 
0x8f84, 
0xbae4, 
0x9544, 
0x9ac4, 
0x8f84, 
0xa724, 
0x8004, 
0x8884, 
0x8004, 
0x8884, 
0x8d84, 
0xbae4, 
0x9dc4, 
0x9fc4, 
0x8f84, 
0xa724, 
0x8004, 
0x8884, 
0x8004, 
0x7ff8, 
};
#define sysstart_3_color	BLACK	/* 0 0 0 */
unsigned short sysstart_3_bits[] = {
0x0200, 
0x0200, 
0x0200, 
0xffe0, 
0x0010, 
0x0e10, 
0x2090, 
0x0010, 
0x4050, 
0x4050, 
0x4050, 
0x2090, 
0x3190, 
0x0e10, 
0x0010, 
0x0e10, 
0x2090, 
0x0010, 
0x4050, 
0x4050, 
0x60d0, 
0x3190, 
0x3f90, 
0x0e10, 
0x0010, 
0x0e10, 
0x2090, 
0x0010, 
0x4050, 
0x4050, 
0x60d0, 
0x3190, 
0x3f90, 
0x0e10, 
0x0010, 
};
#define sysstart_4_color	WHITE2	/* 170 170 170 */
unsigned short sysstart_4_bits[] = {
0x8000, 
};
#define sysstart_5_color	CYAN2	/* 56 131 131 */
unsigned short sysstart_5_bits[] = {
0xffe0, 
0xe0e0, 
0xca60, 
0x9520, 
0xa0a0, 
0x8120, 
0xa0a0, 
0x9120, 
0xce60, 
0xe0e0, 
0xffe0, 
0xe0e0, 
0xc060, 
0x8020, 
0x8020, 
0x8020, 
0x8020, 
0x8020, 
0xc060, 
0xe0e0, 
0xffe0, 
0xe0e0, 
0xc060, 
0x8020, 
0x8020, 
0x8020, 
0x8020, 
0x8020, 
0xc060, 
0xe0e0, 
0xffe0, 
};
#define sysstart_6_color	GREEN	/* 0 255 0 */
unsigned short sysstart_6_bits[] = {
0x2000, 
0x5000, 
0xa800, 
0x9400, 
0x4800, 
0x3000, 
};
#define sysstart_7_color	WHITE	/* 255 255 255 */
unsigned short sysstart_7_bits[] = {
0x2800, 
0xb000, 
0x5800, 
0x2000, 
};
#define sysstart_8_color	YELLOW2	/* 142 142 56 */
unsigned short sysstart_8_bits[] = {
0x5000, 
0x8800, 
0x5000, 
};
#define sysstart_9_color	YELLOW	/* 255 255 0 */
unsigned short sysstart_9_bits[] = {
0x8000, 
};
#define sysstart_10_color	RED	/* 255 0 0 */
unsigned short sysstart_10_bits[] = {
0x4000, 
0xa000, 
0x4000, 
};

struct pcbm_node sysstart_nodes[] = {
	{sysstart_1_color,0,0,13,37,sysstart_1_bits},
	{sysstart_2_color,1,0,14,36,sysstart_2_bits},
	{sysstart_3_color,2,0,12,35,sysstart_3_bits},
	{sysstart_4_color,6,2,1,1,sysstart_4_bits},
	{sysstart_5_color,2,4,11,31,sysstart_5_bits},
	{sysstart_6_color,5,6,6,6,sysstart_6_bits},
	{sysstart_7_color,4,8,5,4,sysstart_7_bits},
	{sysstart_8_color,5,17,5,3,sysstart_8_bits},
	{sysstart_9_color,7,18,1,1,sysstart_9_bits},
	{sysstart_10_color,6,26,3,3,sysstart_10_bits},
	{0,0,0,0,0,0}
};
struct pcbm sysstart = {
	15,
	37,
	0,
	0,
	sysstart_nodes
};
