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
  int beatp,beatc; // song's beat, mapped to game cycles
  int srcbeatp; // absolute beat
  struct ch_song {
    int id;
    struct rb_song *song;
  } *songv;
  int songc,songa;
};

struct ch_app *ch_app_new(int argc,char **argv);
void ch_app_del(struct ch_app *app);

// Blocks according to video timing.
int ch_app_update(struct ch_app *app);

int ch_app_set_game(struct ch_app *app,struct ch_game *game);

int ch_app_play_song(struct ch_app *app,int songid);
#define CH_SONGID_SILENCE -1
#define CH_SONGID_RANDOM  -2

#endif
