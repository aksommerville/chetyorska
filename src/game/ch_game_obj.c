#include "ch_internal.h"
#include "ch_game.h"
#include "ch_sprites.h"
#include <rabbit/rb_grid.h>
#include <rabbit/rb_image.h>
#include <rabbit/rb_sprite.h>

/* New.
 */
 
struct ch_game *ch_game_new() {
  struct ch_game *game=calloc(1,sizeof(struct ch_game));
  if (!game) return 0;
  
  game->refc=1;

  game->lines=0;
  ch_game_advance_level(game);
  game->linescorev[0]=100;
  game->linescorev[1]=300;
  game->linescorev[2]=500;
  game->linescorev[3]=800;
  
  return game;
}

/* Delete.
 */
 
void ch_game_del(struct ch_game *game) {
  if (!game) return;
  if (game->refc-->1) return;
  
  ch_gridder_cleanup(&game->gridder);
  rb_sprite_group_del(game->sprites);
  rb_sprite_group_del(game->nextsprites);
  
  free(game);
}

/* Retain.
 */
 
int ch_game_ref(struct ch_game *game) {
  if (!game) return -1;
  if (game->refc<1) return -1;
  if (game->refc==INT_MAX) return -1;
  game->refc++;
  return 0;
}

/* Generate grid.
 * This happens when a game gets attached to the app.
 */
 
struct rb_grid *ch_game_generate_grid(struct ch_game *game) {

  const int colc=RB_FB_W/CH_TILESIZE; // 42, and 4 columns leftover
  const int rowc=RB_FB_H/CH_TILESIZE; // 24
  struct rb_grid *grid=rb_grid_new(colc,rowc);
  if (!grid) return 0;
  grid->imageid=1;
  
  int err=ch_gridder_set_grid(&game->gridder,grid);
  rb_grid_del(grid);
  if (err<0) return 0;
  
  ch_gridder_fill(&game->gridder,0x31);
  
  struct ch_gridder_region *region;
  
  // Tower in the middle. Hold on to a copy of it; most of the rest of the UI builds around it.
  if (!(region=ch_gridder_new_region(&game->gridder,CH_RGN_TOWER_FRAME))) return 0;
  region->w=CH_TOWER_W+2;
  region->h=CH_TOWER_H+2;
  region->x=(colc>>1)-(region->w>>1);
  region->y=(rowc>>1)-(region->h>>1);
  struct ch_gridder_region tframe=*region;
  
  // And a separate region for just the tower's innards.
  if (!(region=ch_gridder_new_region(&game->gridder,CH_RGN_TOWER))) return 0;
  region->x=tframe.x+1;
  region->y=tframe.y+1;
  region->w=tframe.w-2;
  region->h=tframe.h-2;
  
  // Lines aligned with tower's top, flush against its right, extending to 1 column off the edge.
  if (!(region=ch_gridder_new_region(&game->gridder,CH_RGN_LINES))) return 0;
  region->x=tframe.x+tframe.w;
  region->y=tframe.y;
  region->w=colc-region->x-1;
  region->h=4;
  
  // Score immediately below lines, same idea.
  if (!(region=ch_gridder_new_region(&game->gridder,CH_RGN_SCORE))) return 0;
  region->x=tframe.x+tframe.w;
  region->y=tframe.y+4;
  region->w=colc-region->x-1;
  region->h=4;
  
  // Rhythm bar below score, with 1 row margin.
  if (!(region=ch_gridder_new_region(&game->gridder,CH_RGN_RHYTHM))) return 0;
  region->x=tframe.x+tframe.w;
  region->y=tframe.y+9;
  region->w=colc-region->x-1;
  region->h=1;
  
  // Next brick aligned with tower's top, flush against its left. Fixed size 6x6.
  if (!(region=ch_gridder_new_region(&game->gridder,CH_RGN_NEXT))) return 0;
  region->w=6;
  region->h=6;
  region->x=tframe.x-region->w;
  region->y=tframe.y;
  
  // Metronome below Next.
  if (!(region=ch_gridder_new_region(&game->gridder,CH_RGN_METRONOME))) return 0;
  region->w=2;
  region->h=2;
  region->x=tframe.x-region->w-1;
  region->y=tframe.y+7;
  
  if (ch_gridder_validate_all(&game->gridder)<0) return 0;
  
