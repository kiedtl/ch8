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

	switch (inst.P) {
	break; case 0x0:
		switch (inst.Y) {
		break; case 0xC: // 00CN: (SCHIP) Scroll down by N pixels.
			inst.type = I_00CN;
		break; case 0xD: // 00DN: (XO) Scroll up by N pixels.
			inst.type = I_00DN;
		break; default:
			switch (inst.op) {
			break; case 0x00E0: // 00E0: CLS - reset display
				inst.type = I_00E0;
			break; case 0x00EE: // 00EE: RET - pop stack frame and return to that address
				inst.type = I_00EE;
			break; case 0x00FB: // 00FB: Scroll right by 4 pixels.
				inst.type = I_00FB;
			break; case 0x00FC: // 00FC: Scroll left by 4 pixels.
				inst.type = I_00FC;
			break; case 0x00FD: // 00FD: Halt.
				inst.type = I_00FD;
			break; case 0x00FE: // 00FE: Disable hi-res mode.
				inst.type = I_00FE;
			break; case 0x00FF: // 00FF: Enable hi-res mode.
				inst.type = I_00FF;
			break; default:
				inst.type = I_UNKNOWN;
			break;
			};
		}
	break; case 0x1: // 1NNN: JMP - Jump to NNN
		inst.type = I_1NNN;
	break; case 0x2: // 2NNN: CAL - Push current PC, jump to NNN
		inst.type = I_2NNN;
	break; case 0x3: // 3XNN: Skip next if X == NN
		inst.type = I_3XNN;
	break; case 0x4: // 4XNN: Skip next if X != NN
		inst.type = I_4XNN;
	break; case 0x5:
		switch (inst.N) {
		break; case 0: // 5XY0: Skip next if X == Y
			inst.type = I_5XY0;
		break; case 2: // 5XY2: (XO) Save registers VX..=VY to memory starting at I.
			inst.type = I_5XY2;
		break; case 3: // 5XY3: Load registers VX..=VY from memory starting at I.
			inst.type = I_5XY3;
		break; default:
			inst.type = I_UNKNOWN;
		break;
		}
	break; case 0x6: // 6XNN: Set VX = NN
		inst.type = I_6XNN;
	break; case 0x7: // 7XNN: Add NN to VX
		inst.type = I_7XNN;
	break; case 0x8:
		switch (inst.N) {
		break; case 0x0: // 8XY0: VX = VY
			inst.type = I_8XY0;
		break; case 0x1: // 8XY1: VX |= VY
			inst.type = I_8XY1;
		break; case 0x2: // 8XY2: VX &= VY
			inst.type = I_8XY2;
		break; case 0x3: // 8XY3: VX ^= VY
			inst.type = I_8XY3;
		break; case 0x4: // 8XY4: VX += VY (VF == overflow?)
			inst.type = I_8XY4;
		break; case 0x5: // 8XY5: VX -= VY
			inst.type = I_8XY5;
		break; case 0x6: // 8X06: VX >>= 1, VF = LSB
			inst.type = I_8X06;
		break; case 0x7: // 8XY7: VX = VY - VX
			inst.type = I_8XY7;
		break; case 0xE: // 8X0E: VX <<= 1, VF = MSB
			inst.type = I_8X0E;
		break; default:
			inst.type = I_UNKNOWN;
		break;
		}
	break; case 0x9: // 9XY0: Skip next if X != Y
		inst.type = I_9XY0;
	break; case 0xA: // ANNN: Set I to NNN
		inst.type = I_ANNN;
	break; case 0xB: // BNNN: Jump to V0 + NNN
		inst.type = I_BNNN;
	break; case 0xC: // CXNN: VX = rand() & NN
		inst.type = I_CXNN;
	break; case 0xD: // DXYN: Draw a sprite with a height of N at coord VX,VY
		inst.type = I_DXYN;
	break; case 0xE:
		switch (inst.NN) {
		break; case 0x9E: // EX9E: Skip next if key VX is pressed
			inst.type = I_EX9E;
		break; case 0xA1: // EXA1: Skip next if key VX is not pressed
			inst.type = I_EXA1;
		break; default:
			inst.type = I_UNKNOWN;
		break;
		}
	break; case 0xF:
		switch (inst.NN) {
		break; case 0x00: // F000: (XO) Load I with the next word.
			inst.type = I_F000;
		break; case 0x01: // FX01: (XO) Select X planes
			inst.type = I_FX01;
		break; case 0x02: // F002: Store 16 bytes starting at I in the audio pattern buffer.
			inst.type = I_F002;
		break; case 0x07: // FX07: Set VX value of delay timer
			inst.type = I_FX07;
		break; case 0x15: // FX15: Set delay timer to VX
			inst.type = I_FX15;
		break; case 0x18: // FX18: Set sound timer to VX
			inst.type = I_FX18;
		break; case 0x29: // FX29: Set I to FONT_START + VX
			inst.type = I_FX29;
		break; case 0x30: // FX30: Set I to S_FONT_START + VX
			inst.type = I_FX30;
		break; case 0x1E: // FX1E: Add VX to I
			inst.type = I_FX1E;
		break; case 0x0A: // FX0A: Wait until a key is pressed, then store in VX
			inst.type = I_FX0A;
		break; case 0x33: // FX33: Convert VX to 3-char string and place at I[0..2]
			inst.type = I_FX33;
		break; case 0x55: // FX55: Load registers V0..=VX into memory[I..]
			inst.type = I_FX55;
		break; case 0x65: // FX65: Load memory[I..] into registers V0..=VX
			inst.type = I_FX65;
		break; case 0x75: // FX75: Save registers V0..=VX into flag registers
			inst.type = I_FX75;
		break; case 0x85: // FX85: Load registers V0..=VX from flag registers
			inst.type = I_FX85;
		break; default:
			inst.type = I_UNKNOWN;
		break;
		}
	break; default:
		inst.type = I_UNKNOWN;
	break;
	};

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
	uint8_t     X = inst.X;
	uint8_t     Y = inst.Y;
	uint8_t     N = inst.N;
	uint8_t    NN = inst.NN;
	uint16_t  NNN = inst.NNN;
	uint16_t NNNN = inst.NNNN;

	chip8->PC += inst.op_len;

	bool set_vf = false;

	switch (inst.type) {
	break; case I_00CN:
			for (ssize_t y = D_HEIGHT - 1; y >= 0; --y) {
				for (ssize_t x = 0; x < D_WIDTH; ++x) {
					chip8_mvpx(chip8, x, y, x, y - N);
				}
			}
	break; case I_00DN:
			for (ssize_t y = 0; y < D_HEIGHT; ++y) {
				for (size_t x = 0; x < D_WIDTH; ++x) {
					chip8_mvpx(chip8, x, y, x, y + N);
				}
			}
	break; case I_00E0:
				chip8->redraw = true;
				for (size_t i = 0; i < sizeof(chip8->display); ++i)
					chip8->display[i] &= ~chip8->plane;
	break; case I_00EE:
				// TODO: handle underflow
				chip8->SC -= 1;
				chip8->PC = chip8->stack[chip8->SC];
				chip8->stack[chip8->SC] = 0;
	break; case I_00FB:
				for (ssize_t y = 0; y < D_HEIGHT; ++y)
					for (ssize_t x = D_WIDTH - 1; x >= 0; --x)
						chip8_mvpx(chip8, x, y, x - 4, y);
	break; case I_00FC:
				for (ssize_t y = 0; y < D_HEIGHT; ++y)
					for (ssize_t x = 0; x < D_WIDTH; ++x)
						chip8_mvpx(chip8, x, y, x + 4, y);
	break; case I_00FD:
				chip8->halt = true;
	break; case I_00FE:
				chip8->hires = false;
				memset(chip8->display, 0x0, sizeof(chip8->display));
	break; case I_00FF:
				chip8->hires = true;
				memset(chip8->display, 0x0, sizeof(chip8->display));
	break; case I_1NNN:
		chip8->PC = NNN;
	break; case I_2NNN:
		// TODO: handle overflow
		chip8->stack[chip8->SC] = chip8->PC;
		chip8->SC += 1;
		chip8->PC = NNN;
	break; case I_3XNN:
	if (chip8->vregs[X] == NN) chip8->PC += chip8_next(chip8, chip8->PC).op_len;
	break; case I_4XNN:
		if (chip8->vregs[X] != NN) chip8->PC += chip8_next(chip8, chip8->PC).op_len;
	break; case I_5XY0:
			if (chip8->vregs[X] == chip8->vregs[Y])
				chip8->PC += chip8_next(chip8, chip8->PC).op_len;
	break; case I_5XY2:
			for (size_t i = 0; i <= abs((ssize_t)X - (ssize_t)Y); ++i) {
				size_t r = X < Y ? X + i : X - i;
				chip8->memory[chip8->I + i] = chip8->vregs[r];
			}
	break; case I_5XY3:
			for (size_t i = 0; i <= abs((ssize_t)X - (ssize_t)Y); ++i) {
				size_t r = X < Y ? X + i : X - i;
				chip8->vregs[r] = chip8->memory[chip8->I + i];
			}
	break; case I_6XNN:
		chip8->vregs[X] = NN;
	break; case I_7XNN:
		chip8->vregs[X] += NN;
	break; case I_8XY0:
			chip8->vregs[X] = chip8->vregs[Y];
	break; case I_8XY1:
			chip8->vregs[X] |= chip8->vregs[Y];
	break; case I_8XY2:
			chip8->vregs[X] &= chip8->vregs[Y];
	break; case I_8XY3:
			chip8->vregs[X] ^= chip8->vregs[Y];
	break; case I_8XY4:
			;
			size_t result = chip8->vregs[X] + chip8->vregs[Y];
			if (result > 255) chip8->vregs[15] = 1;
			chip8->vregs[X] = (char)(result & 0xFF);
	break; case I_8XY5:
			set_vf = chip8->vregs[X] >= chip8->vregs[Y];
			chip8->vregs[X] -= chip8->vregs[Y];
			chip8->vregs[15] = (size_t)set_vf;
	break; case I_8X06:
			// TODO: option to shift VX into VY
			chip8->vregs[15] = chip8->vregs[X] & 1;
			chip8->vregs[X] >>= 1;
	break; case I_8XY7:
			set_vf = chip8->vregs[Y] >= chip8->vregs[X];
			chip8->vregs[X] = chip8->vregs[Y] - chip8->vregs[X];
			chip8->vregs[15] = (size_t)set_vf;
	break; case I_8X0E:
			// TODO: option to shift VX into VY
			chip8->vregs[15] = (chip8->vregs[X] & 0x80) != 0;
			chip8->vregs[X] <<= 1;
	break; case I_9XY0:
		if (chip8->vregs[X] != chip8->vregs[Y])
			chip8->PC += chip8_next(chip8, chip8->PC).op_len;
	break; case I_ANNN:
		chip8->I = NNN;
	break; case I_BNNN:
		// TODO: configurable BXNN behaviour (jump to XNN + VX)?
		//chip8->PC = NNN + chip8->vregs[0];
		chip8->PC = NNN + chip8->vregs[X];
	break; case I_CXNN:
		chip8->vregs[X] = rand() & NN;
	break; case I_DXYN:
		chip8->redraw = true;
		size_t coord_x = chip8->vregs[X] & (D_WIDTH-1);
		size_t coord_y = chip8->vregs[Y] & (D_HEIGHT-1);
		chip8->vregs[15] = 0;

		size_t i = chip8->I;
		size_t xd = N == 0 ? 16 : 8;
		size_t yd = N == 0 ? 16 : N;

		for (size_t color = 1; color <= 2; ++color) {
			if ((chip8->plane & color) == 0) continue;

			for (size_t y = 0; y < yd; ++y) for (size_t x = 0; x < xd; ++x) {
				size_t p = N == 0 ? (chip8->memory[i + (2 * y) + (x > 7 ? 1 : 0)] >> (7 - (x % 8)))
				                  : (chip8->memory[i +      y                   ] >> (7 - x      ));
				p &= 1;

				size_t pos_x = (x + coord_x) & (D_WIDTH-1);
				size_t pos_y = (y + coord_y) & (D_HEIGHT-1);
				size_t pos = (D_WIDTH * pos_y) + pos_x;

				if (p) {
					uint8_t *pixel = &chip8->display[pos];
					if ((color & *pixel) == 0) { // set
						*pixel |= color;
					} else { // clear
						*pixel &= ~color;
						chip8->vregs[15] = 1;
					}
				}
				
			}
			i += N == 0 ? 32 : N;
		}
	break; case I_EX9E: {
		uint8_t key = chip8->vregs[X] & 0xF;
			if ((chip8->keydown_fn)(key)) chip8->PC += 2;
	} break; case I_EXA1: {
		uint8_t key = chip8->vregs[X] & 0xF;
			if (!(chip8->keydown_fn)(key)) chip8->PC += 2;
	} break; case I_F000:
			chip8->I = NNNN;
	break; case I_FX01:
			chip8->plane = X & 3;
	break; case I_F002:
			// TODO: audio
	break; case I_FX07:
			chip8->vregs[X] = chip8->delay_tmr;
	break; case I_FX15:
			chip8->delay_tmr = chip8->vregs[X];
	break; case I_FX18:
			chip8->sound_tmr = chip8->vregs[X];
	break; case I_FX29:
			chip8->I = FONT_START + (chip8->vregs[X] & 0xF) * 5;
	break; case I_FX30:
			chip8->I = S_FONT_START + (chip8->vregs[X] & 0xF) * 10;
	break; case I_FX1E:
			// TODO: configurable behaviour to set VF to 1 if I overflows
			//       (Apparently "Spacefight 2091!" relies on this)
			chip8->I += chip8->vregs[X];
	break; case I_FX0A:
			chip8->wait_key = X;
	break; case I_FX33:
			;
			uint8_t value = chip8->vregs[X];
			chip8->memory[chip8->I + 0] = value / 100;
			chip8->memory[chip8->I + 1] = (value / 10) % 10;
			chip8->memory[chip8->I + 2] = value % 10;
	break; case I_FX55:
			// TODO: configurable behaviour to increment I with r
			for (size_t r = 0; r <= X; ++r)
				chip8->memory[chip8->I + r] = chip8->vregs[r];
	break; case I_FX65:
			// TODO: configurable behaviour to increment I with r
			for (size_t r = 0; r <= X; ++r)
				chip8->vregs[r] = chip8->memory[chip8->I + r];
	break; case I_FX75:
			for (size_t r = 0; r <= X; ++r)
				chip8->fregs[r] = chip8->vregs[r];
	break; case I_FX85:
			for (size_t r = 0; r <= X; ++r)
				chip8->vregs[r] = chip8->fregs[r];
	break; case I_UNKNOWN:
		log("Unknown opcode %04X", inst.op);
		chip8->halt = true;
	break; default:
		log("Internal error: chip8_next returned unknown opcode %04X", inst.op);
		chip8->halt = true;
	break;
	};

}
