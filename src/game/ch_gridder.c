#include "ch_internal.h"
#include "ch_gridder.h"
#include "ch_game.h"
#include <rabbit/rb_grid.h>

/* Cleanup.
 */
 
void ch_gridder_cleanup(struct ch_gridder *gridder) {
  if (!gridder) return;
  rb_grid_del(gridder->grid);
  if (gridder->regionv) free(gridder->regionv);
  memset(gridder,0,sizeof(struct ch_gridder));
}

/* Set grid.
 */
 
int ch_gridder_set_grid(struct ch_gridder *gridder,struct rb_grid *grid) {
  if (gridder->grid==grid) return 0;
  if (grid&&(rb_grid_ref(grid)<0)) return -1;
  rb_grid_del(gridder->grid);
  gridder->grid=grid;
  ch_gridder_sanitize_all(gridder);
  return 0;
}

/* Read cell.
 */
 
uint8_t ch_gridder_read(
  const struct ch_gridder *gridder,
  const struct ch_gridder_region *region,
  int x,int y
) {
  if (!gridder||!gridder->grid) return 0xff;
  if ((x<0)||(y<0)) return 0xff;
  if (region) {
    if (x>=region->w) return 0xff;
    if (y>=region->h) return 0xff;
    x+=region->x;
    y+=region->y;
  } else {
    if (x>=gridder->grid->w) return 0xff;
    if (y>=gridder->grid->h) return 0xff;
  }
  return gridder->grid->v[y*gridder->grid->w+x];
}

/* New region.
 */

struct ch_gridder_region *ch_gridder_new_region(struct ch_gridder *gridder,int id) {
  struct ch_gridder_region *region=gridder->regionv;
  int i=gridder->regionc;
  for (;i-->0;region++) {
    if (region->id==id) return 0;
  }
  if (gridder->regionc>=gridder->regiona) {
    int na=gridder->regiona+8;
    if (na>INT_MAX/sizeof(struct ch_gridder_region)) return 0;
    void *nv=realloc(gridder->regionv,sizeof(struct ch_gridder_region)*na);
    if (!nv) return 0;
    gridder->regionv=nv;
    gridder->regiona=na;
  }
  region=gridder->regionv+gridder->regionc++;
  memset(region,0,sizeof(struct ch_gridder_region));
  region->id=id;
  return region;
}

/* Get region.
 */
 
struct ch_gridder_region *ch_gridder_get_region(struct ch_gridder *gridder,int id,int create) {
  struct ch_gridder_region *region=gridder->regionv;
  int i=gridder->regionc;
  for (;i-->0;region++) {
    if (region->id==id) return region;
  }
  if (create) return ch_gridder_new_region(gridder,id);
  return 0;
}

/* Delete region.
 */
 
void ch_gridder_delete_region(struct ch_gridder *gridder,int id) {
  int i=gridder->regionc;
  struct ch_gridder_region *region=gridder->regionv+i-1;
  for (;i-->0;region--) {
    if (region->id==id) {
      gridder->regionc--;
      memmove(region,region+1,sizeof(struct ch_gridder_region)*(gridder->regionc-i));
      return;
    }
  }
}

void ch_gridder_delete_all(struct ch_gridder *gridder) {
  gridder->regionc=0;
}

/* Validate region.
 */
 
int ch_gridder_validate_region(const struct ch_gridder *gridder,const struct ch_gridder_region *region) {
  if (!gridder||!region) return -1;
  if (region->x<0) return -1;
  if (region->y<0) return -1;
  if (region->w<0) return -1;
  if (region->h<0) return -1;
  if (gridder->grid) {
    if (region->x+region->w>gridder->grid->w) return -1;
    if (region->y+region->h>gridder->grid->h) return -1;
  } else {
    if (region->x+region->w) return -1;
    if (region->y+region->h) return -1;
  }
  return 0;
}

void ch_gridder_sanitize_region(const struct ch_gridder *gridder,struct ch_gridder_region *region) {
  if (!gridder||!region) return;
  if (gridder->grid) {
    if (region->x<0) { region->w+=region->x; region->x=0; }
    if (region->y<0) { region->h+=region->y; region->y=0; }
    if (region->x>=gridder->grid->w) { region->x=gridder->grid->w; region->w=0; }
    else if (region->x+region->w>gridder->grid->w) region->w=gridder->grid->w-region->x;
    else if (region->w<0) region->w=0;
    if (region->y>=gridder->grid->h) { region->y=gridder->grid->h; region->h=0; }
    else if (region->y+region->h>gridder->grid->h) region->h=gridder->grid->h-region->y;
    else if (region->h<0) region->h=0;
  } else {
    region->x=0;
    region->y=0;
    region->w=0;
    region->h=0;
  }
}