  // Now draw everything...
  #define FF(tag,frame,fill) ch_gridder_framefill_region( \
    &game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_##tag,0),frame,fill \
  );
  #define CONT(tag,tileid) ch_gridder_continuous_bar( \
    &game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_##tag,0),tileid,0,0 \
  );
  #define LABEL(tag,tileid) ch_gridder_text_label( \
    &game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_##tag,0),tileid,6 \
  );
  #define NUMBER(tag) ch_gridder_text_number( \
    &game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_##tag,0),6,0x56,0 \
  );
  #define BULK(tag,tileid) ch_gridder_bulk_region( \
    &game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_##tag,0),tileid \
  );
  
  FF(TOWER_FRAME,0x20,0x00)
  FF(LINES,0x20,0x00)
  LABEL(LINES,0x50)
  NUMBER(LINES)
  FF(SCORE,0x20,0x00)
  LABEL(SCORE,0x70)
  NUMBER(SCORE)
  CONT(RHYTHM,0x23)
  FF(NEXT,0x20,0x00)
  BULK(METRONOME,0x9e)
  
  #undef FF
  #undef CONT
  #undef LABEL
  #undef NUMBER
  #undef BULK
  
  ch_game_new_brick(game);
  if (!game->gridder.grid) return 0; // must never happen, but in theory new_brick could end the game
  
  return grid;
}

/* Generate sprite group.
 */
 
struct rb_sprite_group *ch_game_generate_sprites(struct ch_game *game) {
  if (!game->sprites) {
    if (!(game->sprites=rb_sprite_group_new(0))) return 0;
  }
  if (!game->nextsprites) {
    if (!(game->nextsprites=rb_sprite_group_new(0))) return 0;
    int i=4; while (i-->0) {
      struct rb_sprite *sprite=rb_sprite_new(&rb_sprite_type_dummy);
      if (!sprite) return 0;
      sprite->imageid=1;
      sprite->x=-100; // we can't make them transparent, so go offscreen when invisible
      int err=rb_sprite_group_add(game->sprites,sprite);
      rb_sprite_del(sprite);
      if (err<0) return 0;
      if (rb_sprite_group_add(game->nextsprites,sprite)<0) return 0;
    }
    ch_game_redraw_next_brick(game);
  }
  return game->sprites;
}

/* Select a random brick shape.
 * There are 19 valid shapes, of which 7 are canonical forms.
 * We will return only the canonical forms.
 */

uint16_t ch_game_random_brick_shape(uint8_t *tileid,struct ch_game *game) {
  int p=rand()%CH_CANONICAL_SHAPE_COUNT;
  if (p<0) p+=CH_CANONICAL_SHAPE_COUNT;
  *tileid=ch_shape_metadata[p].tileid;
  return ch_shape_metadata[p].shape;
}

/* Rotate a shape by 90 degrees.
 */
 
uint16_t ch_game_rotate_shape(uint16_t shape,int d) {
  if (!d) return shape;
  const struct ch_shape_metadata *meta=ch_shape_metadata;
  int i=CH_SHAPE_COUNT;
  for (;i-->0;meta++) {
    if (meta->shape==shape) {
      if (d<0) return meta->counter;
      else return meta->clock;
    }
  }
  return shape;
}

/* Replace cells occupied by a brick.
 */

void ch_game_clear_brick_cells(struct ch_game *game,const struct ch_brick *brick) {
  const struct ch_gridder_region *tower=ch_gridder_get_region(&game->gridder,CH_RGN_TOWER,0);
  if (!tower) return;
  ch_gridder_fill_shape(
    &game->gridder,
    tower->x+brick->x,
    tower->y+brick->y,
    brick->shape,
    0x00,
    tower
  );
}

void ch_game_print_brick_cells(struct ch_game *game,const struct ch_brick *brick) {
  const struct ch_gridder_region *tower=ch_gridder_get_region(&game->gridder,CH_RGN_TOWER,0);
  if (!tower) return;
  ch_gridder_fill_shape(
    &game->gridder,
    tower->x+brick->x,
    tower->y+brick->y,
    brick->shape,
    brick->tileid,
    tower
  );
}

/* Populate 4 sprites according to a shape.
 * Their positions are sane relative to each other, but don't assume anything about their absolute position.
 */
 
