#ifndef CH_OSSMIDI_H
#define CH_OSSMIDI_H

int ch_ossmidi_init(
  int (*cb)(void *userdata,int devid,const void *src,int srcc),
  void *userdata
);

void ch_ossmidi_quit();

int ch_ossmidi_update();

#endif
