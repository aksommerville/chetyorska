#include "ch_internal.h"
#include "ch_app.h"
#include "game/ch_game.h"
#include "opt/ossmidi/ch_ossmidi.h"
#include <rabbit/rb_video.h>
#include <rabbit/rb_audio.h>
#include <rabbit/rb_inmgr.h>
#include <rabbit/rb_inmap.h>
#include <rabbit/rb_vmgr.h>
#include <rabbit/rb_synth.h>
#include <rabbit/rb_synth_event.h>
#include <rabbit/rb_grid.h>
#include <rabbit/rb_fs.h>
#include <rabbit/rb_archive.h>

struct rb_image *ch_tilesheet=0;//XXX organize

void ch_app_set_video_callbacks(struct rb_video_delegate *delegate);
void ch_app_set_inmgr_callbacks(struct rb_inmgr_delegate *delegate);
int ch_app_cb_midi(void *userdata,int devid,const void *src,int srcc);

/* Argv.
 */
 
static int ch_argc=0;
static char **ch_argv=0;

static const char *ch_arg_string(const char *k,const char *fallback) {
  int kc=0; while (k[kc]) kc++;
  int i=1; for (;i<ch_argc;i++) {
    if (memcmp(ch_argv[i],k,kc)) continue;
    if (ch_argv[i][kc]!='=') continue;
    return ch_argv[i]+kc+1;
  }
  return fallback;
}

static int ch_arg_int(const char *k,int fallback) {
  const char *src=ch_arg_string(k,0);
  if (!src) return fallback;
  int v=0;
  for (;*src;src++) {
    if ((*src<'0')||(*src>='9')) return fallback;
    v*=10;
    v+=(*src)-'0';
  }
  return v;
}

/* Audio PCM callback.
 */
 
static int ch_app_cb_pcm_out(int16_t *v,int c,struct rb_audio *audio) {
  struct ch_app *app=audio->delegate.userdata;
  if (app->synth) {
    if (rb_synth_update(v,c,app->synth)<0) return -1;
  } else {
    memset(v,0,c<<1);
  }
  return 0;
}

/* Init audio.
 */
 
#ifndef CH_SYNTH_CACHE
  #define CH_SYNTH_CACHE 0
#endif
 
static int ch_app_init_audio(struct ch_app *app) {
  struct rb_audio_delegate delegate={
    .userdata=app,
    .rate=ch_arg_int("--audio-rate",44100),
    .chanc=ch_arg_int("--audio-chanc",2),
    .device=ch_arg_string("--audio-device",0),
    .cb_pcm_out=ch_app_cb_pcm_out,
  };
  if (!(app->audio=rb_audio_new(rb_audio_type_by_index(0),&delegate))) return -1;
  if (!(app->synth=rb_synth_new(app->audio->delegate.rate,app->audio->delegate.chanc))) return -1;
  app->synth->cachedir=CH_SYNTH_CACHE;
  return 0;
}

/* Init video.
 */
 
static int ch_app_init_video(struct ch_app *app) {
  //TODO configure (persist?) window size, fullscreen, drivertype
  struct rb_video_delegate delegate={
    .userdata=app,
    .winw=1200,
    .winh=675,
    .fullscreen=0,
    .title="Chetyorska",
    .device=ch_arg_string("--video-device",0),
  };
  ch_app_set_video_callbacks(&delegate);
  if (!(app->video=rb_video_new(0,&delegate))) return -1;
  if (!(app->vmgr=rb_vmgr_new())) return -1;
  return 0;
}

/* Initialize input.
 */
 
static int ch_app_init_input(struct ch_app *app) {

  struct rb_inmgr_delegate delegate={
    .userdata=app,
  };
  ch_app_set_inmgr_callbacks(&delegate);
  if (!(app->inmgr=rb_inmgr_new(&delegate))) return -1;
  if (rb_inmgr_use_system_keyboard(app->inmgr)<0) return -1;
  if (rb_inmgr_connect_all(app->inmgr)<0) return -1;
  
  const char *incfg_path=ch_arg_string("--input-config",0);
  if (incfg_path) {
    void *serial=0;
    int serialc=rb_file_read(&serial,incfg_path);
    if (serialc>=0) {
      if (rb_inmap_store_decode(app->inmgr->store,serial,serialc)<0) {
        fprintf(stderr,"%s: Error applying input cofnig.\n",incfg_path);
      } else {
        fprintf(stderr,"%s: Applied input config.\n",incfg_path);
      }
      free(serial);
    } else {
      fprintf(stderr,"%s: Read failed. Input mapping unconfigured.\n",incfg_path);
    }
  }
  
  if (ch_ossmidi_init(ch_app_cb_midi,app)<0) return -1;
  
  return 0;
}

/* Song list primitives.
 */
 