static void ch_game_populate_sprites_for_shape(
  struct rb_sprite **spritev,
  int spritec,
  uint16_t shape
) {
  int y=0;
  for (;y<4;y++) {
    int x=0;
    for (;x<4;x++,shape<<=1) {
      if (shape&0x8000) {
        if (spritec<1) return;
        (*spritev)->x=x*CH_TILESIZE;
        (*spritev)->y=y*CH_TILESIZE;
        spritev++;
        spritec--;
      }
    }
  }
}

/* Next brick UI.
 */
 
void ch_game_redraw_next_brick(struct ch_game *game) {
  if (!game->nextsprites||(game->nextsprites->c<4)) return;
  const struct ch_gridder_region *dst=ch_gridder_get_region(&game->gridder,CH_RGN_NEXT,0);
  if (!dst) return;
  
  ch_game_populate_sprites_for_shape(game->nextsprites->v,game->nextsprites->c,game->nextbrick.shape);
  int xlo=game->nextsprites->v[0]->x;
  int ylo=game->nextsprites->v[0]->y;
  int xhi=xlo,yhi=ylo;
  int i=4;
  while (i-->0) {
    struct rb_sprite *sprite=game->nextsprites->v[i];
    if (sprite->x<xlo) xlo=sprite->x;
    else if (sprite->x>xhi) xhi=sprite->x;
    if (sprite->y<ylo) ylo=sprite->y;
    else if (sprite->y>yhi) yhi=sprite->y;
    sprite->tileid=game->nextbrick.tileid;
  }
  int x=(xlo+xhi)>>1;
  int y=(ylo+yhi)>>1;
  
  int dstx=dst->x*CH_TILESIZE+((dst->w*CH_TILESIZE)>>1);
  int dsty=dst->y*CH_TILESIZE+((dst->h*CH_TILESIZE)>>1);
  int addx=dstx-x;
  int addy=dsty-y;
  
  struct rb_sprite **sprite=game->nextsprites->v;
  int spritec=game->nextsprites->c;
  int visitedc=0;
  while ((visitedc<4)&&(spritec>0)) {
    (*sprite)->x+=addx;
    (*sprite)->y+=addy;
    sprite++;
    spritec--;
    visitedc++;
  }
}

/* Generate a new brick at the top of the tower, fossilize the old one.
 */
 
static int ch_game_check_new_brick(int x,int y,void *userdata) {
  struct ch_game *game=userdata;
  if (ch_gridder_read(&game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_TOWER,0),x,y)) return 1;
  return 0;
}
 
void ch_game_new_brick(struct ch_game *game) {

  if (game->nextbrick.shape) {
    game->brick=game->nextbrick;
  } else {
    game->brick.shape=ch_game_random_brick_shape(&game->brick.tileid,game);
  }

  game->nextbrick.shape=ch_game_random_brick_shape(&game->nextbrick.tileid,game);
  ch_game_redraw_next_brick(game);

  game->brick.x=(CH_TOWER_W>>1)-2;
  // Align vertically to the top of the tower.
       if (game->brick.shape&0xf000) game->brick.y=-0;
  else if (game->brick.shape&0x0f00) game->brick.y=-1;
  else if (game->brick.shape&0x00f0) game->brick.y=-2;
  else game->brick.y=-3;
  
  if (ch_game_for_shape(game->brick.x,game->brick.y,game->brick.shape,ch_game_check_new_brick,game)) {
    game->newoverlapped=1;
  }
  ch_game_print_brick_cells(game,&game->brick);
}

void ch_game_generate_next_brick(struct ch_game *game) {
  game->nextbrick.shape=ch_game_random_brick_shape(&game->nextbrick.tileid,game);
  ch_game_redraw_next_brick(game);
}

/* Iterate the lower neighbors of a brick.
 */
 
int ch_game_for_brick_lower_neighbors(
  struct ch_game *game,
  const struct ch_brick *brick,
  int (*cb)(int x,int y,void *userdata),
  void *userdata
) {
  //TODO More efficient to enumerate the 19 valid shapes.
  int err;
  uint16_t mask=0x8000;
  int suby=0; for (;suby<4;suby++) {
    int subx=0; for (;subx<4;subx++,mask>>=1) {
      if (!(brick->shape&mask)) continue;
      if (brick->shape&(mask>>4)) continue; // brick is its own neighbor here; skip it
      if (err=cb(brick->x+subx,brick->y+suby+1,userdata)) return err;
    }
  }
  return 0;
}

