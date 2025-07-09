#include "clangd.h"
#include <c64/cia.h>
#include <c64/keyboard.h>
#include <c64/memmap.h>
#include <c64/vic.h>
#include <oscar.h>
#include <string.h>

// Place bitmap memory underneath kernal ROM
char* const hires = (char*)0xe000;

// Place cell color map underneath IO
char* const screen  = (char*)0xd000;
char* const charrom = (char*)0xd000;

static char numchars[80];

void write_ch(char x, char y, char d) {
  for (char chrow = 0; chrow < 8; chrow++) {
    hires[y * 40 * 8 + 8 * x + chrow] = numchars[d * 8 + chrow];
  }
}

void write_num(char x, char y, char n) {
  write_ch(x + 0, y, n / 10);
  write_ch(x + 1, y, n % 10);
}

void clear_all() {
#pragma unroll(page)
  for (unsigned i = 0; i < 8000; i++) {
    hires[i] = 0;
  }
}

void set(char x, char y) {
  unsigned yoffset = 40 * 8 * (y >> 3) + (y & 7);
  char     mask    = 0x80 >> (x & 7);
  char     xoffset = (x & ~7);
  hires[yoffset + xoffset] |= mask;
}

const char maxchcols = 20;

char get_bcol_max(char size) {
  return ((maxchcols - 5) * 8) / size;
}

void clear_row(char y) {
  char* rp = hires + 40 * 8 * (y >> 3) + (y & 7);
  for (char i = 0; i < maxchcols; ++i) {
    *rp = 0;
    rp += 8;
  }
}

void clear(char yoffset, char yoffsetnew, char size, char newsize) {
  for (char y = yoffset; y < yoffsetnew; y++) {
    clear_row(y);
  }

  char ymax    = yoffset + get_bcol_max(size) * size;
  char ymaxnew = yoffsetnew + get_bcol_max(newsize) * newsize;

  for (char y = ymaxnew; y < ymax; y++) {
    clear_row(y);
  }
}

__striped char* const hires_ptr[200] = {
#for (y, 200)(hires + 40 * 8 * (y >> 3) + (y & 7)),
};

void draw_grid(char xoffset, char yoffset, char size) {
  char bcol_max = get_bcol_max(size);

  char oddrowbuf[maxchcols];
  char evenrowbuf[maxchcols];

  for (char i = 0; i < maxchcols; ++i) {
    oddrowbuf[i]  = 0;
    evenrowbuf[i] = 0;
  }

  // build the 2 types of row
  for (char brow = 0; brow < 2; brow++) {
    bool  even = !(brow & 1);
    char* buf  = even ? evenrowbuf : oddrowbuf;
    char  x    = xoffset + (even ? 0 : size);
    for (char bcol = 0; bcol < bcol_max / 2 + (bcol_max & 1 && even); bcol++) {
      for (char w = 0; w < size; w++) {
        char mask = 0x80 >> (x & 7);
        buf[x / 8] |= mask;
        x++;
      }
      x += size;
    }
  }
  // just copy the appropriate pre-built row, for all even and odd rows
  char y = yoffset;
  for (char brow = 0; brow < bcol_max; brow++) {
    bool even = !(brow & 1);
    for (char h = 0; h < size; h++) {
      char* rp = hires_ptr[y];
      for (char i = 0; i < maxchcols; ++i) {
        *rp = even ? evenrowbuf[i] : oddrowbuf[i];
        rp += 8;
      }
      y++;
    }
  }
}

void display(char x, char y, char size) {
  draw_grid(x, y, size);
  write_num(35, 0, size);
  write_num(35, 1, x);
  write_num(38, 1, y);
}

int main(void) {
  __asm {cli}
  cia_init();
  mmap_set(MMAP_CHAR_ROM);
  memcpy(numchars, charrom + 48 * 8, 80);
  mmap_set(MMAP_RAM);
  memset(screen, 0x01, 1000);
  mmap_set(MMAP_NO_ROM);
  __asm {sei}
  vic_setmode(VICM_HIRES, screen, hires);
  vic.color_border = VCOL_WHITE;
  char x           = 0;
  char y           = 0;
  char size        = 15;
  clear_all();
  display(x, y, size);
  while (true) {
    vic_waitFrame();
    keyb_poll();
    if (keyb_key & KSCAN_QUAL_DOWN) {
      switch (keyb_key & 0x7f) {
      case KSCAN_HOME:
        clear(y, 0, size, 15);
        x    = 0;
        y    = 0;
        size = 15;
        break;
      case KSCAN_EQUAL:
      case KSCAN_PLUS:
        if (size < 60) {
          clear(y, y, size, size + 1);
          ++size;
        }
        break;
      case KSCAN_MINUS:
        if (size > 1) {
          clear(y, y, size, size - 1);
          --size;
        }
        break;
      case KSCAN_CSR_DOWN:
        if (y < (200 - get_bcol_max(size) * size)) {
          clear(y, y + 1, size, size);
          ++y;
        }
        break;
      case KSCAN_CSR_DOWN | KSCAN_QUAL_SHIFT:
        if (y > 0) {
          clear(y, y - 1, size, size);
          --y;
        }
        break;
      case KSCAN_CSR_RIGHT:
        if (x < (maxchcols * 8 - get_bcol_max(size) * size)) {
          ++x;
        }
        break;
      case KSCAN_CSR_RIGHT | KSCAN_QUAL_SHIFT:
        if (x > 0) {
          --x;
        }
        break;
      }
      display(x, y, size);
    }
  }
  return 0;
}
