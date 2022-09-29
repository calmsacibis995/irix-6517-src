/* Special font for underlines, and box characters.  Values are dependent
 * on font[0]'s dimensions.
 */
static unsigned short tpsdata[] = {
 								/* L_BRAK */
	0x07c0, 0x1fc0,	0x3800, 0x3000,	0x6000, 0x6000,	0x6000, 0x6000,
	0x6000, 0x6000,	0x6000, 0x6000,	0x6000, 0x6000,	0x6000, 0x6000,
	0x3000, 0x3800,	0x1fc0, 0x07c0,
 								/* [UO]_BAR */
 	0xffff, 0xffff, 
 								/* R_BRAK */
 	0xf000, 0xfc00,	0x0e00, 0x0600,	0x0300, 0x0300,	0x0300, 0x0300,
	0x0300, 0x0300,	0x0300, 0x0300,	0x0300, 0x0300,	0x0300, 0x0300, 
 	0x0600, 0x0e00,	0xfc00, 0xf000, 

	0x0000, 0x0000
};
static struct fontinfo tpspecial[] = {
	{ 42, FONTWIDTH,  2, 0, -4, 0 },	/* blank  */
	{ 00, FONTWIDTH, 20, 0, -4, FONTWIDTH },/* L_BRAK */
	{ 20, FONTWIDTH, 2,  0, -4, 0 },	/* U_BAR  */
	{ 42, FONTWIDTH,  2, 0, -4, 0 },	/* blank  */
	{ 42, FONTWIDTH, 18, 0, -4, 0 },	/* blank  */
	{ 20, FONTWIDTH, 2,  0, 14, 0 },	/* O_BAR  */
	{ 22, FONTWIDTH, 20, 0, -4, FONTWIDTH }	/* R_BRAK */
};

#define special_ht	20
#define special_ds	4
#define special_nc	(sizeof(tpspecial)/sizeof(struct fontinfo))
#define special_nr	(sizeof(tpsdata)/sizeof(short))
#define special_iso	0