/* Iterate all the tiles of a shape.
 * These are not necessarily valid shapes.
 */
 
int ch_game_for_shape(
  int x,int y,uint16_t shape,
  int (*cb)(int x,int y,void *userdata),
  void *userdata
) {
  int err;
  uint16_t rowmask=0xf000;
  int yi=4; for (;yi-->0;y++,rowmask>>=4) {
    if (!(shape&rowmask)) continue;
    uint16_t mask=0x8888&rowmask;
    int xi=4,xp=x; for (;xi-->0;xp++,mask>>=1) {
      if (!(shape&mask)) continue;
      if (err=cb(xp,y,userdata)) return err;
    }
  }
  return 0;
}

/* Return the row occupied by the lowest tile of brick.
 */
 
int ch_game_brick_bottom_row(const struct ch_brick *brick) {
  if (brick->shape&0x000f) return brick->y+3;
  if (brick->shape&0x00f0) return brick->y+2;
  if (brick->shape&0x0f00) return brick->y+1;
  return brick->y;
}

/* Print rhythm bar.
 */
 
void ch_game_print_rhythm_bar(struct ch_game *game) {
  const struct ch_gridder_region *rgn=ch_gridder_get_region(&game->gridder,CH_RGN_RHYTHM,0);
  if (!rgn) return;
  ch_gridder_continuous_bar(
    &game->gridder,rgn,
    0x23,game->rhlopass,900
  );
}

/* Finish the game.
 */

int ch_game_end(struct ch_game *game) {
  fprintf(stderr,"*********** game over, lines=%d, score=%d **************\n",game->lines,game->score);
  ch_game_sound(game,CH_SFX_GAMEOVER);
  game->finished=1;
  game->rhlopass=0;
  game->beatp=0;
  game->beatc=0;
  ch_game_print_rhythm_bar(game);
  ch_gridder_bulk_region(&game->gridder,ch_gridder_get_region(&game->gridder,CH_RGN_METRONOME,0),0x9e);
  return 0;
}

/* Play sound effect.
 */
 
int ch_game_sound(struct ch_game *game,int sfx) {
  if (!game->cb_sound) return 0;
  return game->cb_sound(sfx,game->sound_userdata);
}

/* Advance to the next level (every 10 lines).
 */
 
int ch_game_advance_level(struct ch_game *game) {

  int level=game->lines/10;
  if (level<0) level=0;
  else if (level>=CH_LEVEL_COUNT) level=CH_LEVEL_COUNT-1;
  //level=12;//XXX
  game->framesperfall=ch_level_metadata[level].framesperfall;
  game->fallskip=ch_level_metadata[level].fallskip;
  game->tempo=ch_level_metadata[level].tempo;
  game->framesperfall_drop=1;
  game->fallcounter=game->framesperfall;
  
  ch_game_sound(game,CH_SFX_LEVELUP);
  if (ch_game_start_fireworks(game)<0) return -1;
  
  return 0;
}

/* Add score sprite.
 */
 
int ch_game_add_score_sprite(struct ch_game *game,int score,int x,int y) {

  struct rb_sprite *sprite=rb_sprite_new(&ch_sprite_type_score);
  if (!sprite) return -1;
  int err=rb_sprite_group_add(game->sprites,sprite);
  rb_sprite_del(sprite);
  if (err<0) return -1;
  
  const struct ch_gridder_region *region=ch_gridder_get_region(&game->gridder,CH_RGN_TOWER,0);
  if (!region) return -1;
  
  sprite->x=region->x*CH_TILESIZE+x;
  sprite->y=region->y*CH_TILESIZE+y;
  ((struct ch_sprite_score*)sprite)->score=score;

  return 0;
}

/* Add a bunch of fireworks sprites.
 */
 
int ch_game_start_fireworks(struct ch_game *game) {
  const int count=100;
  int i=count; while (i-->0) {
    struct rb_sprite *sprite=rb_sprite_new(&ch_sprite_type_fireworks);
    if (!sprite) return -1;
    int err=rb_sprite_group_add(game->sprites,sprite);
    rb_sprite_del(sprite);
    if (err<0) return -1;
  }
  return 0;
}
