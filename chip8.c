#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include "chip8.h"
#include "util.h"

void
chip8_init(struct CHIP8 *chip8, keydown_fn_t keydown)
{
	srand(time(NULL));

	memset((void *)chip8->memory, 0x0, sizeof(chip8->memory));
	memset((void *)chip8->display, false, sizeof(chip8->display));
	chip8->plane = 1;
	chip8->PC = ROM_START;
	chip8->I = 0;
	memset((void *)chip8->stack, 0, sizeof(chip8->stack));
	chip8->SC = 0;
	chip8->delay_tmr = 0;
	chip8->sound_tmr = 0;
	memset((void *)chip8->vregs, 0, sizeof(chip8->vregs));
	chip8->redraw = false;
	chip8->halt = false;
	chip8->hires = false;
	chip8->wait_key = -1;
	chip8->keydown_fn = keydown;

	// set fonts
	memcpy((void *)&chip8->memory[FONT_START], (void *)&fonts, sizeof(fonts));
	memcpy((void *)&chip8->memory[S_FONT_START], (void *)&s_fonts, sizeof(s_fonts));
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
	uint8_t op3 = (where + 2) < SIZEOF(chip8->memory) ? chip8->memory[where + 2] : 0;
	uint8_t op4 = (where + 3) < SIZEOF(chip8->memory) ? chip8->memory[where + 3] : 0;

	inst.op     = (op1 << 8) | op2;
	inst.op_len = inst.op == 0xF000 ? 4 : 2;
	inst.P      = (inst.op >> 12);
	inst.X      = (inst.op >>  8) & 0xF;
	inst.Y      = (inst.op >>  4) & 0xF;
	inst.N      = (inst.op >>  0) & 0xF;
	inst.NN     = (inst.op >>  0) & 0xFF;
	inst.NNN    = (inst.op >>  0) & 0xFFF;
	inst.NNNN   = (op3 << 8) | op4;

	return inst;
}

// Stolen from:
// https://github.com/JohnEarnest/c-octo, src/octo_emulator.h, octo_emulator_move_pix()
//
static void
chip8_mvpx(struct CHIP8 *chip8, ssize_t dx, ssize_t dy, ssize_t sx, ssize_t sy)
{
	for (size_t plane = 1; plane <= 2; ++plane) {
		if (chip8->plane & plane) {
			uint8_t *d = &chip8->display[(dy * D_WIDTH) + dx];
			uint8_t *s = &chip8->display[(sy * D_WIDTH) + sx];
			uint8_t color = (sx < 0 || sy < 0 || sx >= D_WIDTH || sy >= D_HEIGHT)
				? 0 : plane & *s;
			*d &= ~plane; // Remove old pixel
			*d |= color;  // Set new pixel
		}
	}
}

