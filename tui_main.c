// Thanks:
//    - https://tobiasvl.github.io/blog/write-a-chip-8-emulator/

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "chip8.h"
#include "termbox.h"
#include "util.h"

#define log(fmt, ...) fprintf(stderr, fmt"\n", __VA_ARGS__)

/*
 * keep track of termbox's state, so that we know if
 * tb_shutdown() is safe to call, and whether we should redraw.
 *
 * (Calling tb_shutdown twice, or before tb_init, results in
 * a call to abort().)
 */
static const size_t TB_ACTIVE   = (1<<1);
static size_t tb_status = 0;

static size_t ui_height, ui_width;
static bool ui_buzzer = false;

static void
init_gui(void)
{
	char *errstrs[] = {
		NULL,
		"termbox: unsupported terminal",
		"termbox: cannot open terminal",
		"termbox: pipe trap error"
	};
	char *err = errstrs[-(tb_init())];
	if (err) die(err);

	tb_status |= TB_ACTIVE;
	tb_select_input_mode(TB_INPUT_ALT|TB_INPUT_MOUSE);
	tb_select_output_mode(TB_OUTPUT_TRUECOLOR);
}

static void
draw(struct CHIP8 *chip8)
{
	size_t ty = 0;
	if (ui_buzzer) {
		for (size_t x = 0; x < D_WIDTH; ++x) tb_change_cell(x, ty, ' ', 0, 0xfa8072);
	} else {
		for (size_t x = 0; x < D_WIDTH; ++x) tb_change_cell(x, ty, ' ', 0, 0);
	}
	ty += 2;

	for (size_t y = 0; y < D_HEIGHT; y += 2, ++ty) {
		for (size_t x = 0; x < D_WIDTH; ++x) {
			uint32_t fg = chip8->display[D_WIDTH * (y+0) + x] ? 0 : 0xffffff;
			uint32_t bg = chip8->display[D_WIDTH * (y+1) + x] ? 0 : 0xffffff;
			tb_change_cell(x, ty, 0x2584, fg, bg);
		}
	}

	tb_present();
}

static void
init_cpu(struct CHIP8 *chip8)
{
	srand(time(NULL));

	memset((void *)chip8->memory, 0x0, sizeof(chip8->memory));
	memset((void *)chip8->display, false, sizeof(chip8->display));
	chip8->PC = 0;
	chip8->I = 0;
	memset((void *)chip8->stack, 0, sizeof(chip8->stack));
	chip8->SC = 0;
	chip8->delay_tmr = 0;
	chip8->sound_tmr = 0;
	memset((void *)chip8->vregs, 0, sizeof(chip8->vregs));
	chip8->redraw = false;
	chip8->wait_key = -1;

	// set fonts
	memcpy((void *)&chip8->memory[FONT_START], (void *)&fonts, sizeof(fonts));
}

static void
load(struct CHIP8 *chip8, char *filename)
{
	struct stat st;
	stat(filename, &st); // TODO: assert that retcode != 0

	char *src = ecalloc(st.st_size, sizeof(char));
	FILE *src_f = fopen(filename, "rb");
	fread(src, sizeof(char), st.st_size, src_f);
	fclose(src_f);

	memcpy(chip8->memory + ROM_START, src, st.st_size);

	free(src);
}

static size_t
keydown(char key)
{
	const size_t keys[255] = {
		['X'] = 0x0, ['1'] = 0x1, ['2'] = 0x2, ['3'] = 0x3,
		['Q'] = 0x4, ['W'] = 0x5, ['E'] = 0x6, ['A'] = 0x7,
		['S'] = 0x8, ['D'] = 0x9, ['Z'] = 0xA, ['C'] = 0xB,
		['4'] = 0xC, ['R'] = 0xD, ['F'] = 0xE, ['V'] = 0xF,
	};

	if (key < 0 || key > 15) return 0;

	struct tb_event ev;
	ssize_t ret = 0;

	if ((ret = tb_peek_event(&ev, 8)) == 0)
		return 0;
        ENSURE(ret != -1); /* termbox error */

	if (ev.type == TB_EVENT_KEY && ev.ch) {
		return keys[MAX(ev.ch, sizeof(keys))];
	}

	return 0;
}

