#include "ch_internal.h"
#include "ch_sprites.h"
#include <rabbit/rb_image.h>

extern struct rb_image *ch_tilesheet;//TODO do this nicer

/* Score.
 *********************************************************/
 
#define SPRITE ((struct ch_sprite_score*)sprite)

static void _ch_score_del(struct rb_sprite *sprite) {
  rb_image_del(SPRITE->image);
}

static int _ch_score_init(struct rb_sprite *sprite) {
  SPRITE->ttl=30;
  return 0;
}

static uint32_t ch_blend_to_ck(uint32_t dst,uint32_t src,void *userdata) {
  if (!(src&0x00ffffff)) return 0;
  return src;
}

static int ch_score_draw_image(struct rb_sprite *sprite) {
  int digitc=1;
  int limit=10;
  while (SPRITE->score>=limit) { digitc++; if (limit>INT_MAX/10) break; limit*=10; }
  int colw=6,rowh=12;
  
  if (!(SPRITE->image=rb_image_new(colw*digitc,rowh))) return -1;
  SPRITE->image->alphamode=RB_ALPHAMODE_COLORKEY;
  
  struct rb_image *srcimage=ch_tilesheet;
  if (!srcimage) return 0;
  
  int srcy=5*colw;
  int x=colw*digitc;
  int i=digitc;
  int n=SPRITE->score;
  for (;i-->0;n/=10) {
    x-=colw;
    int digit=n%10;
    int srcx=(6+digit)*colw;
    rb_image_blit_unchecked(
      SPRITE->image,x,0,
      srcimage,srcx,srcy,
      colw,rowh,
      0,ch_blend_to_ck,0
    );
  }
  
  return 0;
}

static int _ch_score_render(struct rb_image *dst,struct rb_sprite *sprite,int x,int y) {
  if (!SPRITE->image) {
    if (ch_score_draw_image(sprite)<0) return -1;
  }
  return rb_image_blit_safe(
    dst,x,y,
    SPRITE->image,0,0,
    SPRITE->image->w,SPRITE->image->h,
    0,0,0
  );
}

static int _ch_score_update(struct rb_sprite *sprite) {
  if (--(SPRITE->ttl)<=0) return rb_sprite_kill(sprite);
  sprite->y--;
  return 0;
}

const struct rb_sprite_type ch_sprite_type_score={
  .name="score",
  .objlen=sizeof(struct ch_sprite_score),
  .del=_ch_score_del,
  .init=_ch_score_init,
  .render=_ch_score_render,
  .update=_ch_score_update,
};

#undef SPRITE