int ch_gridder_validate_all(const struct ch_gridder *gridder) {
  const struct ch_gridder_region *region=gridder->regionv;
  int i=gridder->regionc;
  for (;i-->0;region++) {
    if (ch_gridder_validate_region(gridder,region)<0) return -1;
  }
  return 0;
}

void ch_gridder_sanitize_all(struct ch_gridder *gridder) {
  struct ch_gridder_region *region=gridder->regionv;
  int i=gridder->regionc;
  for (;i-->0;region++) ch_gridder_sanitize_region(gridder,region);
}

/* Fill all.
 */
 
void ch_gridder_fill(struct ch_gridder *gridder,uint8_t tileid) {
  if (!gridder->grid) return;
  memset(gridder->grid->v,tileid,gridder->grid->w*gridder->grid->h);
}

/* Fill region.
 */

void ch_gridder_fill_region(struct ch_gridder *gridder,const struct ch_gridder_region *region,uint8_t tileid) {
  if (!region||!region->w||!region->h) return;
  if (!gridder->grid) return;
  uint8_t *row=gridder->grid->v+region->y*gridder->grid->w+region->x;
  int i=region->h;
  for (;i-->0;row+=gridder->grid->w) {
    memset(row,tileid,region->w);
  }
}

void ch_gridder_frame_region(struct ch_gridder *gridder,const struct ch_gridder_region *region,uint8_t tileid) {
  if (!region||!region->w||!region->h) return;
  if (!gridder->grid) return;
  uint8_t *row=gridder->grid->v+region->y*gridder->grid->w+region->x;
  
  row[0]=tileid+0x00;
  if (region->w>=2) {
    row[region->w-1]=tileid+0x02;
    memset(row+1,tileid+0x01,region->w-2);
  }
  
  if (region->h>=2) {
    uint8_t *bottom=row+(region->h-1)*gridder->grid->w;
    bottom[0]=tileid+0x20;
    if (region->w>=2) {
      bottom[region->w-1]=tileid+0x22;
      memset(bottom+1,tileid+0x21,region->w-2);
    }
    
    int i=region->h-1;
    int fillw=region->w-2;
    if (fillw<0) fillw=0;
    while (i-->1) { // (h-2) inner rows
      row+=gridder->grid->w;
      row[region->w-1]=tileid+0x12; // possibly redundant, whatever
      row[0]=tileid+0x10;
    }
  }
}

void ch_gridder_framefill_region(struct ch_gridder *gridder,const struct ch_gridder_region *region,uint8_t frame,uint8_t fill) {
  if (!region||!region->w||!region->h) return;
  if (!gridder->grid) return;
  uint8_t *row=gridder->grid->v+region->y*gridder->grid->w+region->x;
  
  row[0]=frame+0x00;
  if (region->w>=2) {
    row[region->w-1]=frame+0x02;
    memset(row+1,frame+0x01,region->w-2);
  }
  
  if (region->h>=2) {
    uint8_t *bottom=row+(region->h-1)*gridder->grid->w;
    bottom[0]=frame+0x20;
    if (region->w>=2) {
      bottom[region->w-1]=frame+0x22;
      memset(bottom+1,frame+0x21,region->w-2);
    }
    
    int i=region->h-1;
    int fillw=region->w-2;
    if (fillw<0) fillw=0;
    while (i-->1) { // (h-2) inner rows
      row+=gridder->grid->w;
      row[region->w-1]=frame+0x12; // possibly redundant, whatever
      row[0]=frame+0x10;
      memset(row+1,fill,fillw);
    }
  }
}

/* Draw continuous-value progress bar.
 */
 
void ch_gridder_continuous_bar(
  struct ch_gridder *gridder,
  const struct ch_gridder_region *region,
  uint8_t tileid,
  int v,int c
) {
  if (!region||!region->w||!region->h) return;
  if (!gridder->grid) return;
  uint8_t *row=gridder->grid->v+region->y*gridder->grid->w+region->x;
  
  row[0]=tileid;
  if (region->w<2) return;
  row[region->w-1]=tileid+2;
  if (region->w<3) return;
  
  // 0.0, 1.0, and OVERFLOW are easy, pick them off.
  if (v<=0) {
    memset(row+1,tileid+1,region->w-2);
  } else if (v>c) {
    memset(row+1,tileid+9,region->w-2);
  } else if (v==c) {
    memset(row+1,tileid+8,region->w-2);
  
  // The truly continuous case.
  } else {
    int dstcolc=region->w-2;
    int dstpixelc=dstcolc*CH_TILESIZE;
    int setpixelc=(v*dstpixelc)/c;
    row++;
    for (;dstcolc-->0;row++,setpixelc-=CH_TILESIZE) {
      if (setpixelc<=0) *row=tileid+1;
      else if (setpixelc>=CH_TILESIZE) *row=tileid+2+CH_TILESIZE;
      else *row=tileid+2+setpixelc;
    }
  }
}