static void
step(struct CHIP8 *chip8)
{
	// Check if we're waiting for a keypress.
	if (chip8->wait_key != -1) {
		for (size_t k = 0; k < 16; ++k) {
			ssize_t status = keydown(k);
			if (status) {
				// Key was pressed
				chip8->vregs[chip8->wait_key] = k;
				chip8->wait_key = -1;
				break;
			}
		}

		// If we haven't reset chip8->wait_key, a key hasn't been
		// pressed. Return to try again later.
		if (chip8->wait_key != -1)
			return;
	}

	uint8_t op1 = chip8->memory[chip8->PC];
	uint8_t op2 = chip8->memory[chip8->PC + 1];
	uint16_t op = (op1 << 8) | op2;

	uint8_t    P = (op >> 12);
	uint8_t    X = (op >>  8) & 0xF;
	uint8_t    Y = (op >>  4) & 0xF;
	uint8_t    N = (op >>  0) & 0xF;
	uint8_t   NN = (op >>  0) & 0xFF;
	uint16_t NNN = (op >>  0) & 0xFFF;

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
			chip8->vregs[X] = result;
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
	break; case 0xD: // DXYN: Draw a sprite at coord VX,VY
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
			if (keydown(key)) chip8->PC += 2;
		break; case 0xA1: // EXA1: Skip next if key VX is not pressed
			if (!keydown(key)) chip8->PC += 2;
		}
	break; case 0xF:
		switch (NN) {
		break; case 0x07: // FX07: Set VX value of delay timer
			chip8->vregs[X] = chip8->delay_tmr;
		break; case 0x15: // FX15: Set delay timer to VX
			chip8->delay_tmr = chip8->vregs[X];
		break; case 0x18: // FX18: Set sound timer to VX
			chip8->sound_tmr = chip8->vregs[X];
		break; case 0x29: // FX29: Set I to FONT_START
			chip8->I = FONT_START;
		break; case 0x1E: // FX1E: Add VX to I
			// TODO: configurable behaviour to set VF to 1 if I overflows
			//       (Apparently "Spacefight 2091!" relies on this)
			chip8->I += chip8->vregs[X];
		break; case 0x0A: // FX0A: Wait until key VX is pressed
			chip8->wait_key = chip8->vregs[X] & 0xF;
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

static void
exec(struct CHIP8 *chip8)
{
	ssize_t rs = 1000 / 60;

	struct timespec t;
	clock_gettime(CLOCK_BOOTTIME, &t);

	ssize_t last_ticks = t.tv_nsec / 1000000;
	ssize_t last_delta = 0;
	ssize_t global_delta = 0;
	ssize_t step_delta = 0;
	ssize_t render_delta = 0;

	bool quit = false;

	while (!quit) {
		/* SDL_Event ev; */
		/* while (SDL_PollEvent(&ev)) { */
		/* 	if (ev.type == SDL_QUIT) { */
		/* 		quit = true; */
		/* 		break; */
		/* 	} */
		/* } */

		// Update timers.
		clock_gettime(CLOCK_BOOTTIME, &t);
		ssize_t new_last_ticks = t.tv_nsec / 1000000;
		ssize_t old_last_ticks = last_ticks;
		last_delta = new_last_ticks - last_ticks;
		last_ticks = new_last_ticks;
		step_delta += last_delta;
		render_delta += last_delta;

		for (; step_delta >= 1; --step_delta)
			step(chip8);

		global_delta += last_delta;
		while (global_delta > rs) {
			global_delta -= rs;

			if (chip8->delay_tmr > 0) {
				--chip8->delay_tmr;
			}

			--chip8->sound_tmr;
			ui_buzzer = chip8->sound_tmr > 0;
		}

		if (render_delta > rs && chip8->redraw) {
			draw(chip8);
			chip8->redraw = false;
			render_delta -= rs;
		}

		usleep(1000);
	}
}

static void
fini(void)
{
	if ((tb_status & TB_ACTIVE) == TB_ACTIVE) {
		tb_shutdown();
		tb_status ^= TB_ACTIVE;
	}
}

int
main(int argc, char **argv)
{
	char *filename = "ibm.ch8";
	if (argc > 1) filename = argv[1];

	struct CHIP8 chip8;

	init_gui();
	init_cpu(&chip8);
	draw(&chip8);
	load(&chip8, filename);
	exec(&chip8);
	fini();

	return 0;
}
