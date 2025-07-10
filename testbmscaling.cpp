#include "clangd.h"
#include <c64/cia.h>
#include <c64/keyboard.h>
#include <c64/memmap.h>
#include <c64/rasterirq.h>
#include <c64/vic.h>
#include <opp/array.h>
#include <oscar.h>
#include <string.h>

#define ARRAYSIZE(arr) sizeof(arr) / sizeof(arr[0])

char keyb_queue, keyb_repeat;
char csr_cnt;
char irq_cnt;
char msg_cnt;

struct key_combo {
  KeyScanCode scan_code;
  bool        allow_shift;
};
static const key_combo key_combos[5] = {
    {KSCAN_CSR_RIGHT, true}, {KSCAN_CSR_DOWN, true}, {KSCAN_PLUS, false},
    {KSCAN_MINUS, false},    {KSCAN_EQUAL, false}, // for emulator compatibility
};

__interrupt void isr(void) {
  keyb_poll();
  if (!keyb_queue) {
    if (!(keyb_key & KSCAN_QUAL_DOWN)) {
      if (keyb_repeat)
        keyb_repeat--;
      else {
        for (char i = 0; i < ARRAYSIZE(key_combos); ++i) {
          const auto& kc = key_combos[i];
          if (key_pressed(kc.scan_code)) {
            keyb_queue = kc.scan_code | KSCAN_QUAL_DOWN;
            if (kc.allow_shift && key_shift()) keyb_queue |= KSCAN_QUAL_SHIFT;
            keyb_repeat = 5; // repeat 10x per second, paint and swap takes ~80ms
          }
        }
      }
    } else {
      keyb_queue  = keyb_key;
      keyb_repeat = 20;
    }
  }
}

char* const screens[] = {(char*)0x8000, (char*)0xc000};
char* const hiress[]  = {(char*)0xa000, (char*)0xe000};
char        bank      = 0;
char*       hires_inact;

char* const charrom = (char*)0xd000;
char* const font    = (char*)0xc400;

void write_ch(char x, char y, char ch) {
  for (char chrow = 0; chrow < 8; chrow++) {
    hires_inact[y * 40 * 8 + 8 * x + chrow] = font[ch * 8 + chrow];
  }
}

void write_num(char x, char y, char n) {
  write_ch(x + 0, y, n / 10 + '0');
  write_ch(x + 1, y, n % 10 + '0');
}

void write_str(char x, char y, const char* str) {
  while (char ch = *str++) write_ch(x++, y, ch);
}

void clear_all(char b) {
#pragma unroll(page)
  for (unsigned i = 0; i < 8000; i++) {
    hiress[b][i] = 0;
  }
}

const char maxchcols = 20;

char get_bcol_max(char size) {
  return ((maxchcols - 5) * 8) / size;
}

__striped char* hires_ptrs0[200] = {
#for (y, 200)(hiress[0] + 40 * 8 * (y >> 3) + (y & 7)),
};
__striped char* hires_ptrs1[200] = {
#for (y, 200)(hiress[1] + 40 * 8 * (y >> 3) + (y & 7)),
};

void clear_row(char y) {
  char* rp = bank == 0 ? hires_ptrs1[y] : hires_ptrs0[y];
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
      char* rp = bank == 0 ? hires_ptrs1[y] : hires_ptrs0[y];
      for (char i = 0; i < maxchcols; ++i) {
        *rp = even ? evenrowbuf[i] : oddrowbuf[i];
        rp += 8;
      }
      y++;
    }
  }
}

void switch_bank(char b) {
  vic_waitFrame();
  vic_setmode(VICM_HIRES, screens[b], hiress[b]);
  hires_inact = hiress[b ^ 1];
  bank        = b;
}

// write to inactive bank and switch it in
void display(char x, char y, char size) {
  draw_grid(x, y, size);
  write_num(35, 0, size);
  write_num(35, 1, x);
  write_num(38, 1, y);
  switch_bank(bank ^ 1);
}

RIRQCode rirq_isr;

int main(void) {
  __asm {sei}
  cia_init();
  mmap_set(MMAP_CHAR_ROM);
  memcpy(font, charrom, 128 * 8);
  mmap_set(MMAP_RAM);
  memset(screens[0], 0x01, 1000);
  memset(screens[1], 0x01, 1000);
  mmap_set(MMAP_NO_ROM);

  rirq_init_io();

  rirq_build(&rirq_isr, 1);
  rirq_call(&rirq_isr, 0, isr);
  rirq_set(0, 250, &rirq_isr);

  rirq_sort();
  rirq_start();

  clear_all(0);
  clear_all(1);
  switch_bank(0);
  vic.color_border = VCOL_WHITE;

  char xs[2]    = {0, 0};
  char ys[2]    = {0, 0};
  char sizes[2] = {15, 15};
  display(xs[0], ys[0], sizes[0]);
  char x    = xs[1];
  char y    = ys[1];
  char size = sizes[1];

  while (true) {
    if (keyb_queue & KSCAN_QUAL_DOWN) {
      char k     = keyb_queue & KSCAN_QUAL_MASK;
      keyb_queue = 0;
      switch (k) {
      case KSCAN_HOME:
        clear(y, 0, size, 15);
        x    = 0;
        y    = 0;
        size = 15;
        break;
      case KSCAN_EQUAL:
      case KSCAN_PLUS:
        if (size < 60) ++size;
        break;
      case KSCAN_MINUS:
        if (size > 1) --size;
        break;
      case KSCAN_CSR_DOWN:
        if (y < (200 - get_bcol_max(size) * size)) ++y;
        break;
      case KSCAN_CSR_DOWN | KSCAN_QUAL_SHIFT:
        if (y > 0) --y;
        break;
      case KSCAN_CSR_RIGHT:
        if (x < (maxchcols * 8 - get_bcol_max(size) * size)) ++x;
        break;
      case KSCAN_CSR_RIGHT | KSCAN_QUAL_SHIFT:
        if (x > 0) --x;
        break;
      default:
        continue;
      }
      char ia = bank ^ 1;
      clear(ys[ia], y, sizes[ia], size); // clear based on prev state of this inactive bank
      xs[ia]    = x;                     // update inactive bank params
      ys[ia]    = y;
      sizes[ia] = size;
      display(x, y, size);
    }
  }
  return 0;
}
