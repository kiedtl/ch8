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

#define BLACK 16
#define WHITE 255
#define L_RED 1

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

static _Bool quit = false;
static _Bool dbg = true;
static size_t dbg_step = 0;

struct CHIP8 chip8;

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
	tb_select_input_mode(TB_INPUT_ALT);
	tb_select_output_mode(TB_OUTPUT_256);

	ui_height = tb_height();
	ui_width = tb_width();
}

static void
_draw_u16(uint16_t word, size_t x, size_t y)
{
	size_t d1 = (word / 0x1000) % 0x10;
	size_t d2 = (word / 0x0100) % 0x10;
	size_t d3 = (word / 0x0010) % 0x10;
	size_t d4 = (word / 0x0001) % 0x10;
	tb_change_cell(x + 0, y, d1 >= 10 ? (d1 - 10) + 'A' : d1 + '0', BLACK, WHITE);
	tb_change_cell(x + 1, y, d2 >= 10 ? (d2 - 10) + 'A' : d2 + '0', BLACK, WHITE);
	tb_change_cell(x + 2, y, d3 >= 10 ? (d3 - 10) + 'A' : d3 + '0', BLACK, WHITE);
	tb_change_cell(x + 3, y, d4 >= 10 ? (d4 - 10) + 'A' : d4 + '0', BLACK, WHITE);
}

static void
_draw_u08(uint8_t byte, size_t x, size_t y)
{
	size_t d1 = (byte / 0x10) % 0x10;
	size_t d2 = (byte / 0x01) % 0x10;
	tb_change_cell(x + 0, y, d1 >= 10 ? (d1 - 10) + 'A' : d1 + '0', BLACK, WHITE);
	tb_change_cell(x + 1, y, d2 >= 10 ? (d2 - 10) + 'A' : d2 + '0', BLACK, WHITE);
}

static void
draw(void)
{
	size_t ty = 0;
	if (ui_buzzer) {
		for (size_t x = 0; x < ui_width; ++x) tb_change_cell(x, ty, ' ', BLACK, L_RED);
	} else {
		for (size_t x = 0; x < ui_width; ++x) tb_change_cell(x, ty, ' ', WHITE, WHITE);
	}
	ty += 2;

	for (size_t y = 0; y < D_HEIGHT; y += 2, ++ty) {
		for (size_t x = 0; x < D_WIDTH; ++x) {
			uint32_t bg = chip8.display[D_WIDTH * (y+0) + x] ? WHITE : BLACK;
			uint32_t fg = chip8.display[D_WIDTH * (y+1) + x] ? WHITE : BLACK;
			tb_change_cell(x, ty, 0x2584, fg, bg);
		}
	}
	ty += 2;

	for (
		ssize_t ity = ty, i = chip8.PC - 6;
		i < (ssize_t)sizeof(chip8.memory) && ity < (ssize_t)ui_height;
		i += 2, ++ity
	) {
		if (i < 0) continue;
		struct CHIP8_inst inst = chip8_next(&chip8, i);
		if (i == (ssize_t)chip8.PC)
			tb_change_cell(0, ity, '>', BLACK, WHITE);
		_draw_u16(inst.op, 2, ity);
	}

	size_t rty = ty;
	for (size_t r = 0; r < sizeof(chip8.vregs); ++r, ++rty) {
		_draw_u08(r, 10, rty);
		tb_change_cell(10, rty, 'v', BLACK, WHITE); // overwrite first digit of register name
		tb_change_cell(12, rty, ':', BLACK, WHITE);
		tb_change_cell(13, rty, ' ', BLACK, WHITE);
		_draw_u08(chip8.vregs[r], 14, rty);
	}
	++rty;

	tb_change_cell(11, rty, 'I', BLACK, WHITE);
	tb_change_cell(12, rty, ':', BLACK, WHITE);
	tb_change_cell(13, rty, ' ', BLACK, WHITE);
	_draw_u16(chip8.I, 14, rty);
	++rty;

	tb_change_cell(10, rty, 'S', BLACK, WHITE);
	tb_change_cell(11, rty, 'C', BLACK, WHITE);
	tb_change_cell(12, rty, ':', BLACK, WHITE);
	tb_change_cell(13, rty, ' ', BLACK, WHITE);
	_draw_u16(chip8.SC, 14, rty);
	++rty;

	tb_change_cell(10, rty, 'P', BLACK, WHITE);
	tb_change_cell(11, rty, 'C', BLACK, WHITE);
	tb_change_cell(12, rty, ':', BLACK, WHITE);
	tb_change_cell(13, rty, ' ', BLACK, WHITE);
	_draw_u16(chip8.PC, 14, rty);
	rty += 2;

	if (dbg) {
		tb_change_cell(10, rty, ' ', WHITE, BLACK);
		tb_change_cell(11, rty, 'D', WHITE, BLACK);
		tb_change_cell(12, rty, 'E', WHITE, BLACK);
		tb_change_cell(13, rty, 'B', WHITE, BLACK);
		tb_change_cell(14, rty, 'U', WHITE, BLACK);
		tb_change_cell(15, rty, 'G', WHITE, BLACK);
		tb_change_cell(16, rty, ' ', WHITE, BLACK);
	} else {
		tb_change_cell(10, rty, ' ', WHITE, WHITE);
		tb_change_cell(11, rty, ' ', WHITE, WHITE);
		tb_change_cell(12, rty, ' ', WHITE, WHITE);
		tb_change_cell(13, rty, ' ', WHITE, WHITE);
		tb_change_cell(14, rty, ' ', WHITE, WHITE);
		tb_change_cell(15, rty, ' ', WHITE, WHITE);
		tb_change_cell(16, rty, ' ', WHITE, WHITE);
	}

	tb_present();
}

