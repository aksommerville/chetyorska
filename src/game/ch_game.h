/* ch_game.h
 * Represents the entire logical model of a running game.
 * There could be more than one per program run, but only one at a time.
 */
 
#ifndef CH_GAME_H
#define CH_GAME_H

#include "ch_gridder.h"

// Must agree with the source image, and tiles must be square.
#define CH_TILESIZE 6

// Grid regions.
#define CH_RGN_TOWER_FRAME     1 /* 12x22 Where the game happens, including a 1-cell border. */
#define CH_RGN_TOWER           2 /* 10x20 Where the game happens. */
#define CH_RGN_NEXT            3 /* 6x6 Next brick, including border. */
#define CH_RGN_LINES           4 /* 8+x4 Line count -- 8 + digitcount */
#define CH_RGN_SCORE           5 /* 8+x4 Score '' */
#define CH_RGN_RHYTHM          6 /* 2+x1 Rhythm bar -- 2 + barwidth */
#define CH_RGN_METRONOME       7 /* 2x2 Lamp that pulses with the beat */

#define CH_TOWER_W 10
#define CH_TOWER_H 20

// The one moving piece. Always composed of four tiles.
struct ch_brick {
  int x,y; // top-left of shape's 4x4 box, in tower space
  uint16_t shape; // 16 positions LRTB big-endianly, top left at (x,y)
  uint8_t tileid;
};

struct ch_game {
  int refc;
  int input_blackout; // mask of (1<<CH_EVENTID_*), zeroed at update
  int finished;
  
  struct ch_gridder gridder;
  struct rb_sprite_group *sprites;
  
  struct ch_brick brick;
  struct ch_brick nextbrick;
  int framesperfall;
  int framesperfall_drop;
  int fallskip; // How many rows to drop on each fall -- only >1 at ludicrous speed
  int fallcounter;
  int dropping;
  int linescorev[4]; // how many points for each type of elimination
  int beatp,beatc; // Copied at update, ch_app gets them for real
  double tempo; // Tempo multiplier (lower is faster), picked up by app between updates.
  int newoverlapped;
  
  // Eliminating rows stops the action temporarily:
  int eliminatecounter;
  int eliminatev[4]; // rows eliminating
  int eliminatec;
  
  // Scorekeeping:
  int lines;
  int score;
  int rhlopass; // running state of rhythm quality, 0..999
  
  int (*cb_sound)(int sfxid,void *sound_userdata);
  void *sound_userdata;
};

struct ch_game *ch_game_new();
void ch_game_del(struct ch_game *game);
int ch_game_ref(struct ch_game *game);

/* Advance time by one frame.
 */
int ch_game_update(struct ch_game *game,int beatp,int beatc);

/* Deliver input events.
 * Inputs are stateless impulses, not buttons as usual.
 */
int ch_game_input(struct ch_game *game,int eventid);
#define CH_EVENTID_LEFT    0x01 /* Move left */
#define CH_EVENTID_RIGHT   0x02 /* Move right */
#define CH_EVENTID_CCLOCK  0x03 /* Rotate counter-clockwise */
#define CH_EVENTID_CLOCK   0x04 /* Rotate clockwise */
#define CH_EVENTID_DROP    0x05 /* Drop the piece */
#define CH_EVENTID_PAUSE   0x06 /* Pause or resume */

/* Create the initial grid and return a WEAK reference on success.
 */
struct rb_grid *ch_game_generate_grid(struct ch_game *game);
struct rb_sprite_group *ch_game_generate_sprites(struct ch_game *game);

uint16_t ch_game_random_brick_shape(uint8_t *tileid,struct ch_game *game);
uint16_t ch_game_rotate_shape(uint16_t shape,int d);
void ch_game_clear_brick_cells(struct ch_game *game,const struct ch_brick *brick);
void ch_game_print_brick_cells(struct ch_game *game,const struct ch_brick *brick);
void ch_game_new_brick(struct ch_game *game);

int ch_game_for_brick_lower_neighbors(
  struct ch_game *game,
  const struct ch_brick *brick,
  int (*cb)(int x,int y,void *userdata),
  void *userdata
);
int ch_game_for_shape(
  int x,int y,uint16_t shape,
  int (*cb)(int x,int y,void *userdata),
  void *userdata
);

int ch_game_brick_bottom_row(const struct ch_brick *brick);

void ch_game_print_rhythm_bar(struct ch_game *game);

int ch_game_end(struct ch_game *game);

int ch_game_sound(struct ch_game *game,int sndid);
#define CH_SFX_THUD        0x7f30
#define CH_SFX_LINES       0x7f31
#define CH_SFX_TETRIS      0x7f32
#define CH_SFX_CHETYORSKA  0x7f33
#define CH_SFX_GAMEOVER    0x7f34
#define CH_SFX_MOVE        0x7f35
#define CH_SFX_ROTATE      0x7f36
#define CH_SFX_DROP        0x7f37
#define CH_SFX_LEVELUP     0x7f38/*TODO*/

int ch_game_advance_level(struct ch_game *game);

void ch_game_redraw_next_brick(struct ch_game *game);
void ch_game_generate_next_brick(struct ch_game *game);

int ch_game_add_score_sprite(struct ch_game *game,int score,int x,int y);
int ch_game_start_fireworks(struct ch_game *game);

/* The first 7 shapes are the canonical ones, the way they should appear initially.
 */
#define CH_SHAPE_COUNT 19
#define CH_CANONICAL_SHAPE_COUNT 7
extern const struct ch_shape_metadata {
  uint16_t shape;
  uint16_t counter;
  uint16_t clock;
  uint8_t tileid;
} ch_shape_metadata[CH_SHAPE_COUNT];

/* Realistically, I believe only the first 8 are reachable by a human.
 * Maybe 10.
 * By the end of this list, the game is technically impossible.
 */
#define CH_LEVEL_COUNT 16
extern const struct ch_level_metadata {
  int framesperfall;
  int fallskip;
  double tempo;
} ch_level_metadata[CH_LEVEL_COUNT];

#endif
