#include "ch_internal.h"
#include "ch_game.h"
#include <rabbit/rb_grid.h>

/* Fall the brick by one row.
 * Return >0 if fallen, <0 if stuck offscreen, or zero if it's at the ground and must bind.
 */
 
static int ch_game_fall_cb(int x,int y,void *userdata) {
  struct ch_game *game=userdata;
  if ((x<0)||(x>=game->towerw)) return 1; // how?
  if (y<0) return 0; // falling in from offscreen, let it roll
  if (y>=game->towerh) return 1; // struck bottom
  if (game->grid->v[(game->towery+y)*game->grid->w+game->towerx+x]) return 1; // stuck something hard
  return 0; // keep on falling
}
 
static int ch_game_fall(struct ch_game *game) {

  int i=game->fallskip;
  while (i-->0) {
    if (ch_game_for_brick_lower_neighbors(game,&game->brick,ch_game_fall_cb,game)) {
      if (ch_game_brick_bottom_row(&game->brick)<0) return -1;
      return 0;
    }
    ch_game_clear_brick_cells(game,&game->brick);
    game->brick.y++;
    ch_game_print_brick_cells(game,&game->brick);
  }
  
  return 1;
}

/* After fossilizing, check the grid for any completed lines (in rows y..y+3).
 * If found, this will enter "eliminate" state and update the score.
 */
 
static int ch_game_check_line(struct ch_game *game,int y) {
  const uint8_t *v=game->grid->v+(game->towery+y)*game->grid->w+game->towerx;
  int i=game->towerw;
  for (;i-->0;v++) if (!*v) return 0;
  return 1;
}
 
static int ch_game_check_lines(struct ch_game *game,int y) {
  game->eliminatec=0;
  int i=4; for (;i-->0;y++) {
    if (y<0) continue;
    if (y>=game->towerh) break;
    if (ch_game_check_line(game,y)) {
      game->eliminatev[game->eliminatec++]=y;
    }
  }
  if (!game->eliminatec) {
    ch_game_sound(game,CH_SFX_THUD);
    return 0;
  }
  
  if (game->eliminatec==4) {
    if (game->rhlopass>=900) {
      ch_game_sound(game,CH_SFX_CHETYORSKA);
    } else {
      ch_game_sound(game,CH_SFX_TETRIS);
    }
  } else {
    ch_game_sound(game,CH_SFX_LINES);
  }
  
  game->eliminatecounter=60;//TODO how long?
  
  int pvlevel=game->lines/10;
  game->lines+=game->eliminatec;
  int level=game->lines/10;
  if (game->rhlopass>=900) {
    game->score+=game->linescorev[game->eliminatec-1]*2;
  } else {
    game->score+=(game->linescorev[game->eliminatec-1]*game->rhlopass)/900;
  }
  
  if (level>pvlevel) {
    ch_game_advance_level(game);
  }
  
  ch_game_print_number(game,game->linesuip,game->linesuic,game->lines,0);
  ch_game_print_number(game,game->scoreuip,game->scoreuic,game->score,0);
  
  return 0;
}

/* Finalize elimination, after animating.
 * Lines and score were advanced at the initial detection.
 */
 
static int ch_game_finalize_elimination(struct ch_game *game) {
  const int *y=game->eliminatev;
  int i=game->eliminatec;
  for (;i-->0;y++) {
    int rowc=*y;
    uint8_t *p=game->grid->v+(game->towery+(*y))*game->grid->w+game->towerx;
    for (;rowc-->0;p-=game->grid->w) {
      memmove(p,p-game->grid->w,game->towerw);
    }
    memset(p,0,game->towerw);
  }
  game->eliminatecounter=0;
  game->eliminatec=0;
  return 0;
}

/* Flash the eliminating rows by toggling tiles between rows 0 and 1.
 */
 
static int ch_game_flash_eliminations(struct ch_game *game) {
  const int *y=game->eliminatev;
  int i=game->eliminatec;
  for (;i-->0;y++) {
    uint8_t *v=game->grid->v+(game->towery+(*y))*game->grid->w+game->towerx;
    int xi=game->towerw;
    for (;xi-->0;v++) (*v)^=0x10;
  }
  return 0;
}

/* Update, main entry point.
 */
 
int ch_game_update(struct ch_game *game) {
  game->input_blackout=0;
  if (!game->grid) return 0;
  
  if (game->eliminatecounter>0) {
    game->eliminatecounter--;
    if (!game->eliminatecounter) {
      ch_game_finalize_elimination(game);
    } else if (game->eliminatecounter%5==4) {//TODO timing
      return ch_game_flash_eliminations(game);
    } else {
      return 0;
    }
  }
  
  if (game->rhlopass>0) {
    game->rhlopass--;
    ch_game_print_rhythm_bar(game);
  }

  game->fallcounter--;
  if (game->fallcounter<=0) {
    if (game->dropping) {
      game->fallcounter=game->framesperfall_drop;
    } else {
      game->fallcounter=game->framesperfall;
    }
    if (game->nextbrickdelay>0) {
      game->nextbrickdelay--;
      if (!game->nextbrickdelay) ch_game_generate_next_brick(game);
    }
    int err=ch_game_fall(game);
    if (err<0) {
      return ch_game_end(game);
    }
    if (!err) {
      // This both generates a new one and binds the old (bricks are kind of "always bound")
      int cktop=game->brick.y;
      ch_game_new_brick(game);
      game->dropping=0;
      game->fallcounter=game->framesperfall;
      ch_game_check_lines(game,cktop);
    }
  }

  return 0;
}

/* Start of any event, consider timing against the background music.
 */
 
