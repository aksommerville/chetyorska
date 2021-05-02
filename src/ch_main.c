#include "ch_internal.h"
#include "app/ch_app.h"
#include "game/ch_game.h"
#include <time.h>

int main(int argc,char **argv) {
  int err=0;
  int randseed=time(0);
  fprintf(stderr,"Random seed %d\n",randseed);
  srand(randseed);
  
  struct ch_app *app=ch_app_new(argc,argv);
  if (!app) return 1;
  
  //XXX TEMP: Eventually games can come and go, and there will be some lobby UI outside it.
  struct ch_game *game=ch_game_new();
  if (!game) return 1;
  if (ch_app_set_game(app,game)<0) return 1;
  
  while (1) {
    if ((err=ch_game_update(game))<0) break;
    if ((err=ch_app_update(app))<=0) break;
    
    //XXX TEMP: Detect completion and reset.
    if (!game->grid) {
      ch_game_del(game);
      if (!(game=ch_game_new())) { err=-1; break; }
      if ((err=ch_app_set_game(app,game))<0) break;
    }
  }
  
  ch_game_del(game);
  ch_app_del(app);
  return (err<0)?1:0;
}
