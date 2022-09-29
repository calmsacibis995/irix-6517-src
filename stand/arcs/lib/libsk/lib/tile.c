#if IP32 /* whole file */
/*
 * A very simple 64KB aligned 64KB tile manager
 */
#include <sys/cpu.h>
#include <libsc.h>
#include <tiles.h>

union Tile_{
  union Tile_* next;
  char         xxx[1024*64];
};

static union Tile_ *tiles, *tileMin, *tileMax;


/***********
 * initTiles
 */
void
initTiles(void){
  union Tile_* t = (union Tile_*)PROM_TILE_BASE;
  int           i;

  tileMin = tiles = t;

  for (i=0;i<PROM_TILE_CNT;t++,i++)
    t->next = t+1;
  tileMax = t--;
  t->next = 0;
}


/*********
 * getTile
 */
void* getTile(void){
  union Tile_* t = tiles;
  if (t==0)
    return 0;

  tiles = t->next;
  return t;
}


/**********
 * freeTile
 *
 * Note, some tile users leave them with bad ECC.  We zero the tile
 * when it is freed to eliminate badd ECC in the tile.
 */
void
freeTile(void* tile){
  long long* crime = (long long*)PHYS_TO_K1(CRM_MEM_CONTROL);
  long long tmp;
  union Tile_* t = (union Tile_*)tile;

  if (t<tileMin || t>=tileMax || ((unsigned int)tile & 0xffff)!=0)
    panic("freeTile: invalid tile address");

  /* ECC checking off so that cache fill's don't cause ECC errors */
  tmp = READ_REG64(PHYS_TO_K1(CRM_MEM_CONTROL), long long);
  tmp &= ~CRM_MEM_CONTROL_ECC_ENA;
  WRITE_REG64(tmp,PHYS_TO_K1(CRM_MEM_CONTROL), long long);
  bzero(t,sizeof *t);
/*************** wait for prom version 2.0 with ecc init
  tmp |= CRM_MEM_CONTROL_ECC_ENA;
  WRITE_REG64(tmp,PHYS_TO_K1(CRM_MEM_CONTROL), long long);
***********************************************************/
  /* Restore ECC checking. */

  t->next = tiles;
  tiles = t;
}
#endif /* IP32 */
