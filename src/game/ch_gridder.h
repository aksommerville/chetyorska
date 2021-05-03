/* ch_gridder.h
 * Manages logical regions of the display grid.
 */
 
#ifndef CH_GRIDDER_H
#define CH_GRIDDER_H

struct ch_gridder {
  struct rb_grid *grid;
  struct ch_gridder_region {
    int id;
    int x,y,w,h;
  } *regionv;
  int regionc,regiona;
};

void ch_gridder_cleanup(struct ch_gridder *gridder);
int ch_gridder_set_grid(struct ch_gridder *gridder,struct rb_grid *grid);

/* Convenience: Safely read a cell at (x,y), optionally offsetting from (region).
 * 0xff if OOB.
 */
uint8_t ch_gridder_read(
  const struct ch_gridder *gridder,
  const struct ch_gridder_region *region,
  int x,int y
);

/* "new" fails if already defined.
 * "get" optionally creates if not already defined.
 * Don't retain these pointers; they'll be invalidated by any future new or delete.
 * You may set regions' geometry yourself, but they must remain in bounds always.
 */
struct ch_gridder_region *ch_gridder_new_region(struct ch_gridder *gridder,int id);
struct ch_gridder_region *ch_gridder_get_region(struct ch_gridder *gridder,int id,int create);
void ch_gridder_delete_region(struct ch_gridder *gridder,int id);
void ch_gridder_delete_all(struct ch_gridder *gridder);

/* Valid regions have positions in 0..(w,h), sizes >=0, and do not exceed the right or bottom edge.
 * (an all-zeroes region is always valid)
 */
int ch_gridder_validate_region(const struct ch_gridder *gridder,const struct ch_gridder_region *region);
void ch_gridder_sanitize_region(const struct ch_gridder *gridder,struct ch_gridder_region *region);
int ch_gridder_validate_all(const struct ch_gridder *gridder);
void ch_gridder_sanitize_all(struct ch_gridder *gridder);

void ch_gridder_fill(struct ch_gridder *gridder,uint8_t tileid);

/* "fill" covers the region entirely with just one tile.
 * "frame" draw a 1-cell outline within the region, (tileid) is top-left corner of a 3x3 box image.
 * "framefill" does both.
 * These are guaranteed safe for null (region), you can feed directly from ch_gridder_get_region().
 * Beyond that, (region) is presumed valid and we don't check.
 */
void ch_gridder_fill_region(struct ch_gridder *gridder,const struct ch_gridder_region *region,uint8_t tileid);
void ch_gridder_frame_region(struct ch_gridder *gridder,const struct ch_gridder_region *region,uint8_t tileid);
void ch_gridder_framefill_region(struct ch_gridder *gridder,const struct ch_gridder_region *region,uint8_t frame,uint8_t fill);

/* Draws a continuous-value progress bar, 1 cell tall and >2 width.
 * (tileid) is the left edge, from there, the ten source tiles are:
 *   LEFT, 0, RIGHT, 1, 2, 3, 4, 5, 6, OVERFLOW
 * The bar draws as OVERFLOW if (v>c).
 */
void ch_gridder_continuous_bar(
  struct ch_gridder *gridder,
  const struct ch_gridder_region *region,
  uint8_t tileid,
  int v,int c
);

/* Bulk copy from the tilesheet to (region+(1,1)), 2 rows high, (w) long.
 */
void ch_gridder_text_label(
  struct ch_gridder *gridder,
  const struct ch_gridder_region *region,
  uint8_t tileid,
  int w
);

/* Print a positive decimal integer (v) using 10x2 tiles starting at (tileid).
 * Output digits clamp to zero and straight nines.
 * Region is presumed to have a border, its height should be exactly 4.
 * (labelw) is inviolate left edge, eg from ch_gridder_text_label().
 */
void ch_gridder_text_number(
  struct ch_gridder *gridder,
  const struct ch_gridder_region *region,
  int labelw,
  uint8_t tileid,
  int v
);

/* Draw a brick, up to 16 tiles but of course they only use 4 each.
 * (x,y) is the top-left destination cell.
 * (shape) is a 4x4 bitmap (LRTB); the set bits will be overwritten with (tileid).
 * If provided, (scissor) is a limit outside which we do not draw.
 */
void ch_gridder_fill_shape(
  struct ch_gridder *gridder,
  int x,int y,
  uint16_t shape,
  uint8_t tileid,
  const struct ch_gridder_region *scissor
);

#endif
