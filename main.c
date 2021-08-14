#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <SDL.h>

#include "chip8.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_AudioDeviceID device = 0;
static SDL_AudioSpec *spec = NULL;

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
init_cpu(struct CHIP8 *chip8)
{
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

	// set fonts
	memcpy((void *)&chip8->memory[FONT_START], (void *)&fonts, sizeof(fonts));
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

	memcpy(chip8->memory + ROM_START, src, st.st_size);
}

uint8_t
listen_for(char key)
{
	uint8_t scancode = keys[key];
	const uint8_t *sdl_keys = SDL_GetKeyboardState(NULL);
	return sdl_keys[scancode];
}

void
exec(struct CHIP8 *chip8)
{
	int i = 0;
	while (++i < 6000) {
		if (chip8->redraw) {
			draw(chip8);
			chip8->redraw = false;
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
		break; case 0x9: // 9XY0: SNE - Skip next if X != Y
			chip8->PC += chip8->vregs[X] != chip8->vregs[Y] ? 2 : 0;
		break; case 0xA: // ANNN: LD - Set I to NNN
			chip8->I = NNN;
		break; case 0xB: // BNNN: JMP - Jump to V0 + NNN
			// TODO: configurable BXNN behaviour (jump to XNN + VX)?
			chip8->PC = (chip8->vregs[0] + NNN) & 0xFFF;
		break; case 0xC: // CXNN: RND - VX =  rand() & NN
			chip8->vregs[X] = rand() & NN;
		break; case 0xD: // DXYN: DRW - Draw a sprite at coord VX,VY
			chip8->redraw = true;
			size_t x = chip8->vregs[X] & (D_WIDTH-1);
			size_t y = chip8->vregs[Y] & (D_HEIGHT-1);

			chip8->vregs[15] = 0;
			for (int j = 0; j < N; ++j) {
				if ((y + 1) >= D_HEIGHT) continue;

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
				if (listen_for(key)) chip8->PC += 2;
			break; case 0xA1: // EXA1: Skip next if key VX is not pressed
				if (!listen_for(key)) chip8->PC += 2;
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
				;
				uint8_t key = chip8->vregs[X] & 0xF;
				if (!listen_for(key))
					chip8->PC -= 2; // redo this instruction. hacky.
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

		SDL_Delay(1);
	}
}

void
fini(void)
{
	if (texture  != NULL) SDL_DestroyTexture(texture);
	if (renderer != NULL) SDL_DestroyRenderer(renderer);
	if (window   != NULL) SDL_DestroyWindow(window);
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

	init_cpu(&chip8);
	load(&chip8, filename);
	exec(&chip8);
	fini();

	return 0;
}
