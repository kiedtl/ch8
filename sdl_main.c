// Thanks:
//    - https://tobiasvl.github.io/blog/write-a-chip-8-emulator/

#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <SDL.h>
#include <time.h>

#include "chip8.h"
#include "util.h"
#include "font.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_AudioDeviceID device = 0;
static SDL_AudioSpec *spec = NULL;

static bool debug = true;
static size_t debug_steps = 0;

// Stolen from danirod/chip8
struct AudioData {
	float tone_pos;
	float tone_inc;
};

void feed(void *udata, uint8_t *stream, int len);
size_t keydown(char key);

bool key_statuses[16] = {0};
enum CHIP8_inst_type last_op = I_UNKNOWN;
size_t op_statistics[I_MAX] = {0};
size_t op_when[I_MAX] = {0};
size_t op_total = 0;
enum { INFM_1, INFM_3 } info_mode = INFM_1;

uint32_t _sdl_tick(uint32_t interval, void *param);
uint32_t _sdl_tick(uint32_t interval, void *param) {
	SDL_Event ev;
	SDL_UserEvent u_ev;

	ev.type = SDL_USEREVENT;
	u_ev.type = SDL_USEREVENT;
	u_ev.data1 = param;
	ev.user = u_ev;

	SDL_PushEvent(&ev);
	return interval;
}

static bool
init_gui(void)
{
	if (SDL_Init(SDL_INIT_EVERYTHING))
		return false;

	window = SDL_CreateWindow(
		"CHIP-8",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		C_D_WIDTH * 10, (C_D_HEIGHT * 10) * 2,
		SDL_WINDOW_SHOWN
	);
	if (window == NULL)
		return false;

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (renderer == NULL)
		return false;

	texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_STREAMING,
		128, 64 * 2
	);
	if (texture == NULL)
		return false;

	struct AudioData *audio = malloc(sizeof(struct AudioData));
	audio->tone_pos = 0;
	audio->tone_inc = 2 * 3.14159 * 1000 / 44100;

	// Set up the audiospec data structure required by SDL.
	spec = (SDL_AudioSpec *) malloc(sizeof(SDL_AudioSpec));
	spec->freq = 44100;
	spec->format = AUDIO_U8;
	spec->channels = 1;
	spec->samples = 4096;
	spec->callback = *feed;
	spec->userdata = audio;

	device = SDL_OpenAudioDevice(
		NULL, 0, spec, NULL,
		SDL_AUDIO_ALLOW_FORMAT_CHANGE
	);

	SDL_AddTimer((1000 / 60), _sdl_tick, NULL);

	return true;
}

