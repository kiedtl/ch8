#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>
#include <stdbool.h>

static const uint8_t fonts[] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

static const uint8_t s_fonts[] = {
	0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, // 0
	0x18, 0x78, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF, // 1
	0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // 2
	0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 3
	0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03, // 4
	0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 5
	0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 6
	0xFF, 0xFF, 0x03, 0x03, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x18, // 7
	0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 8
	0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 9
	0x7E, 0xFF, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xC3, // A
	0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, // B
	0x3C, 0xFF, 0xC3, 0xC0, 0xC0, 0xC0, 0xC0, 0xC3, 0xFF, 0x3C, // C
	0xFC, 0xFE, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFE, 0xFC, // D
	0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // E
	0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0  // F
};

#define ROM_START 0x200
#define FONT_START 0x00
#define S_FONT_START (0x00 + sizeof(fonts))

#define C_D_HEIGHT 32
#define C_D_WIDTH  64
#define S_D_HEIGHT 64
#define S_D_WIDTH  128
#define D_HEIGHT (chip8->hires ? S_D_HEIGHT : C_D_HEIGHT)
#define D_WIDTH  (chip8->hires ?  S_D_WIDTH : C_D_WIDTH)

typedef size_t (*keydown_fn_t)(char);

struct CHIP8 {
	uint8_t  memory[65535];
	uint8_t  display[S_D_HEIGHT * S_D_WIDTH];
	size_t   plane;
	size_t   PC;
	uint16_t I;
	uint16_t stack[4096];
	size_t   SC;
	uint8_t  delay_tmr;
	uint8_t  sound_tmr;
	uint8_t  vregs[16];
	uint8_t  fregs[16];
	bool     redraw;
	bool     halt;
	bool     hires;
	ssize_t  wait_key;

	keydown_fn_t keydown_fn;
};

struct CHIP8_inst {
	uint16_t   op;
	size_t op_len;
	uint8_t     P;
	uint8_t     X;
	uint8_t     Y;
	uint8_t     N;
	uint8_t    NN;
	uint16_t  NNN;
	uint16_t NNNN;
};

void chip8_init(struct CHIP8 *chip8, keydown_fn_t keydown);
void chip8_load(struct CHIP8 *chip8, char *data, size_t sz);
struct CHIP8_inst chip8_next(struct CHIP8 *chip8, size_t where);
void chip8_step(struct CHIP8 *chip8);

#endif
