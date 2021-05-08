/* ch_sprites.h
 * I don't anticipate there being many.
 * "Next brick" are sprites but they use the dummy type.
 */
 
#ifndef CH_SPRITES_H
#define CH_SPRITES_H

#include <rabbit/rb_sprite.h>

extern const struct rb_sprite_type ch_sprite_type_score;

struct ch_sprite_score {
  struct rb_sprite hdr;
  int score;
  int ttl;
  struct rb_image *image;
};

extern const struct rb_sprite_type ch_sprite_type_fireworks;

struct ch_sprite_fireworks {
  struct rb_sprite hdr;
  int ttl;
  double x,y;
  double dx,dy;
};

#endif