static int ch_app_songv_search(const struct ch_app *app,int id) {
  int lo=0,hi=app->songc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (id<app->songv[ck].id) hi=ck;
    else if (id>app->songv[ck].id) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct ch_song *ch_app_songv_insert(struct ch_app *app,int p,int id) {
  if ((p<0)||(p>app->songc)) return 0;
  if (p&&(id<=app->songv[p-1].id)) return 0;
  if ((p<app->songc)&&(id>=app->songv[p].id)) return 0;
  
  if (app->songc>=app->songa) {
    int na=app->songa+8;
    if (na>INT_MAX/sizeof(struct ch_song)) return 0;
    void *nv=realloc(app->songv,sizeof(struct ch_song)*na);
    if (!nv) return 0;
    app->songv=nv;
    app->songa=na;
  }
  
  struct ch_song *song=app->songv+p;
  memmove(song+1,song,sizeof(struct ch_song)*(app->songc-p));
  app->songc++;
  memset(song,0,sizeof(struct ch_song));
  song->id=id;
  return song;
}

/* Load a song -- Rabbit doesn't store these, we do it.
 */
 
static int ch_app_load_song(struct ch_app *app,int id,const void *src,int srcc) {

  struct rb_song *song=rb_song_new(src,srcc);
  if (!song) return -1;

  int p=ch_app_songv_search(app,id);
  struct ch_song *wrapper;
  if (p>=0) {
    wrapper=app->songv+p;
  } else {
    if (!(wrapper=ch_app_songv_insert(app,-p-1,id))) {
      rb_song_del(song);
      return -1;
    }
  }
  
  rb_song_del(wrapper->song);
  wrapper->song=song;
  
  return 0;
}

/* Load resources.
 */
 
static int ch_app_archive_cb(uint32_t type,int id,const void *src,int srcc,void *userdata) {
  struct ch_app *app=userdata;
  switch (type) {
    case RB_RES_TYPE_imag: return rb_vmgr_set_image_serial(app->vmgr,id,src,srcc);
    case RB_RES_TYPE_song: return ch_app_load_song(app,id,src,srcc);
    case RB_RES_TYPE_snth: return rb_synth_load_program(app->synth,id,src,srcc);
  }
  return 0;
}
 
static int ch_app_load_archive(struct ch_app *app,const char *path) {

  // If we cared about underrunning the PCM out, we could lock on individual snth resources instead of the whole archive.
  // We don't care, because playback hasn't started yet.
  if (rb_audio_lock(app->audio)<0) return -1;
  int err=rb_archive_read(path,ch_app_archive_cb,app);
  rb_audio_unlock(app->audio);
  if (err<0) return -1;

  if (!(ch_tilesheet=app->vmgr->imagev[1])) {
    fprintf(stderr,"%s: Data archive did not contain a tilesheet.\n",path);
    return -1;
  }
  
  if (app->songc>0) app->seqsongp=rand()%app->songc;
  
  return 0;
}

/* New.
 */
 
struct ch_app *ch_app_new(int argc,char **argv) {
  ch_argc=argc;
  ch_argv=argv;
  struct ch_app *app=calloc(1,sizeof(struct ch_app));
  if (!app) return 0;
  
  app->cb_event=ch_app_cb_event_null;
  
  // Get sound effects running so we can judge this better
  //app->timing_bias=-2000;//TODO Figure out an appropriate value, accounting for latency in MIDI and PCM drivers.
  
  if (ch_app_init_audio(app)<0) {
    fprintf(stderr,"Failed to initialize audio.\n");
    ch_app_del(app);
    return 0;
  }
  
  if (ch_app_init_video(app)<0) {
    fprintf(stderr,"Failed to initialize video.\n");
    ch_app_del(app);
    return 0;
  }
  
  if (ch_app_init_input(app)<0) {
    fprintf(stderr,"Failed to initialize input.\n");
    ch_app_del(app);
    return 0;
  }
  
  const char *datapath="out/data";
  int i=1; for (;i<argc;i++) {
    if (memcmp(argv[i],"--data=",7)) continue;
    datapath=argv[i]+7;
    break;
  }
  if (ch_app_load_archive(app,datapath)<0) {
    fprintf(stderr,"%s: Failed to load data.\n",datapath);
    ch_app_del(app);
    return 0;
  }
  
  return app;
}

/* Delete.
 */
 
static void ch_song_cleanup(struct ch_song *song) {
  rb_song_del(song->song);
}
 
void ch_app_del(struct ch_app *app) {
  if (!app) return;
  
  rb_audio_del(app->audio);
  rb_video_del(app->video);
  rb_inmgr_del(app->inmgr);
  rb_vmgr_del(app->vmgr);
  rb_synth_del(app->synth);
  ch_ossmidi_quit();
  ch_game_del(app->game);
  
  if (app->songv) {
    while (app->songc-->0) ch_song_cleanup(app->songv+app->songc);
    free(app->songv);
  }
  
  free(app);
}

/* Update.
 */

int ch_app_update(struct ch_app *app) {

  // One normally should lock audio when reading the synth context.
  // I'm making an exception because we know that song changes can only be effected from this thread.
  // This exception would be invalid if there's a chance we play songs without repeat, beware!
  int songp,songc;
  if ((rb_synth_get_song_phase(&songp,&songc,app->synth)>0)&&(songc>0)) {
    int srcbeatp=songp/songc;
    if (srcbeatp>app->srcbeatp) {
      app->srcbeatp=srcbeatp;
      app->beatc=app->beatp;
      app->beatp=0;
    } else if (srcbeatp<app->srcbeatp) { // reset
      app->srcbeatp=srcbeatp;
      app->beatc=app->beatp;
      app->beatp=0;
    } else {
      app->beatp++;
    }
  } else {
    app->beatp=0;
    app->beatc=0;
    app->srcbeatp=0;
  }
  
  if (app->game&&app->synth->song) {
    rb_song_player_adjust_tempo(app->synth->song,app->game->tempo);
  }
  
  //XXX TEMP
  //if (app->suspend>1) app->suspend--;
  
  if (rb_audio_update(app->audio)<0) return -1;
  if (rb_inmgr_update(app->inmgr)<0) return -1;
  if (ch_ossmidi_update()<0) return -1;
  if (rb_video_update(app->video)<0) return -1;
  
  struct rb_image *fb=rb_vmgr_render(app->vmgr);
  if (!fb) return -1;
  if (app->cb_postrender) {
    if (app->cb_postrender(fb,app->postrender_userdata)<0) return -1;
  }
  if (rb_video_swap(app->video,fb)<0) return -1;
  
  if (app->quit) {
    app->quit=0;
    return 0;
  }
  return 1;
}

/* Play sound effect.
 */
 
int ch_app_play_sound(struct ch_app *app,int sfx) {
  if (rb_audio_lock(app->audio)<0) return -1;
  int err=rb_synth_play_note(app->synth,sfx>>8,sfx&0xff);
  rb_audio_unlock(app->audio);
  return err;
}

static int ch_app_cb_sound(int sfx,void *userdata) {
  return ch_app_play_sound(userdata,sfx);
}

/* Forward event to game.
 */
 
static int ch_app_game_event(int eventid,void *userdata) {
  return ch_game_input(userdata,eventid);
}

/* Set game.
 */
 
int ch_app_set_game(struct ch_app *app,struct ch_game *game) {
  if (app->game==game) return 0;
  
  if (game&&(ch_game_ref(game)<0)) return -1;
  ch_game_del(app->game);
  app->game=game;
  
  if (game) {
    struct rb_grid *grid=ch_game_generate_grid(game);
    if (!grid) return -1;
    if (rb_vmgr_set_grid(app->vmgr,grid)<0) return -1;
    app->vmgr->scrollx=-((RB_FB_W-CH_TILESIZE*grid->w)>>1);
    app->vmgr->scrolly=0;
    
    struct rb_sprite_group *group=ch_game_generate_sprites(game);
    if (!group) return -1;
    if (rb_vmgr_set_sprites(app->vmgr,group)<0) return -1;
    
    game->cb_sound=ch_app_cb_sound;
    game->sound_userdata=app;
    app->cb_event=ch_app_game_event;
    app->event_userdata=game;
 
  } else {
    if (rb_vmgr_set_grid(app->vmgr,0)<0) return -1;
    if (rb_vmgr_set_sprites(app->vmgr,0)<0) return -1;
    app->cb_event=ch_app_cb_event_null;
    app->event_userdata=0;
  }
  return 0;
}

/* Play song.
 */
 
int ch_app_play_song(struct ch_app *app,int songid) {
  struct rb_song *song=0;
  if (app->songc>=1) switch (songid) {
    case CH_SONGID_SILENCE: break;
    case CH_SONGID_RANDOM: {
        int p=rand()%app->songc;
        if (p<0) p+=app->songc;
        song=app->songv[p].song;
        fprintf(stderr,"Randomly selected song %d\n",app->songv[p].id);
      } break;
    case CH_SONGID_SEQUENTIAL: {
        if (app->seqsongp>=app->songc) app->seqsongp=0;
        song=app->songv[app->seqsongp].song;
        fprintf(stderr,"Song %d, next in sequence\n",app->songv[app->seqsongp].id);
        app->seqsongp++;
      } break;
    default: {
        int p=ch_app_songv_search(app,songid);
        if (p>=0) song=app->songv[p].song;
        fprintf(stderr,"Begin song %d\n",songid);
      }
  }
  if (rb_audio_lock(app->audio)<0) return -1;
  int err=rb_synth_play_song(app->synth,song,1);
  rb_audio_unlock(app->audio);
  if (err<0) return -1;
  app->beatp=0;
  app->beatc=0;
  app->srcbeatp=0;
  return 0;
}
