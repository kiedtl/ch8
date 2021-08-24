#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include "chip8.h"

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

void
chip8_init(struct CHIP8 *chip8, keydown_fn_t keydown)
{
	srand(time(NULL));

	memset((void *)chip8->memory, 0x0, sizeof(chip8->memory));
	memset((void *)chip8->display, false, sizeof(chip8->display));
	chip8->PC = ROM_START;
	chip8->I = 0;
	memset((void *)chip8->stack, 0, sizeof(chip8->stack));
	chip8->SC = 0;
	chip8->delay_tmr = 0;
	chip8->sound_tmr = 0;
	memset((void *)chip8->vregs, 0, sizeof(chip8->vregs));
	chip8->redraw = false;
	chip8->wait_key = -1;
	chip8->keydown_fn = keydown;

	// set fonts
	memcpy((void *)&chip8->memory[FONT_START], (void *)&fonts, sizeof(fonts));
}

void
chip8_load(struct CHIP8 *chip8, char *data, size_t sz)
{
	memcpy(&chip8->memory[ROM_START], data, sz);
}

struct CHIP8_inst
chip8_next(struct CHIP8 *chip8, size_t where)
{
	struct CHIP8_inst inst;

	uint8_t op1 = chip8->memory[where];
	uint8_t op2 = chip8->memory[where + 1];

	inst.op = (op1 << 8) | op2;
	inst.P   = (inst.op >> 12);
	inst.X   = (inst.op >>  8) & 0xF;
	inst.Y   = (inst.op >>  4) & 0xF;
	inst.N   = (inst.op >>  0) & 0xF;
	inst.NN  = (inst.op >>  0) & 0xFF;
	inst.NNN = (inst.op >>  0) & 0xFFF;

	return inst;
}

