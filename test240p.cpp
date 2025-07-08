#include "clangd.h"
#include <c64/joystick.h>
#include <c64/keyboard.h>
#include <c64/memmap.h>
#include <c64/sprites.h>
#include <c64/vic.h>
#include <oscar.h>
#include <string.h>

#define ARRAYSIZE(arr) sizeof(arr) / sizeof(arr[0])

static char* const sprites = (char*)0x3000;
static char* const screen  = (char*)0x0400;
static char* const charset = (char*)0x2000;
static char* const color   = (char*)0xd800;

static const char spr_offset    = (unsigned)sprites / 64;
const char        sprite_data[] = {
#embed spd_sprites lzo "sprites.spd"
};

const char char_data[] = {
#embed ctm_chars lzo "grids.ctm"
};

static constexpr const char tiles[] = {
#embed ctm_tiles8 "grids.ctm"
};

static constexpr const char map[] = {
#embed ctm_map8 "grids.ctm"
};

void dprint(char x, char y, const char* str, char xmax = 0) {
  while (char ch = *str++) {
    (screen + 40 * y)[x++] = ch;
  }
  while (x < xmax) {
    (screen + 40 * y)[x++] = SCRC(' '); // wipe rest of row until xmax
  }
}

static const char* help_text[3][16] = {
    {
        SCRC("drop shadow test"),
        "",
        SCRC("you should see a"),
        SCRC("flickering shape."),
        SCRC("it should not"),
        SCRC("be striped."),
        "",
        SCRC("move it with joyst."),
        SCRC("no stripes or arti-"),
        SCRC("facts?"),
        "",
        SCRC("proves your display"),
        SCRC("understands c64 pro-"),
        SCRC("gressive signal."),
        "",
        "",
    },
    {
        SCRC("striped sprite test"),
        "",
        SCRC("you should see a"),
        SCRC("solid shape on a "),
        SCRC("stripey background."),
        "",
        SCRC("move it with joyst."),
        SCRC("will appear stripey"),
        SCRC("while moving vert."),
        SCRC("will disappear when"),
        SCRC("stopped on even row."),
        "",
        SCRC("otherwise no arti-"),
        SCRC("facts?"),
        "",
        "",
    },
    {
        SCRC("scaling test"),
        "",
        SCRC("you should see a"),
        SCRC("a set of grids on"),
        SCRC("left. use magnifi-"),
        SCRC("cation. count lcd"),
        SCRC("pixels for each"),
        SCRC("black square."),
        "",
        SCRC("find the grid which"),
        SCRC("matches an integer"),
        SCRC("number of pixels."),
        "",
        SCRC("the grids are"),
        SCRC("1x1 => 12x12"),
        SCRC("c64 pixels"),
    },
};

static const char bgch           = 64; // special custom space
static const char mode_help_x    = 20;
static const char mode_help_ymax = 16;
static const char global_help_x  = 16;
static const char global_help_y  = 19;

void show_help(char mode) {
  dprint(global_help_x, global_help_y + 0, SCRC("f1 - show / hide help"), 40);
  dprint(global_help_x, global_help_y + 1, SCRC("f3 - drop shadow test"), 40);
  dprint(global_help_x, global_help_y + 2, SCRC("f5 - striped sprite test"), 40);
  dprint(global_help_x, global_help_y + 3, SCRC("f7 - scaling test"), 40);
  dprint(global_help_x, global_help_y + 4, SCRC("1..8 - change color"), 40);
  dprint(global_help_x, global_help_y + 5, SCRC("js p2 - move, btn=faster"), 40);

  for (char i = 0; i < ARRAYSIZE(help_text[mode]); i++) {
    dprint(mode_help_x, i, help_text[mode][i], 40);
  };
}

void clear_help() {
  for (char y = 0; y < mode_help_ymax; y++) {
    for (char x = mode_help_x; x < 40; x++) {
      (screen + 40 * y)[x] = bgch;
    }
  }
  for (char y = global_help_y; y < 25; y++) {
    for (char x = global_help_x; x < 40; x++) {
      (screen + 40 * y)[x] = bgch;
    }
  }
}

void set_spr_image(char mode, char color) {
  for (char i = 0; i < 4; i++) {
    spr_image(i, spr_offset + 4 * mode + i);
    spr_color(i, color);
  }
}

void update_sprites(unsigned xpos, char ypos, bool shown) {
  static const char xoffset[4] = {0, 24, 0, 24};
  static const char yoffset[4] = {0, 0, 21, 21};

  for (char i = 0; i < 4; i++) {
    spr_move(i, xpos + xoffset[i], ypos + yoffset[i]);
    spr_show(i, shown);
  }
}

void set_bg_char_even_rows(char rowval) {
  for (char i = 0; i < 4; i++) charset[bgch * 8 + (i * 2)] = rowval;
}

void clear_screen() {
#pragma unroll(page)
  for (unsigned i = 0; i < 1000; i++) {
    screen[i] = bgch;
  }
}

void set_color(char clr, bool help_shown) {
  char* cp = color;
  for (char y = 0; y < 25; y++) {
    for (char x = 0; x < 40; x++) {
      char c = (help_shown && ((y < mode_help_ymax && x >= mode_help_x) ||
                               (y >= global_help_y && x >= global_help_x)))
                   ? VCOL_BLACK
                   : clr;

      cp[x] = c;
    }
    cp += 40;
  }
}

