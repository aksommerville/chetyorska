#ifndef CH_APP_H
#define CH_APP_H

struct ch_app {
  struct rb_inmgr *inmgr;
  struct rb_audio *audio;
  struct rb_video *video;
  struct rb_vmgr *vmgr;
  struct rb_synth *synth;
  int quit; // nonzero if someone requested quit (eg close window)
  struct ch_game *game;
  int timing_bias;
};

struct ch_app *ch_app_new(int argc,char **argv);
void ch_app_del(struct ch_app *app);

// Blocks according to video timing.
int ch_app_update(struct ch_app *app);

int ch_app_set_game(struct ch_app *app,struct ch_game *game);

#endif
