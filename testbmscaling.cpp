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
        for (const auto& kc: key_combos) {
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

__striped char* const screens[2] = {(char*)0x8000, (char*)0xc000}; // 0x9000 shadow charrom
__striped char* const hiress[2]  = {(char*)0xa000, (char*)0xe000}; // 0xd000 convenient i/o access

char  bank   = 0;
char  iabank = 1;
char* hires_inact;

char* const charrom = (char*)0xd000; // 1kB (first 128 screen codes) copied to font below
char* const font    = (char*)0xc400; // used for display of chars

// speed up tricky hires pointer maths
__striped const unsigned hires_row_offsets[200] = {
#for (y, 200)(40 * 8 * (y >> 3) + (y & 7)),
};

// speed up tricky hires pointer maths for text character aligned usage
__striped const unsigned hires_chrow_offsets[200] = {
#for (row, 25)(40 * 8 * row),
};

char* get_ch_ptr(char col, char row) {
  return hiress[iabank] + hires_chrow_offsets[row] + 8 * col; // use inactive ptrs
}

void write_ch(char* cp, char ch) {
  char* fp = font + ch * 8;
  for (char chrow = 0; chrow < 8; chrow++) {
    cp[chrow] = fp[chrow];
  }
}

void write_num(char x, char y, char n) {
  char* p = get_ch_ptr(x, y);
  write_ch(p, n / 10 + '0');
  write_ch(p + 8, n % 10 + '0');
}

void write_str(char x, char y, const char* str) {
  char* cp = get_ch_ptr(x, y);
  while (char ch = *str++) {
    write_ch(cp, ch);
    cp += 8;
  }
}

// clear hires area
template <int Bank>
void clear_all() {
#pragma unroll(page)
  for (unsigned i = 0; i < 8000; i++) {
    hiress[Bank][i] = 0;
  }
}

// set color for whole screen
template <int Bank>
void set_color(char color) {
#pragma unroll(page)
  for (unsigned i = 0; i < 1000; i++) {
    screens[Bank][i] = color;
  }
}

// clone one hires bank to the other
template <int SrcBank>
void clone() {
#pragma unroll(page)
  for (unsigned i = 0; i < 8000; i++) {
    hiress[SrcBank ^ 1][i] = hiress[SrcBank][i];
  }
}

const char maxchcols = 20;

char get_bcol_max(char size) {
  // aim for grid to occupy 15x15 chars to leave some room for scrolling
  return ((maxchcols - 5) * 8) / size;
}

void clear_row(char y) {
  char* rp = hiress[iabank] + hires_row_offsets[y]; // use inactive ptrs
  for (char i = 0; i < maxchcols; ++i) {
    *rp = 0;
    rp += 8;
  }
}

// clear rows of pixels between 2 version of grid
void clear(char yoffset, char yoffset_new, char size, char size_new) {
  for (char y = yoffset; y < yoffset_new; y++) { // clear above
    clear_row(y);
  }

  char ymax    = yoffset + get_bcol_max(size) * size;
  char ymaxnew = yoffset_new + get_bcol_max(size_new) * size_new;

  for (char y = ymaxnew; y < ymax; y++) { // clear below
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
      char* rp = hiress[iabank] + hires_row_offsets[y]; // use inactive ptrs
      for (char i = 0; i < maxchcols; ++i) {
        *rp = even ? evenrowbuf[i] : oddrowbuf[i];
        rp += 8;
      }
      y++;
    }
  }
}

void switch_bank(char b, bool switch_vic = true) {
  vic_waitFrame();
  bank   = b;
  iabank = bank ^ 1;
  if (switch_vic) vic_setmode(VICM_HIRES, screens[bank], hiress[bank]);
  hires_inact = hiress[iabank];
}

// write to inactive bank and switch it in
void render_grid(char x, char y, char size) {
  draw_grid(x, y, size);
  write_num(35, 0, size);
  write_num(35, 1, x);
  write_num(38, 1, y);
  switch_bank(iabank);
}

RIRQCode rirq_isr;

int main(void) {
  __asm {sei}
  cia_init();
  mmap_set(MMAP_CHAR_ROM);
  memcpy(font, charrom, 128 * 8);
  mmap_set(MMAP_NO_ROM);

  rirq_init_io();

  rirq_build(&rirq_isr, 1);
  rirq_call(&rirq_isr, 0, isr);
  rirq_set(0, 250, &rirq_isr);

  rirq_sort();
  rirq_start();

  switch_bank(0, false); // set active bank, but don't switch vic yet
  clear_all<1>();
  set_color<1>(VCOL_WHITE);
  write_str(22, 0, SCRC("block size = ")); // write on 1
  write_str(26, 1, SCRC("origin =   ,  "));

  char xs[2]    = {0, 0};
  char ys[2]    = {0, 0};
  char sizes[2] = {15, 15};
  render_grid(xs[1], ys[1], sizes[1]); // switches to bank 1
  vic.color_border = VCOL_WHITE;
  // prep other bank
  set_color<0>(VCOL_WHITE);
  clone<1>(); // copy from 1->0: incl clear, text and initial render

  char x    = xs[1]; // running params
  char y    = ys[1];
  char size = sizes[1];
  while (true) {
    if (keyb_queue & KSCAN_QUAL_DOWN) {
      char k     = keyb_queue & KSCAN_QUAL_MASK;
      keyb_queue = 0;
      switch (k) {
      case KSCAN_HOME:
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
        continue; // ignore unrecognised keys, don't clear/display/update params
      }
      clear(ys[iabank], y, sizes[iabank], size); // clear based on prev state of the inactive bank
      xs[iabank]    = x;                         // update
      ys[iabank]    = y;                         // inactive bank
      sizes[iabank] = size;                      // params
      render_grid(x, y, size);
    }
  }
  return 0;
}