static int ch_game_rate_timing(struct ch_game *game) {
  if (!game->cb_get_phase) return 0;
  int p=0,c=0;
  if (game->cb_get_phase(&p,&c,game->phase_userdata)<0) return 0;
  if ((p<0)||(p>=c)) return 0;
  
  double norm=(double)p/(double)c;
  
  /* Hardly any of the songs I've looked at so far use 1/3 notes, so we'll skip those.
   * 1/4 is a bit uncommon. I think still good to acknowledge them.
   * But that is getting very close to the margin of error.
   * OK so like, there are 4 positions that are worth something, and we should allow a wider window at 0/1 and 1/2.
   * - First determine which interval is closest (0/4, 1/4, 2/4, 3/4), and get the absolute distance to it.
   */
  int nearest=(int)((norm+1.0/8.0)*4.0);
  if (nearest>=4) nearest=0;
  if (!nearest&&(norm>0.5)) norm-=1.0; // wrap into negative if 0 is nearest (approaching it from the high end)
  double nearestf=0.25*nearest;
  double distance=norm-nearestf;
  if (distance<0.0) distance=-distance;
  
  double windowsize;
  switch (nearest) {
    case 0: windowsize=1.0/8.0; break;
    case 1: windowsize=1.0/16.0; break;
    case 2: windowsize=1.0/12.0; break;
    case 3: windowsize=1.0/16.0; break;
    default: return 0;//oops
  }
  double quality=1.0-distance/windowsize;
  if (quality<0.0) quality=0.0;
  
  const char *judgment="";
       if (quality>=0.99) judgment="GREAT!";
  else if (quality>=0.80) judgment="GOOD";
  else if (quality>=0.20) judgment="Satisfactory";
  else                    judgment="BAAAAAAAAAAAD";
  
  if (quality>=0.85) {
    game->rhlopass+=100;
  } else if (quality>=0.50) {
    game->rhlopass+=50;
  } else if (quality<=0.01) {
    game->rhlopass-=10;
  }
  if (game->rhlopass<0) game->rhlopass=0;
  else if (game->rhlopass>999) game->rhlopass=999;
  
  ch_game_print_rhythm_bar(game);
  
  /**/
  fprintf(stderr,
    "STROKE AT %5d/%d norm=%+f nearest=%d distance=%f win=%f quality=%f %5d %s\n",
    p,c,norm,nearest,distance,windowsize,quality,game->rhlopass,judgment
  );
  /**/
  
  return 0;
}

/* Lateral movement.
 */
 
static int ch_game_move_cb(int x,int y,void *userdata) {
  struct ch_game *game=userdata;
  if (x<0) return 1;
  if (x>=game->towerw) return 1;
  if (y<0) return 0;
  if (y>=game->towerh) return 1;
  if (game->grid->v[(game->towery+y)*game->grid->w+game->towerx+x]) return 1;
  return 0;
}
 
static int ch_game_move_left(struct ch_game *game) {
  if (ch_game_rate_timing(game)<0) return -1;
  ch_game_sound(game,CH_SFX_MOVE);
  uint16_t edge=game->brick.shape&~((game->brick.shape&0xeeee)>>1);
  if (!ch_game_for_shape(game->brick.x-1,game->brick.y,edge,ch_game_move_cb,game)) {
    ch_game_clear_brick_cells(game,&game->brick);
    game->brick.x--;
    ch_game_print_brick_cells(game,&game->brick);
  }
  return 0;
}
 
static int ch_game_move_right(struct ch_game *game) {
  if (ch_game_rate_timing(game)<0) return -1;
  ch_game_sound(game,CH_SFX_MOVE);
  uint16_t edge=game->brick.shape&~((game->brick.shape&0x7777)<<1);
  if (!ch_game_for_shape(game->brick.x+1,game->brick.y,edge,ch_game_move_cb,game)) {
    ch_game_clear_brick_cells(game,&game->brick);
    game->brick.x++;
    ch_game_print_brick_cells(game,&game->brick);
  }
  return 0;
}

/* Rotation.
 */
 
static int ch_game_rotate(struct ch_game *game,int d) {
  if (ch_game_rate_timing(game)<0) return -1;
  ch_game_sound(game,CH_SFX_ROTATE);
  uint16_t nshape=ch_game_rotate_shape(game->brick.shape,d);
  if (nshape==game->brick.shape) return 0;
  ch_game_clear_brick_cells(game,&game->brick);
  if (!ch_game_for_shape(game->brick.x,game->brick.y,nshape,ch_game_move_cb,game)) {
    game->brick.shape=nshape;
  }
  ch_game_print_brick_cells(game,&game->brick);
  return 0;
}

/* Begin dropping.
 */
 
static int ch_game_drop(struct ch_game *game) {
  if (ch_game_rate_timing(game)<0) return -1;
  ch_game_sound(game,CH_SFX_DROP);
  game->fallcounter=0;
  game->dropping=1;
  return 0;
}

/* Receive input.
 */
 
int ch_game_input(struct ch_game *game,int eventid) {
  if (!game->grid) return 0;
  if (game->eliminatecounter) return 0;
  if (game->input_blackout&(1<<eventid)) return 0;
  game->input_blackout|=1<<eventid;
  switch (eventid) {
    case CH_EVENTID_LEFT: return ch_game_move_left(game);
    case CH_EVENTID_RIGHT: return ch_game_move_right(game);
    case CH_EVENTID_CLOCK: return ch_game_rotate(game,1);
    case CH_EVENTID_CCLOCK: return ch_game_rotate(game,-1);
    case CH_EVENTID_DROP: return ch_game_drop(game);
    case CH_EVENTID_PAUSE: break;//TODO
  }
  return 0;
}