static void __attribute__((format(printf, 6, 7)))
draw_text(uint32_t *pixels, size_t x, size_t y, uint32_t fg, uint32_t bg, char *fmt, ...)
{
	const size_t bdf_magic[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

	char buf[4096];
	memset(buf, 0x0, sizeof(buf));
	va_list ap;
	va_start(ap, fmt);
	int len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	ENSURE((size_t) len < sizeof(buf));

	// Set background color.
	for (size_t dy = y; dy < (y + FONT_HEIGHT + 1); ++dy) {
		for (size_t dx = x; dx < (x + ((FONT_WIDTH + 1) * len) + 1); ++dx) {
			pixels[(128 * dy) + dx] = bg;
		}
	}

	for (size_t startx = x + 1, ich = 0; ich < (size_t)len; ++ich) {
		char ch = buf[ich];

		for (size_t dy = y, fy = 0; fy < FONT_HEIGHT; ++fy, ++dy) {
			uint8_t font_row = font_data[(ch * FONT_HEIGHT) + fy];

			for (size_t dx = startx, fx = 0; fx < FONT_WIDTH; ++fx, ++dx) {
				bool font_px = (font_row & bdf_magic[fx]) != 0;
				pixels[(128 * dy) + dx] = font_px ? fg : bg;
			}
		}

		startx += FONT_WIDTH + 1;
	}
}

static void
draw(struct CHIP8 *chip8)
{
	const uint32_t colors[] = {
		0x001000ff, // backColor
		0xeeeeeeff, // fillColor
		0x7fffd4ff, // fillColor2 aquamarine
		0xffebcdff, // blendcolor aquamarine
	};
	const uint32_t d_fg = 0x001000ff; // fg for debug text
	const uint32_t d_bg = 0xeeeeeeff; // bg for debug text

	uint32_t *pixels;
	int       pitch;

	SDL_LockTexture(texture, NULL, (void *)&pixels, &pitch);

	// Expand pixels
	if (chip8->hires) {
		for (size_t i = 0; i < (S_D_HEIGHT*S_D_WIDTH); ++i)
			pixels[i] = colors[chip8->display[i]];
	} else {
		size_t x = 0;
		size_t y = 0;
		for (size_t i = 0; i < (C_D_HEIGHT*C_D_WIDTH); ++i) {
			uint32_t val = colors[chip8->display[i]];
			pixels[128 * (2 * y + 0) + (2 * x + 0)] = val;
			pixels[128 * (2 * y + 0) + (2 * x + 1)] = val;
			pixels[128 * (2 * y + 1) + (2 * x + 0)] = val;
			pixels[128 * (2 * y + 1) + (2 * x + 1)] = val;

			x += 1;

			if (x == D_WIDTH) {
				x = 0;
				y += 1;
			}

			if (y == D_HEIGHT) {
				break;
			}
		}
	}

	// Set background color of information area to white.
	for (size_t dy = S_D_HEIGHT; dy < 128; ++dy)
		for (size_t dx = 0; dx < S_D_WIDTH; ++dx)
			pixels[(128 * dy) + dx] = d_bg;

	switch (info_mode) {
	break; case INFM_1: {
		// Draw instruction queue.
		for (
			size_t y = S_D_HEIGHT + 4, ipc = chip8->PC;
			y < (127 - FONT_HEIGHT) && ipc < SIZEOF(chip8->memory);
			y += FONT_HEIGHT + 1
		) {
			struct CHIP8_inst inst = chip8_next(chip8, ipc);
			draw_text(
				pixels, 1, y, d_fg,
				ipc == chip8->PC ? 0xb9bab9ff : d_bg,
				"%04X", inst.op
			);
			ipc += inst.op_len;
		}

		// Draw registers.
		for (
			size_t y = S_D_HEIGHT + 4, r = 0;
			y < (127 - FONT_HEIGHT) && r < 8;
			y += FONT_HEIGHT + 1, ++r
		) {
			draw_text(
				pixels, 8 * FONT_WIDTH, y, d_fg, d_bg,
				"v%X:%04X  v%X:%04X",
				r, chip8->vregs[r], r + 8, chip8->vregs[r + 8]
			);
		}

		size_t y = S_D_HEIGHT + 4;
		size_t x = (19 * (FONT_WIDTH + 2)) - 1;

		draw_text(pixels, x, y, d_fg, d_bg, "plane:%02X", chip8->plane);
		y += FONT_HEIGHT + 1;
		draw_text(pixels, x, y, d_fg, d_bg, "delay:%02X", chip8->delay_tmr);
		y += FONT_HEIGHT + 1;
		draw_text(pixels, x, y, d_fg, d_bg, " I: %04X", chip8->I);
		y += FONT_HEIGHT + 1;
		draw_text(pixels, x, y, d_fg, d_bg, "PC: %04X", chip8->PC);
		y += FONT_HEIGHT + 1;
		draw_text(pixels, x, y, d_fg, d_bg, "SC: %04X", chip8->SC);
		y += FONT_HEIGHT + 1;

		draw_text(pixels, x, y,
			d_bg,
			chip8->hires ? 0x8e8f2cff : 0xb9bab9ff,
			" #HIRES "
		); y += FONT_HEIGHT + 1;

		draw_text(pixels, x, y,
			d_bg,
			chip8->wait_key != -1 ? 0x2c2c8eff : 0xb9bab9ff,
			" #INPUT "
		); y += FONT_HEIGHT + 1;

		draw_text(pixels, x, y,
			d_bg,
			chip8->sound_tmr > 0 ? 0x8e2c2cff : 0xb9bab9ff,
			" #SOUND "
		); y += FONT_HEIGHT + 1;
	} break; case INFM_3: {
		size_t y = S_D_HEIGHT + 4;

		draw_text(pixels, 1, y,
			d_bg,
			chip8->hires ? 0x8e8f2cff : 0xb9bab9ff,
			" #HIRES "
		); y += FONT_HEIGHT + 1;

		draw_text(pixels, 1, y,
			d_bg,
			chip8->wait_key != -1 ? 0x2c2c8eff : 0xb9bab9ff,
			" #INPUT "
		); y += FONT_HEIGHT + 1;

		draw_text(pixels, 1, y,
			d_bg,
			chip8->sound_tmr > 0 ? 0x8e2c2cff : 0xb9bab9ff,
			" #SOUND "
		); y += FONT_HEIGHT + 1;

		draw_text(pixels, 1, y,
			d_bg,
			debug ? 0x000000ff : 0xb9bab9ff,
			" #DEBUG "
		); y += FONT_HEIGHT + 1;

		draw_text(pixels, 1, y,
			d_bg,
			chip8->halt ? 0xff5050ff : 0xb9bab9ff,
			" HALTED "
		); y += FONT_HEIGHT + 1;

		for (
			size_t starty = S_D_HEIGHT + 4,
			       startx = 8 * (FONT_WIDTH + 2),
			       i = 0;
			i < I_MAX;
			++i
		) {
			float percent = ((float)op_statistics[i]) * 100 / op_total;
			size_t hue = (1.0 - percent) * 240;
			uint32_t c = (hsl_to_rgb((float)hue, 100, 50) << 8) | 0xFF;
			size_t yy = i / 7;
			size_t xx = i % 7;
			pixels[128 * (2 * yy + 0 + starty) + (2 * xx + 0 + startx)] = c;
			pixels[128 * (2 * yy + 0 + starty) + (2 * xx + 1 + startx)] = c;
			pixels[128 * (2 * yy + 1 + starty) + (2 * xx + 0 + startx)] = c;
			pixels[128 * (2 * yy + 1 + starty) + (2 * xx + 1 + startx)] = c;
		}

		for (
			size_t starty = S_D_HEIGHT + 4 + (7 * 2) + 4,
			       startx = 8 * (FONT_WIDTH + 2),
			       i = 0;
			i < I_MAX;
			++i
		) {
			float l = (float)MAX(10000, op_total - op_when[i]) / 10000.0;
			uint32_t c = (hsl_to_rgb(l * 240, 100, 40) << 8) | 0xFF;
			size_t yy = i / 7;
			size_t xx = i % 7;
			pixels[128 * (2 * yy + 0 + starty) + (2 * xx + 0 + startx)] = c;
			pixels[128 * (2 * yy + 0 + starty) + (2 * xx + 1 + startx)] = c;
			pixels[128 * (2 * yy + 1 + starty) + (2 * xx + 0 + startx)] = c;
			pixels[128 * (2 * yy + 1 + starty) + (2 * xx + 1 + startx)] = c;
		}

		for (
			size_t starty = S_D_HEIGHT + 4,
			       startx = 12 * (FONT_WIDTH + 2),
			       i = 0;
			i < 16;
			++i
		) {
			uint32_t c = key_statuses[i] ? 0x000000ff : 0xb9bab9ff;
			size_t yy = starty + ((i / 4) * (FONT_HEIGHT + 1));
			size_t xx = startx + ((i % 4) * (FONT_WIDTH  + 2));
			draw_text(pixels, xx, yy, d_bg, c, "%X", i);
		}
	} break; default: {
	} break;
	}

	SDL_UnlockTexture(texture);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

static void
load(struct CHIP8 *chip8, char *filename)
{
	struct stat st;
	stat(filename, &st); // TODO: assert that retcode != 0

	char *src = malloc(st.st_size);
	FILE *src_f = fopen(filename, "rb");
	fread(src, sizeof(char), st.st_size, src_f);
	fclose(src_f);

	chip8_load(chip8, src, st.st_size);

	free(src);
}

// Stolen from danirod/chip8 :/
//
// This is the function that generates the beep noise heard in the emulator.
// It generates RAW PCM values that are written to the stream. This is fast
// and has no dependencies on external files.
//
void
feed(void *udata, uint8_t *stream, int len)
{
	struct AudioData *audio = (struct AudioData *)udata;
	for (int i = 0; i < len; ++i) {
		stream[i] = sinf(audio->tone_pos) + 127;
		audio->tone_pos += audio->tone_inc;
	}
}

static void
sound(bool enabled)
{
	SDL_PauseAudioDevice(device, !enabled);
}

size_t
keydown(char key)
{
	char keys[] = {
		SDL_SCANCODE_X, // 0
		SDL_SCANCODE_1, // 1
		SDL_SCANCODE_2, // 2
		SDL_SCANCODE_3, // 3
		SDL_SCANCODE_Q, // 4
		SDL_SCANCODE_W, // 5
		SDL_SCANCODE_E, // 6
		SDL_SCANCODE_A, // 7
		SDL_SCANCODE_S, // 8
		SDL_SCANCODE_D, // 9
		SDL_SCANCODE_Z, // A
		SDL_SCANCODE_C, // B
		SDL_SCANCODE_4, // C
		SDL_SCANCODE_R, // D
		SDL_SCANCODE_F, // E
		SDL_SCANCODE_V  // F
	};

	if (key > 15) return 0;

	const uint8_t *sdl_keys = SDL_GetKeyboardState(NULL);
	uint8_t scancode = keys[(size_t)key];
	return sdl_keys[scancode];
	//return (size_t)key_statuses[(size_t)key];
}

// Because I'm an idiot with SDL, I stole this function wholesale from:
//    - https://github.com/danirod/chip8
static void
exec(struct CHIP8 *chip8)
{
	const char keys[] = {
		SDLK_x, // 0
		SDLK_1, // 1
		SDLK_2, // 2
		SDLK_3, // 3
		SDLK_q, // 4
		SDLK_w, // 5
		SDLK_e, // 6
		SDLK_a, // 7
		SDLK_s, // 8
		SDLK_d, // 9
		SDLK_z, // A
		SDLK_c, // B
		SDLK_4, // C
		SDLK_r, // D
		SDLK_f, // E
		SDLK_v  // F
	};

	ssize_t kcode;
	bool quit = false;
	SDL_Event ev;

	while (SDL_WaitEvent(&ev) && !quit) {
		switch (ev.type) {
		break; case SDL_QUIT:
			quit = true;
		break; case SDL_KEYDOWN:
			kcode = ev.key.keysym.sym;

			for (size_t i = 0; i < SIZEOF(keys); ++i) {
				if (kcode == keys[i]) {
					key_statuses[i] = true;
					break;
				}
			}
		break; case SDL_KEYUP:
			kcode = ev.key.keysym.sym;

			switch (kcode) {
			break; case SDLK_ESCAPE:
				quit = true;
			break; case SDLK_F1:
				debug = !debug;
			break; case SDLK_F2:
				debug_steps += 1;
			break; case SDLK_F9:
				switch (info_mode) {
				break; case INFM_1:
					info_mode = INFM_3;
				break; case INFM_3:
					info_mode = INFM_1;
				break; default:
				break;
				}
			break; default:
				for (size_t i = 0; i < SIZEOF(keys); ++i) {
					if (kcode == keys[i]) {
						key_statuses[i] = false;
						if (chip8->wait_key != -1) {
							chip8->vregs[chip8->wait_key] = i;
							chip8->wait_key = -1;
						}
						break;
					}
				}
			break;
			}
		break; case SDL_USEREVENT:
			for (size_t i = 0; (!debug || (debug && debug_steps > 0)) && i < 500; ++i) {
				struct CHIP8_inst current_inst = chip8_next(chip8, chip8->PC);
				op_total += 1;
				last_op = current_inst.type;
				op_statistics[current_inst.type] += 1;
				op_when[current_inst.type] = op_total;

				chip8_step(chip8);
				if (debug && debug_steps > 0) debug_steps -= 1;
			}

			SDL_FlushEvent(SDL_USEREVENT);

			if (chip8->delay_tmr > 0) --chip8->delay_tmr;
			if (chip8->sound_tmr > 0) --chip8->sound_tmr;

			sound(chip8->sound_tmr > 0);

			draw(chip8);
		break; default:
		break;
		}
	}
}

static void
fini(void)
{
	if (spec     != NULL) { free(spec->userdata); free(spec); }
	if (device   !=    0) { SDL_CloseAudioDevice(device);     }
	if (texture  != NULL) { SDL_DestroyTexture(texture);      }
	if (renderer != NULL) { SDL_DestroyRenderer(renderer);    }
	if (window   != NULL) { SDL_DestroyWindow(window);        }
	SDL_Quit();
}

int
main(int argc, char **argv)
{
	char *filename = "ibm.ch8";
	if (argc > 1) filename = argv[1];

	bool sdl_error = !init_gui();
	if (sdl_error) {
		fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
		return 1;
	}

	struct CHIP8 chip8;

	chip8_init(&chip8, keydown);
	draw(&chip8);
	load(&chip8, filename);
	exec(&chip8);
	fini();

	return 0;
}
