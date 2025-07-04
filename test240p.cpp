#include "clangd.h"
#include <c64/joystick.h>
#include <c64/keyboard.h>
#include <c64/memmap.h>
#include <c64/sprites.h>
#include <c64/vic.h>
#include <oscar.h>
#include <string.h>

static char* const sprites = (char*)0x3000;
static char* const screen  = (char*)0x0400;
static char* const charset = (char*)0x2000;
static char* const color   = (char*)0xd800;

static const char spr_offset    = (unsigned)sprites / 64;
const char        sprite_data[] = {
#embed spd_sprites lzo "sprites.spd"
};

void dprint(char x, char y, const char* str) {
  while (char ch = *str++) {
    (screen + 40 * y)[x] = ch;
    x++;
  }
}

void show_help(char mode) {
  for (char y = 0; y < 22; y++) {
    for (char x = 20; x < 40; x++) {
      (screen + 40 * y)[x] = SCRC(' ');
    }
  }
  switch (mode) {
    case 0:
      dprint(20, 0, SCRC("flicker test"));
      dprint(20, 2, SCRC("you should see a"));
      dprint(20, 3, SCRC("flickering black"));
      dprint(20, 4, SCRC("shape. it should not"));
      dprint(20, 5, SCRC("be striped."));
      dprint(20, 7, SCRC("move it with joyst."));
      dprint(20, 8, SCRC("no stripes or arti-"));
      dprint(20, 9, SCRC("facts?"));
      dprint(20, 11, SCRC("proves your display"));
      dprint(20, 12, SCRC("understands c64 pro-"));
      dprint(20, 13, SCRC("gressive signal."));
      break;
    case 1:
      dprint(20, 0, SCRC("striped test"));
      dprint(20, 2, SCRC("you should see a"));
      dprint(20, 3, SCRC("solid black shape"));
      dprint(20, 4, SCRC("on stripey bg."));
      dprint(20, 6, SCRC("move it with joyst."));
      dprint(20, 7, SCRC("will appear stripey"));
      dprint(20, 8, SCRC("while moving vert."));
      dprint(20, 10, SCRC("when stopped on even"));
      dprint(20, 11, SCRC("row will disappear."));
      break;
  }
}

int main() {
  oscar_expand_lzo(sprites, sprite_data);

  __asm { sei }
  mmap_set(MMAP_CHAR_ROM);
  memcpy(charset, (char*)0xd000, 2048); // clone ROM chars so we can modify shift-space
  mmap_set(MMAP_ROM);
  __asm { cli }

  char bgch = 96; // shift space
  memset(screen, bgch, 1000);
  memset(color, VCOL_BLACK, 1000);

  vic.color_border = VCOL_LT_GREY;
  vic.color_back   = VCOL_LT_GREY;
  vic_setmode(VICM_TEXT, screen, charset);
  spr_init(screen);
  unsigned xpos = 80;
  char     ypos = 101;
  spr_set(0, true, 0, 0, spr_offset + 0, VCOL_BLACK, false, false, false);
  spr_set(1, true, 0, 0, spr_offset + 1, VCOL_BLACK, false, false, false);
  spr_set(2, true, 0, 0, spr_offset + 2, VCOL_BLACK, false, false, false);
  spr_set(3, true, 0, 0, spr_offset + 3, VCOL_BLACK, false, false, false);

  dprint(9, 22, SCRC("joystick port 2 - move shape   "));
  dprint(9, 23, SCRC("f1 - drop shadow / flicker test"));
  dprint(9, 24, SCRC("f3 - striped sprite test       "));

  bool shown   = true;
  bool flicker = true;
  show_help(0);
  
  while (true) {
    vic_waitFrame();
    joy_poll(0);
    xpos += joyx[0];
    ypos += joyy[0];

    spr_move(0, xpos, ypos);
    spr_move(1, xpos + 24, ypos);
    spr_move(2, xpos, ypos + 21);
    spr_move(3, xpos + 24, ypos + 21);

    spr_show(0, shown);
    spr_show(1, shown);
    spr_show(2, shown);
    spr_show(3, shown);
    if (flicker) shown = !shown;
    keyb_poll();
    if (keyb_key & KSCAN_QUAL_DOWN) {
      switch (keyb_key & 0x7f) {
      case KSCAN_F1:
        // clear bgch
        charset[bgch * 8 + 0] = 0x00;
        charset[bgch * 8 + 2] = 0x00;
        charset[bgch * 8 + 4] = 0x00;
        charset[bgch * 8 + 6] = 0x00;
        // use solid sprite
        spr_image(0, spr_offset + 0);
        spr_image(1, spr_offset + 1);
        spr_image(2, spr_offset + 2);
        spr_image(3, spr_offset + 3);
        flicker = true;
        show_help(0);
        xpos = 80;
        ypos = 101;
        break;
      case KSCAN_F3:
        // stripey bg ch
        charset[bgch * 8 + 0] = 0xff;
        charset[bgch * 8 + 2] = 0xff;
        charset[bgch * 8 + 4] = 0xff;
        charset[bgch * 8 + 6] = 0xff;

        // stripey sprite
        spr_image(0, spr_offset + 4);
        spr_image(1, spr_offset + 5);
        spr_image(2, spr_offset + 6);
        spr_image(3, spr_offset + 7);
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
