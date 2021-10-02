#include "ch_internal.h"
#include "app/ch_app.h"
#include "ui/ch_ui.h"
#include <time.h>

int main(int argc,char **argv) {
  int err=0;
  int randseed=time(0);
  fprintf(stderr,"Random seed %d\n",randseed);
  srand(randseed);
  
  struct ch_app *app=ch_app_new(argc,argv);
  if (!app) return 1;
  if (ch_app_play_song(app,CH_SONGID_LOBBY)<0) return 1;
  
  struct ch_ui *ui=ch_ui_new(app);
  if (!ui) return 1;

  while (1) {
    if ((err=ch_app_update(app))<=0) break;
    if (1||(app->suspend!=1)) {//XXX
      if ((err=ch_ui_update(ui))<=0) break;
    }
  }
  
  ch_ui_del(ui);
  ch_app_del(app);
  return (err<0)?1:0;
}
