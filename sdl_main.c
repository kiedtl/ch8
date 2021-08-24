// Thanks:
//    - https://tobiasvl.github.io/blog/write-a-chip-8-emulator/

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <SDL.h>
#include <time.h>

#include "chip8.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_AudioDeviceID device = 0;
static SDL_AudioSpec *spec = NULL;

// Stolen from danirod/chip8
struct AudioData {
	float tone_pos;
	float tone_inc;
};

// Stolen from danirod/chip8
//
// Maps SDL scancodes to CHIP-8 keys.
const char keys[] = {
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

void feed(void *udata, uint8_t *stream, int len);

// Stolen from danirod/chip8
bool
init_gui(void)
{
	if (SDL_Init(SDL_INIT_EVERYTHING))
		return false;

	window = SDL_CreateWindow(
		"CHIP-8",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		D_WIDTH * 10, D_HEIGHT * 10,
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
		128, 64
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

	return true;
}

void
draw(struct CHIP8 *chip8)
{
	uint32_t *pixels;
	int       pitch;

	SDL_LockTexture(texture, NULL, (void *)&pixels, &pitch);

	// Expand pixels
	size_t x = 0;
	size_t y = 0;
	for (size_t i = 0; i < (D_HEIGHT*D_WIDTH); ++i) {
		uint32_t val = chip8->display[i] ? -1 : 0;
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

	SDL_UnlockTexture(texture);

	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

void
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

void
sound(bool enabled)
{
	SDL_PauseAudioDevice(device, !enabled);
}

size_t
keydown(char key)
{
	if (key < 0 || key > 15) return 0;

	const uint8_t *sdl_keys = SDL_GetKeyboardState(NULL);
	uint8_t scancode = keys[key];
	return sdl_keys[scancode];
}


// Because I'm an idiot with SDL, I stole this function wholesale from:
//    - https://github.com/danirod/chip8
void
exec(struct CHIP8 *chip8)
{
	size_t rs = 1000 / 60;

	ssize_t last_ticks = SDL_GetTicks();
	ssize_t last_delta = 0;
	ssize_t global_delta = 0;
	ssize_t step_delta = 0;
	ssize_t render_delta = 0;

	bool quit = false;

	while (!quit) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT) {
				quit = true;
				break;
			}
		}

		// Update timers.
		last_delta = SDL_GetTicks() - last_ticks;
		last_ticks = SDL_GetTicks();

		// Opcode execution; estimated 1000 ops/s
		step_delta += last_delta;
		for (; step_delta >= 1; --step_delta)
			chip8_step(chip8);

		global_delta += last_delta;
		while (global_delta > rs) {
			global_delta -= rs;

			if (chip8->delay_tmr > 0) {
				--chip8->delay_tmr;
			}

			--chip8->sound_tmr;
			sound(chip8->sound_tmr > 0);
		}

		// Render frame every 1/60th of a second
		render_delta += last_delta;
		for (; render_delta >= rs; render_delta -= rs)
			if (chip8->redraw) draw(chip8);
		chip8->redraw = false;

		SDL_Delay(1);
	}
}

void
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

	chip8_init(&chip8);
	draw(&chip8);
	load(&chip8, filename);
	exec(&chip8);
	fini();

	return 0;
}
