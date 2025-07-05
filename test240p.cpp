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
        SCRC("flickering black"),
        SCRC("shape. it should not"),
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
        SCRC("solid black shape"),
        SCRC("on stripey bg."),
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

void show_help(char mode) {
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

static const char bgch = 96; // shift space

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
  memset(color, VCOL_BLACK, 1000);

  vic.color_border = VCOL_LT_GREY;
  vic.color_back   = VCOL_LT_GREY;
  vic_setmode(VICM_TEXT, screen, charset);
  spr_init(screen);
  vic.spr_multi = vic.spr_expand_x = vic.spr_expand_y = 0;

  dprint(9, 22, SCRC("joystick port 2 - move shape"), 40);
  dprint(9, 23, SCRC("f1 - drop shadow / flicker test"), 40);
  dprint(9, 24, SCRC("f3 - striped sprite test"), 40);

  unsigned xpos = 80;
  char     ypos = 101;
  bool shown   = true;
  bool flicker = true;
  show_help(0);
  set_spr_image(0, VCOL_BLACK);

  while (true) {
    vic_waitFrame();
    
    joy_poll(0);
    xpos += joyx[0];
    ypos += joyy[0];
    update_sprites(xpos, ypos, shown);
    if (flicker) shown = !shown;
    
    keyb_poll();
    if (keyb_key & KSCAN_QUAL_DOWN) {
      switch (keyb_key & 0x7f) {
      case KSCAN_F1:
        set_bg_char_even_rows(0);     // empty bg
        set_spr_image(0, VCOL_BLACK); // use solid sprite
        flicker = true;
        show_help(0);
        xpos = 80;
        ypos = 101;
        break;
      case KSCAN_F3:
        set_bg_char_even_rows(255);   // stripey bg
        set_spr_image(1, VCOL_BLACK); // use stripey sprite
        shown   = true;
        flicker = false;
        show_help(1);
        xpos = 80;
        ypos = 101;
        break;
      }
    }
  }
  return 0;
}