/* Text label.
 */
 
void ch_gridder_text_label(
  struct ch_gridder *gridder,
  const struct ch_gridder_region *region,
  uint8_t tileid,
  int w
) {
  if (!region||(w<1)||(region->w<2+w)||(region->h<3)) return;
  if (!gridder->grid) return;
  uint8_t *dsthi=gridder->grid->v+(region->y+1)*gridder->grid->w+region->x+1;
  uint8_t *dstlo=dsthi+gridder->grid->w;
  uint8_t thi=tileid;
  uint8_t tlo=tileid+0x10;
  for (;w-->0;dsthi++,dstlo++,thi++,tlo++) {
    *dsthi=thi;
    *dstlo=tlo;
  }
}

/* Positive decimal integer.
 */
 
void ch_gridder_text_number(
  struct ch_gridder *gridder,
  const struct ch_gridder_region *region,
  int labelw,
  uint8_t tileid,
  int v
) {
  if (!region||(region->h<3)) return;
  int w=region->w-2-labelw;
  if ((w<1)||(w>region->w-2)) return;
  if (!gridder->grid) return;
  
  uint8_t *dst=gridder->grid->v+region->y*gridder->grid->w+region->x;
  dst+=gridder->grid->w; // expect a top border
  dst+=1+labelw; // skip label and left border
  
  // Clamp (v) to zero and the largest expressible integer here.
  if (v<=0) v=0;
  else if (w>=10) {
    if (v>999999999) v=999999999; // above 1G-1, we can't saturate with nines
  } else {
    int limit=10;
    int i=w; while (i-->1) limit*=10;
    limit--;
    if (v>limit) v=limit;
  }
  
  int x=w;
  for (;x-->0;v/=10) {
  
    // Left-fill with space instead of zero
    if (!v&&(x<w-1)) {
      dst[x]=0x00;
      dst[gridder->grid->w+x]=0x00;
      
    } else {
      int digit=v%10;
      dst[x]=tileid+digit;
      dst[gridder->grid->w+x]=tileid+0x10+digit;
    }
  }
}

/* Draw shape.
 */

void ch_gridder_fill_shape(
  struct ch_gridder *gridder,
  int x,int y,
  uint16_t shape,
  uint8_t tileid,
  const struct ch_gridder_region *scissor
) {
  if (!gridder->grid) return;
  
  // To apply the scissor, it's easier to modify (shape) in advance than to consult during iteration.
  struct ch_gridder_region _fakescissor;
  if (!scissor) {
    scissor=&_fakescissor;
    _fakescissor.x=0;
    _fakescissor.y=0;
    _fakescissor.w=gridder->grid->w;
    _fakescissor.h=gridder->grid->h;
  }
  int trim;
  if ((trim=scissor->x-x)>0) {
    if (trim>=4) return;
    uint16_t rmbits=0x8888;
    int i=trim-1; while (i-->0) rmbits|=rmbits>>1;
    shape&=~rmbits;
  }
  if ((trim=x+4-scissor->w-scissor->x)>0) {
    if (trim>=4) return;
    uint16_t rmbits=0x1111;
    int i=trim-1; while (i-->0) rmbits|=rmbits<<1;
    shape&=~rmbits;
  }
  if ((trim=scissor->y-y)>0) {
    if (trim>=4) return;
    uint16_t rmbits=0xf000;
    int i=trim-1; while (i-->0) rmbits|=rmbits>>4;
    shape&=~rmbits;
  }
  if ((trim=y+4-scissor->h-scissor->y)>0) {
    if (trim>=4) return;
    uint16_t rmbits=0x000f;
    int i=trim-1; while (i-->0) rmbits|=rmbits<<4;
    shape&=~rmbits;
  }
  if (!shape) return;
  
  // Due to scissor, it is OK to iterate OOB here; we know (shape) won't try to write OOB.
  uint8_t *row=gridder->grid->v+y*gridder->grid->w+x;
  int yi=4;
  for (;yi-->0;row+=gridder->grid->w,shape<<=4) {
    uint8_t *p=row;
    uint16_t mask=0x8000;
    int xi=4;
    for (;xi-->0;p++,mask>>=1) {
      if (shape&mask) *p=tileid;
    }
  }
}
