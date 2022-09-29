/* prom color bitmap format */

struct pcbm_node {
	int color;			/* color index */
	int dx,dy;			/* offset from 0,0 */
	int w,h;			/* size */
	unsigned short *bits;		/* bits in pbm format */
};

struct pcbm {
	int w;
	int h;
	int dx;
	int dy;
	struct pcbm_node *bitlist;
};