static size_t
keydown(char key)
{
	draw();

	const char keys[] = {
		'X', '1', '2', '3',
		'Q', 'W', 'E', 'A',
		'S', 'D', 'Z', 'C',
		'4', 'R', 'F', 'V'
	};

	struct tb_event ev;
	ssize_t ret = 0;

	if ((ret = tb_peek_event(&ev, 512)) == 0)
		return 0;
        ENSURE(ret != -1); /* termbox error */

	if (ev.type == TB_EVENT_KEY && ev.ch) {
		return keys[(size_t)key] == (char)MAX(ev.ch, 255);
	} else if (ev.type == TB_EVENT_KEY && ev.key) {
		switch (ev.key) {
		break; case TB_KEY_CTRL_C: quit = true;
		break; case TB_KEY_CTRL_D: dbg = !dbg;
		break; case TB_KEY_CTRL_E: dbg_step += 1;
		}
	} else if (ev.type == TB_EVENT_RESIZE) {
		ui_height = tb_height();
		ui_width = tb_width();
	}

	return 0;
}

static void
load(char *filename)
{
	struct stat st;
	stat(filename, &st); // TODO: assert that retcode != 0

	char *src = ecalloc(st.st_size, sizeof(char));
	FILE *src_f = fopen(filename, "rb");
	fread(src, sizeof(char), st.st_size, src_f);
	fclose(src_f);

	chip8_load(&chip8, src, st.st_size);

	free(src);
}

static void
exec(void)
{
	ssize_t rs = 1000 / 60;

	struct timespec t;
	clock_gettime(CLOCK_BOOTTIME, &t);

	ssize_t last_ticks = t.tv_nsec / 1000000;
	ssize_t last_delta = 0;
	ssize_t global_delta = 0;
	ssize_t step_delta = 0;
	ssize_t render_delta = 0;

	while (!quit) {
		if (chip8.wait_key == -1) keydown(0);

		if (dbg) {
			if (dbg_step > 0) {
				draw();
				chip8_step(&chip8);
				chip8.delay_tmr = CHKSUB(chip8.delay_tmr, 1);
				chip8.sound_tmr = CHKSUB(chip8.sound_tmr, 1);
				ui_buzzer = chip8.sound_tmr > 0;
				chip8.redraw = false;
				--dbg_step;
			}

			usleep(10000);
			continue;
		}

		// Update timers.
		clock_gettime(CLOCK_BOOTTIME, &t);
		ssize_t new_last_ticks = t.tv_nsec / 1000000;
		last_delta = new_last_ticks - last_ticks;
		last_ticks = new_last_ticks;

		step_delta += last_delta;
		for (; step_delta >= 1; --step_delta)
			chip8_step(&chip8);

		global_delta += last_delta;
		while (global_delta > rs) {
			global_delta -= rs;

			chip8.delay_tmr = CHKSUB(chip8.delay_tmr, 1);
			chip8.sound_tmr = CHKSUB(chip8.sound_tmr, 1);

			ui_buzzer = chip8.sound_tmr > 0;
		}

		render_delta += last_delta;
		if (render_delta > rs) {
			if (chip8.redraw) {
				draw();
				chip8.redraw = false;
			}
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

	init_gui();
	chip8_init(&chip8, keydown);
	load(filename);
	draw();
	exec();
	fini();

	return 0;
}