void
chip8_step(struct CHIP8 *chip8)
{
	// Check if we're waiting for a keypress.
	if (chip8->wait_key != -1) {
		size_t pressed = 0;
		for (char key = 0; key < 15; ++key) {
			if ((chip8->keydown_fn)(key)) {
				pressed = key;
				break;
			}
		}

		if (pressed != 0) {
			// Key was pressed
			chip8->vregs[chip8->wait_key] = pressed;
			chip8->wait_key = -1;
		} else {
			return;
		}
	}

	struct CHIP8_inst inst = chip8_next(chip8, chip8->PC);
	uint8_t   op = inst.op;
	uint8_t    P = inst.P;
	uint8_t    X = inst.X;
	uint8_t    Y = inst.Y;
	uint8_t    N = inst.N;
	uint8_t   NN = inst.NN;
	uint16_t NNN = inst.NNN;

	chip8->PC += 2;

	switch (P) {
	break; case 0x0:
		switch (op) {
		break; case 0x00E0: // 00E0: CLS - reset display
			chip8->redraw = true;
			memset(chip8->display, 0x0, sizeof(chip8->display));
		break; case 0x00EE: // 00EE: RET - pop stack frame and return to that address
			// TODO: handle underflow
			chip8->PC = chip8->stack[chip8->SC];
			chip8->SC -= 1;
		};
	break; case 0x1: // 1NNN: JMP - Jump to NNN
		chip8->PC = NNN;
	break; case 0x2: // 2NNN: CAL - Push current PC, jump to NNN
		// TODO: handle overflow
		chip8->SC += 1;
		chip8->stack[chip8->SC] = chip8->PC;
		chip8->PC = NNN;
	break; case 0x3: // 3XNN: Skip next if X == NN
		chip8->PC += chip8->vregs[X] == NN ? 2 : 0;
	break; case 0x4: // 4XNN: Skip next if X != NN
		chip8->PC += chip8->vregs[X] != NN ? 2 : 0;
	break; case 0x5: // 5XY0: Skip next if X == Y
		chip8->PC += chip8->vregs[X] == chip8->vregs[Y] ? 2 : 0;
	break; case 0x6: // 6XNN: Set VX = NN
		chip8->vregs[X] = NN;
	break; case 0x7: // 7XNN: Add NN to VX
		chip8->vregs[X] += NN;
	break; case 0x8:
		switch (N) {
		break; case 0x0: // 8XY0: ? - VX = VY
			chip8->vregs[X] = chip8->vregs[Y];
		break; case 0x1: // 8XY1: ? - VX |= VY
			chip8->vregs[X] |= chip8->vregs[Y];
		break; case 0x2: // 8XY2: ? - VX &= VY
			chip8->vregs[X] &= chip8->vregs[Y];
		break; case 0x3: // 8XY3: ? - VX ^= VY
			chip8->vregs[X] ^= chip8->vregs[Y];
		break; case 0x4: // 8XY4: ? - VX += VY (VF == overflow?)
			;
			size_t result = chip8->vregs[X] + chip8->vregs[Y];
			if (result > 255) chip8->vregs[15] = 1;
			chip8->vregs[X] = (char)(result & 0xFF);
		break; case 0x5: // 8XY5: ? - VX -= VY
			chip8->vregs[X] -= chip8->vregs[Y];
		break; case 0x6: // 8X06: ? - VX >>= 1, VF = LSB
			chip8->vregs[15] = chip8->vregs[15] & 1;
			chip8->vregs[X] >>= 1;
		break; case 0x7: // 8XY7: ? - VY -= VX
			chip8->vregs[X] = chip8->vregs[Y] - chip8->vregs[X];
		break; case 0xE: // 8X0E: ? - VX <<= 1, VF = MSB
			chip8->vregs[15] = (chip8->vregs[15] & 0x80) != 0;
			chip8->vregs[X] <<= 1;
		}
	break; case 0x9: // 9XY0: Skip next if X != Y
		chip8->PC += chip8->vregs[X] != chip8->vregs[Y] ? 2 : 0;
	break; case 0xA: // ANNN: Set I to NNN
		chip8->I = NNN;
	break; case 0xB: // BNNN: Jump to V0 + NNN
		// TODO: configurable BXNN behaviour (jump to XNN + VX)?
		chip8->PC = (chip8->vregs[0] + NNN) & 0xFFF;
	break; case 0xC: // CXNN: VX = rand() & NN
		chip8->vregs[X] = rand() & NN;
	break; case 0xD: // DXYN: Draw a sprite with a height of N at coord VX,VY
		chip8->redraw = true;
		size_t x = chip8->vregs[X] & (D_WIDTH-1);
		size_t y = chip8->vregs[Y] & (D_HEIGHT-1);

		chip8->vregs[15] = 0;
		for (int j = 0; j < N; ++j) {
			if ((y + j) >= D_HEIGHT) continue;

			uint8_t sprite_row = chip8->memory[chip8->I + j];

			for (int i = 0; i < 8; ++i) {
				size_t ix = (x + i) & (D_WIDTH-1);
				size_t iy = (y + j) & (D_HEIGHT-1);
				size_t pos = (D_WIDTH * iy) + ix;

				if (ix >= D_WIDTH) break;

				size_t pixel = (sprite_row & (1 << (7 - i))) != 0;
				chip8->vregs[15] |= chip8->display[pos] & pixel;
				chip8->display[pos] ^= pixel;
			}
		}
	break; case 0xE:
		;
		uint8_t key = chip8->vregs[X] & 0xF;
		switch (NN) {
		break; case 0x9E: // EX9E: Skip next if key VX is pressed
			if ((chip8->keydown_fn)(key)) chip8->PC += 2;
		break; case 0xA1: // EXA1: Skip next if key VX is not pressed
			if (!(chip8->keydown_fn)(key)) chip8->PC += 2;
		}
	break; case 0xF:
		switch (NN) {
		break; case 0x07: // FX07: Set VX value of delay timer
			chip8->vregs[X] = chip8->delay_tmr;
		break; case 0x15: // FX15: Set delay timer to VX
			chip8->delay_tmr = chip8->vregs[X];
		break; case 0x18: // FX18: Set sound timer to VX
			chip8->sound_tmr = chip8->vregs[X];
		break; case 0x29: // FX29: Set I to FONT_START + VX
			chip8->I = FONT_START + (chip8->vregs[X] & 0xF) * 5;
		break; case 0x1E: // FX1E: Add VX to I
			// TODO: configurable behaviour to set VF to 1 if I overflows
			//       (Apparently "Spacefight 2091!" relies on this)
			chip8->I += chip8->vregs[X];
		break; case 0x0A: // FX0A: Wait until a key is pressed, then store in VX
			chip8->wait_key = X;
		break; case 0x33: // FX33: Convert VX to 3-char string and place at I[0..2]
			;
			uint8_t value = chip8->vregs[X];
			chip8->memory[chip8->I + 0] = value / 100;
			chip8->memory[chip8->I + 1] = (value / 10) % 10;
			chip8->memory[chip8->I + 2] = value % 10;
		break; case 0x55: // FX55: Load registers V0..=VX into memory[I..]
			// TODO: configurable behaviour to increment I with r
			for (size_t r = 0; r <= X; ++r)
				chip8->memory[chip8->I + r] = chip8->vregs[r];
		break; case 0x65: // FX65: Load memory[I..] into registers V0..=VX
			// TODO: configurable behaviour to increment I with r
			for (size_t r = 0; r <= X; ++r)
				chip8->vregs[r] = chip8->memory[chip8->I + r];
		break; default:
		break;
		}
	break; default:
	break;
	};

}