void show_grids() {
  const char  rows = 4;
  const char  cols = 3;
  const char* mp   = map;

  char* sp = screen;

  for (char y = 0; y < rows; y++) {
    for (char x = 0; x < cols; x++) {
      const char* tp = tiles + 9 * mp[x]; // cast to positive
#pragma unroll(full)
      for (char iy = 0; iy < 3; iy++) {
#pragma unroll(full)
        for (char ix = 0; ix < 3; ix++) {
          char charnum = tp[3 * iy + ix];
          char scridx  = 40 * iy + ix;
          sp[scridx]   = charnum + 64;
        }
      }
      sp += 3;
    }
    mp += cols;
    sp += 2 * 40 + (40 - 3 * cols); // already cumulatively inc'd by 12 above
  }
}

struct seqstep {
  int dx;
  int dy;
  int scount; // -1 mean absolute jump
};

seqstep steps[] = {
    {70, 75, -1}, {1, 1, 50},    {-1, 1, 50},  {-1, -1, 50},
    {1, -1, 50},  {70, 175, -1}, {0, -1, 100}, {0, 1, 100},
};

int main() {
  oscar_expand_lzo(sprites, sprite_data);

  __asm { sei }
  mmap_set(MMAP_CHAR_ROM);
  memcpy(charset, (char*)0xd000, 2048); // clone ROM chars so we can modify shift-space
  mmap_set(MMAP_ROM);
  __asm { cli }
  oscar_expand_lzo(charset + 64 * 8, char_data); // put custom grid chars staring at screen code 64

  clear_screen();
  vic.color_border = VCOL_LT_GREY;
  vic.color_back   = VCOL_LT_GREY;
  vic_setmode(VICM_TEXT, screen, charset);
  spr_init(screen);
  vic.spr_multi = vic.spr_expand_x = vic.spr_expand_y = 0;

  unsigned xpos             = 80;
  char     ypos             = 101;
  bool     sprite_shown     = true;
  bool     help_shown       = true;
  char     mode             = 0;
  char     clr              = 0;
  bool     update_help      = true;
  bool     update_spr_image = true;
  bool     annimation       = false;
  char     idle_cnt         = 0;
  seqstep* step_ptr         = steps;
  int      scnt             = step_ptr->scount;
  while (true) {
    vic_waitFrame();
    joy_poll(0);
    if (joyx[0] != 0 || joyy[0] != 0) {
      idle_cnt   = 0;
      annimation = false;
    }
    if (annimation) {
      if (scnt == -1) {
        xpos = step_ptr->dx;
        ypos = step_ptr->dy;
        step_ptr++;
        if (step_ptr == steps + ARRAYSIZE(steps)) step_ptr = steps;
        scnt = step_ptr->scount;
      } else {
        xpos += step_ptr->dx;
        ypos += step_ptr->dy;
        scnt--;
        if (!scnt) {
          step_ptr++;
          if (step_ptr == steps + ARRAYSIZE(steps)) step_ptr = steps;
          scnt = step_ptr->scount;
        }
      }
    } else {
      if (joyx[0] == 0 && joyy[0] == 0) {
        idle_cnt++;
        if (++idle_cnt == 250) {
          idle_cnt   = 0;
          annimation = true;
        }
      } else {
        if (mode == 0) {
          xpos += (joyb[0] + 1) * joyx[0];
          ypos += (joyb[0] + 1) * joyy[0];
        } else { // dont' allow speed up in striped sprite test, because it can hide the sprite
          xpos += joyx[0];
          ypos += joyy[0];
        }
      }
    }
    update_sprites(xpos, ypos, sprite_shown);
    if (mode == 0) sprite_shown = !sprite_shown;

    keyb_poll();
    if (keyb_key & KSCAN_QUAL_DOWN) {
      switch (keyb_key & 0x7f) {
      case KSCAN_F1:
        help_shown  = !help_shown;
        update_help = true;
        break;
      case KSCAN_F3:
        set_bg_char_even_rows(0); // empty bg
        mode             = 0;
        update_help      = true;
        update_spr_image = true;
        clear_screen();
        step_ptr = steps;
        scnt     = step_ptr->scount;
        break;
      case KSCAN_F5:
        set_bg_char_even_rows(255); // stripey bg
        sprite_shown     = true;
        mode             = 1;
        update_help      = true;
        update_spr_image = true;
        clear_screen();
        step_ptr = steps;
        scnt     = step_ptr->scount;
        break;
      case KSCAN_F7:
        set_bg_char_even_rows(0); // empty bg
        mode = 2;
        show_grids();
        sprite_shown = false;
        update_help  = true;
        break;
      default:
        char ch = keyb_codes[keyb_key & 0x7f];
        if ('1' <= ch && ch <= '8') {
          clr              = ch - '1';
          update_spr_image = true;
        }
        break;
      }
    }
    if (update_spr_image) {
      set_spr_image(mode, clr);
      set_color(clr, help_shown);
      update_spr_image = false;
    }
    if (update_help) {
      if (help_shown) {
        show_help(mode);
      } else {
        clear_help();
      }
      set_color(clr, help_shown);
      update_help = false;
    }
  }
  return 0;
}
