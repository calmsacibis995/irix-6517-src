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
  union Tile_* t = (union Tile_*)tile;

  if (t<tileMin || t>=tileMax || ((unsigned int)tile & 0xffff)!=0)
    panic("freeTile: invalid tile address");

  /* ECC checking off so that cache fill's don't cause ECC errors */
  *crime = 0;
  bzero(t,sizeof *t);
  
#if 0
  /* Do not enable ECC. Wait for firmware 2.0 */
  *crime |= CRM_MEM_CONTROL_ECC_ENA;
  /* Restore ECC checking. */
#endif

  t->next = tiles;
  tiles = t;
}