void
chip8_step(struct CHIP8 *chip8)
{
	if (chip8->halt || chip8->wait_key != -1) {
		return;
	}

	struct CHIP8_inst inst = chip8_next(chip8, chip8->PC);
	uint8_t    op = inst.op;
	uint8_t     P = inst.P;
	uint8_t     X = inst.X;
	uint8_t     Y = inst.Y;
	uint8_t     N = inst.N;
	uint8_t    NN = inst.NN;
	uint16_t  NNN = inst.NNN;
	uint16_t NNNN = inst.NNNN;

	chip8->PC += inst.op_len;

	bool set_vf = false;

	switch (P) {
	break; case 0x0:
		if (Y == 0xC) { // 00CN: (SCHIP) Scroll down by N pixels.
			for (ssize_t y = D_HEIGHT - 1; y >= 0; --y) {
				for (ssize_t x = 0; x < D_WIDTH; ++x) {
					chip8_mvpx(chip8, x, y, x, y - N);
				}
			}
		} else if (Y == 0xD) { // 00DN: (XO) Scroll up by N pixels.
			for (ssize_t y = 0; y < D_HEIGHT; ++y) {
				for (size_t x = 0; x < D_WIDTH; ++x) {
					chip8_mvpx(chip8, x, y, x, y + N);
				}
			}
		} else {
			switch (op) {
			break; case 0x00E0: // 00E0: CLS - reset display
				chip8->redraw = true;
				for (size_t i = 0; i < sizeof(chip8->display); ++i)
					chip8->display[i] &= ~chip8->plane;
			break; case 0x00EE: // 00EE: RET - pop stack frame and return to that address
				// TODO: handle underflow
				chip8->SC -= 1;
				chip8->PC = chip8->stack[chip8->SC];
				chip8->stack[chip8->SC] = 0;
			break; case 0x00FB: // 00FB: Scroll right by 4 pixels.
				for (ssize_t y = 0; y < D_HEIGHT; ++y)
					for (ssize_t x = D_WIDTH - 1; x >= 0; --x)
						chip8_mvpx(chip8, x, y, x - 4, y);
			break; case 0x00FC: // 00FC: Scroll left by 4 pixels.
				for (ssize_t y = 0; y < D_HEIGHT; ++y)
					for (ssize_t x = 0; x < D_WIDTH; ++x)
						chip8_mvpx(chip8, x, y, x + 4, y);
			break; case 0x00FD: // 00FD: Halt.
				chip8->halt = true;
			break; case 0x00FE: // 00FE: Disable hi-res mode.
				chip8->hires = false;
				memset(chip8->display, 0x0, sizeof(chip8->display));
			break; case 0x00FF: // 00FF: Enable hi-res mode.
				chip8->hires = true;
				memset(chip8->display, 0x0, sizeof(chip8->display));
			};
		}
	break; case 0x1: // 1NNN: JMP - Jump to NNN
		chip8->PC = NNN;
	break; case 0x2: // 2NNN: CAL - Push current PC, jump to NNN
		// TODO: handle overflow
		chip8->stack[chip8->SC] = chip8->PC;
		chip8->SC += 1;
		chip8->PC = NNN;
	break; case 0x3: // 3XNN: Skip next if X == NN
		if (chip8->vregs[X] == NN) chip8->PC += chip8_next(chip8, chip8->PC).op_len;
	break; case 0x4: // 4XNN: Skip next if X != NN
		if (chip8->vregs[X] != NN) chip8->PC += chip8_next(chip8, chip8->PC).op_len;
	break; case 0x5:
		switch (N) {
		break; case 0: // 5XY0: Skip next if X == Y
			if (chip8->vregs[X] == chip8->vregs[Y])
				chip8->PC += chip8_next(chip8, chip8->PC).op_len;
		break; case 2: // 5XY2: (XO) Save registers VX..=VY to memory starting at I.
			for (size_t i = 0; i <= abs((ssize_t)X - (ssize_t)Y); ++i) {
				size_t r = X < Y ? X + i : X - i;
				chip8->memory[chip8->I + i] = chip8->vregs[r];
			}
		break; case 3: // 5XY3: Load registers VX..=VY from memory starting at I.
			for (size_t i = 0; i <= abs((ssize_t)X - (ssize_t)Y); ++i) {
				size_t r = X < Y ? X + i : X - i;
				chip8->vregs[r] = chip8->memory[chip8->I + i];
			}
		break; default:
		break;
		}
	break; case 0x6: // 6XNN: Set VX = NN
		chip8->vregs[X] = NN;
	break; case 0x7: // 7XNN: Add NN to VX
		chip8->vregs[X] += NN;
	break; case 0x8:
		switch (N) {
		break; case 0x0: // 8XY0: VX = VY
			chip8->vregs[X] = chip8->vregs[Y];
		break; case 0x1: // 8XY1: VX |= VY
			chip8->vregs[X] |= chip8->vregs[Y];
		break; case 0x2: // 8XY2: VX &= VY
			chip8->vregs[X] &= chip8->vregs[Y];
		break; case 0x3: // 8XY3: VX ^= VY
			chip8->vregs[X] ^= chip8->vregs[Y];
		break; case 0x4: // 8XY4: VX += VY (VF == overflow?)
			;
			size_t result = chip8->vregs[X] + chip8->vregs[Y];
			if (result > 255) chip8->vregs[15] = 1;
			chip8->vregs[X] = (char)(result & 0xFF);
		break; case 0x5: // 8XY5: VX -= VY
			set_vf = chip8->vregs[X] >= chip8->vregs[Y];
			chip8->vregs[X] -= chip8->vregs[Y];
			chip8->vregs[15] = (size_t)set_vf;
		break; case 0x6: // 8X06: VX >>= 1, VF = LSB
			// TODO: option to shift VX into VY
			chip8->vregs[15] = chip8->vregs[X] & 1;
			chip8->vregs[X] >>= 1;
		break; case 0x7: // 8XY7: VX = VY - VX
			set_vf = chip8->vregs[Y] >= chip8->vregs[X];
			chip8->vregs[X] = chip8->vregs[Y] - chip8->vregs[X];
			chip8->vregs[15] = (size_t)set_vf;
		break; case 0xE: // 8X0E: VX <<= 1, VF = MSB
			// TODO: option to shift VX into VY
			chip8->vregs[15] = (chip8->vregs[X] & 0x80) != 0;
			chip8->vregs[X] <<= 1;
		}
	break; case 0x9: // 9XY0: Skip next if X != Y
		if (chip8->vregs[X] != chip8->vregs[Y])
			chip8->PC += chip8_next(chip8, chip8->PC).op_len;
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

		if (chip8->plane != 0 && chip8->hires && N == 0) {
			for (int j = 0; j < 16; ++j) {
				if ((y + j) >= D_HEIGHT) continue;

				uint8_t sprite_row_hi = chip8->memory[chip8->I + 2 * j + 0];
				uint8_t sprite_row_lo = chip8->memory[chip8->I + 2 * j + 1];
				uint16_t sprite_row = sprite_row_hi << 8 | sprite_row_lo;

				for (size_t i = 0; i < 16; ++i) {
					size_t ix = (x + i) & (D_WIDTH-1);
					size_t iy = (y + j) & (D_HEIGHT-1);
					if (ix >= D_WIDTH) break;

					size_t pos = (D_WIDTH * iy) + ix;
					size_t sprite_pixel = (sprite_row & (1 << (15 - i))) != 0;

					if (sprite_pixel != 0) {
						uint8_t *pixel = &chip8->display[pos];
						if ((chip8->plane & *pixel) == 0) { // set
							*pixel |= chip8->plane;
						} else { // clear
							*pixel &= ~chip8->plane;
							chip8->vregs[15] = 1;
						}
					}
				}
			}
		} else if (chip8->plane != 0) {
			for (int j = 0; j < N; ++j) {
				if ((y + j) >= D_HEIGHT) continue;

				uint8_t sprite_row = chip8->memory[chip8->I + j];

				for (int i = 0; i < 8; ++i) {
					size_t ix = (x + i) & (D_WIDTH-1);
					size_t iy = (y + j) & (D_HEIGHT-1);
					if (ix >= D_WIDTH) break;

					size_t pos = (D_WIDTH * iy) + ix;
					size_t sprite_pixel = (sprite_row & (1 << (7 - i))) != 0;

					if (sprite_pixel != 0) {
						uint8_t *pixel = &chip8->display[pos];
						if ((chip8->plane & *pixel) == 0) { // set
							*pixel |= chip8->plane;
						} else { // clear
							*pixel &= ~chip8->plane;
							chip8->vregs[15] = 1;
						}
					}
				}
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
		break; case 0x00: // F000: (XO) Load I with the next word.
			chip8->I = NNNN;
		break; case 0x01: // FX01: (XO) Select X planes
			chip8->plane = X & 3;
		break; case 0x02: // F002: Store 16 bytes starting at I in the audio pattern buffer.
			// TODO
		break; case 0x07: // FX07: Set VX value of delay timer
			chip8->vregs[X] = chip8->delay_tmr;
		break; case 0x15: // FX15: Set delay timer to VX
			chip8->delay_tmr = chip8->vregs[X];
		break; case 0x18: // FX18: Set sound timer to VX
			chip8->sound_tmr = chip8->vregs[X];
		break; case 0x29: // FX29: Set I to FONT_START + VX
			chip8->I = FONT_START + (chip8->vregs[X] & 0xF) * 5;
		break; case 0x30: // FX30: Set I to S_FONT_START + VX
			chip8->I = S_FONT_START + (chip8->vregs[X] & 0xF) * 10;
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
		break; case 0x75: // FX75: Save registers V0..=VX into flag registers
			for (size_t r = 0; r <= X; ++r)
				chip8->fregs[r] = chip8->vregs[r];
		break; case 0x85: // FX85: Load registers V0..=VX from flag registers
			for (size_t r = 0; r <= X; ++r)
				chip8->vregs[r] = chip8->fregs[r];
		break; default:
		break;
		}
	break; default:
		log("Unknown opcode %04X", op);
	break;
	};

}
