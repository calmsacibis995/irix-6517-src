/* Prototypes for light diagnostics.
 */

#ident "$Revision: 1.1 $"

extern Rexchip *REX;

struct lg1_info;
void Lg1RegisterInit(struct rexchip *, struct lg1_info *);
int Lg1Probe(struct rexchip *hw);

void rex_setclt(void);
void rex_clear(void);
int rex_dualhead(void);
int rex_get4pixel(int,int);

void vc1_on(void);
void vc1_off(void);

int _planes(int);
int _color(int);
int _block(int, int, int, int);
int _rgbcolor(int, int, int);
int _line(int, int, int, int);
int _polygon(int, int, int, int, float color[][3]);
