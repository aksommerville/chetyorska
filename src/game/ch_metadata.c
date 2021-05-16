#include "ch_internal.h"
#include "ch_game.h"

const struct ch_shape_metadata ch_shape_metadata[CH_SHAPE_COUNT]={
  {0x0660,0x0660,0x0660,0x01}, // O
  {0x00f0,0x2222,0x2222,0x02}, // I
  {0x0740,0x2230,0x6220,0x03}, // L
  {0x0710,0x3220,0x2260,0x04}, // L reverse
  {0x0720,0x2320,0x2620,0x05}, // T
  {0x0630,0x1320,0x1320,0x06}, // Z
  {0x0360,0x2310,0x2310,0x07}, // S
  
  {0x2222,0x00f0,0x00f0,0x02}, // I
  {0x2230,0x1700,0x0740,0x03}, // L
  {0x1700,0x6220,0x2230,0x03},
  {0x6220,0x0740,0x1700,0x03},
  {0x3220,0x4700,0x0710,0x04}, // L reverse
  {0x4700,0x2260,0x3220,0x04},
  {0x2260,0x0710,0x4700,0x04},
  {0x2320,0x2700,0x0720,0x05}, // T
  {0x2700,0x2620,0x2320,0x05},
  {0x2620,0x0720,0x2700,0x05},
  {0x1320,0x0630,0x0630,0x06}, // Z
  {0x2310,0x0360,0x0360,0x07}, // S
};

const struct ch_level_metadata ch_level_metadata[CH_LEVEL_COUNT]={
// 70 remains "pretty much impossible", that's where I normally die.
// 100 I believe is technically impossible, even under automation (but haven't proven yet).
  {50,1,1.20}, //   0
  {40,1,1.15}, //  10
  {30,1,1.10}, //  20
  {20,1,1.05}, //  30
  {10,1,1.00}, //  40
  { 7,1,0.90}, //  50
  { 5,1,0.80}, //  60
  { 3,1,0.70}, //  70
  { 2,1,0.60}, //  80
  { 1,1,0.50}, //  90
  { 1,2,0.50}, // 100: possible under automation
  { 1,4,0.50}, // 110: might just barely be possible under automation -- i was not able to complete 10 lines
  { 1,8,0.50}, // 100: definitely not possible
};
