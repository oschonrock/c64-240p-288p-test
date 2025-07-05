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

void dprint(char x, char y, const char* str, char xmax = 0) {
  while (char ch = *str++) {
    (screen + 40 * y)[x++] = ch;
  }
  while (x < xmax) {
    (screen + 40 * y)[x++] = SCRC(' '); // wipe rest of row until xmax
  }
}

static const char* help_text[2][14] = {
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
        "",
        SCRC("when stopped on even"),
        SCRC("row, will disappear."),
        "",
        "",
    },
};

static const char bgch           = 96; // shift space
static const char mode_help_x    = 20;
static const char mode_help_ymax = 14;
static const char global_help_x  = 14;
static const char global_help_y  = 20;

void show_help(char mode) {
  dprint(global_help_x, global_help_y + 0, SCRC("f1 - show / hide help"), 40);
  dprint(global_help_x, global_help_y + 1, SCRC("f3 - drop shadow test"), 40);
  dprint(global_help_x, global_help_y + 2, SCRC("f5 - striped sprite test"), 40);
  dprint(global_help_x, global_help_y + 3, SCRC("1..8 - change shape color"), 40);
  dprint(global_help_x, global_help_y + 4, SCRC("js p2 - move, btn = faster"), 40);

  for (char i = 0; i < ARRAYSIZE(help_text[mode]); i++) {
    dprint(mode_help_x, i, help_text[mode][i], 40);
  };
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

int main() {
  oscar_expand_lzo(sprites, sprite_data);

  __asm { sei }
  mmap_set(MMAP_CHAR_ROM);
  memcpy(charset, (char*)0xd000, 2048); // clone ROM chars so we can modify shift-space
  mmap_set(MMAP_ROM);
  __asm { cli }

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

  while (true) {
    vic_waitFrame();
    joy_poll(0);
    
    xpos += (joyb[0] + 1) * joyx[0];
    ypos += (joyb[0] + 1) * joyy[0];
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
        xpos             = 80;
        ypos             = 101;
        break;
      case KSCAN_F5:
        set_bg_char_even_rows(255); // stripey bg
        sprite_shown     = true;
        mode             = 1;
        update_help      = true;
        update_spr_image = true;
        xpos             = 80;
        ypos             = 101;
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
        clear_screen();
      }
      set_color(clr, help_shown);
      update_help = false;
    }
  }
  return 0;
}
