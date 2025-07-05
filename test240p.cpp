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
        SCRC("flicker test"),
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

static const char bgch = 96; // shift space

void show_help(char mode) {
  dprint(9, 20, SCRC("joystick port 2 - move shape"), 40);
  dprint(9, 21, SCRC("1..8 - change shape color"), 40);
  dprint(9, 22, SCRC("f1 - show / hide help"), 40);
  dprint(9, 23, SCRC("f3 - drop shadow / flicker test"), 40);
  dprint(9, 24, SCRC("f5 - striped sprite test"), 40);

  for (char i = 0; i < ARRAYSIZE(help_text[mode]); i++) {
    dprint(20, i, help_text[mode][i], 40);
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

int main() {
  oscar_expand_lzo(sprites, sprite_data);

  __asm { sei }
  mmap_set(MMAP_CHAR_ROM);
  memcpy(charset, (char*)0xd000, 2048); // clone ROM chars so we can modify shift-space
  mmap_set(MMAP_ROM);
  __asm { cli }

  memset(screen, bgch, 1000);

  vic.color_border = VCOL_LT_GREY;
  vic.color_back   = VCOL_LT_GREY;
  vic_setmode(VICM_TEXT, screen, charset);
  spr_init(screen);
  vic.spr_multi = vic.spr_expand_x = vic.spr_expand_y = 0;

  unsigned xpos             = 80;
  char     ypos             = 101;
  bool     sprite_shown     = true;
  bool     help_shown       = true;
443  char     mode             = 0;
  char     clr              = 0;
  bool     flicker          = true;
  bool     update_help      = true;
  bool     update_spr_image = true;

  while (true) {
    vic_waitFrame();
    joy_poll(0);
    xpos += joyx[0];
    ypos += joyy[0];
    update_sprites(xpos, ypos, sprite_shown);
    if (flicker) sprite_shown = !sprite_shown;

    keyb_poll();
    if (keyb_key & KSCAN_QUAL_DOWN) {
      switch (keyb_key & 0x7f) {
      case KSCAN_F1:
        help_shown  = !help_shown;
        update_help = true;
        break;
      case KSCAN_F3:
        set_bg_char_even_rows(0); // empty bg
        flicker          = true;
        mode             = 0;
        update_help      = true;
        update_spr_image = true;
        xpos             = 80;
        ypos             = 101;
        break;
      case KSCAN_F5:
        set_bg_char_even_rows(255); // stripey bg
        sprite_shown     = true;
        flicker          = false;
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
    if (update_help) {
      update_spr_image = true;
      if (help_shown) {
        show_help(mode);
      } else {
#pragma unroll(page)
        for (unsigned i = 0; i < 1000; i++) {
          screen[i] = bgch;
        }
      }
      update_help = false;
    }
    if (update_spr_image) {
      set_spr_image(mode, clr);
#pragma unroll(page)
      for (unsigned i = 0; i < 1000; i++) {
        color[i] = clr;
      }
      if (help_shown) {
        for (char y = 20; y < 25; y++) {
          for (char x = 9; x < 40; x++) {
            (color + y * 40)[x] = VCOL_BLACK;
          }
        }
        for (char y = 0; y < 14; y++) {
          for (char x = 20; x < 40; x++) {
            (color + y * 40)[x] = VCOL_BLACK;
          }
        }
      }
      update_spr_image = false;
    }
  }

  return 0;
}
