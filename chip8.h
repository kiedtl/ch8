#include <stdint.h>
#include <stdbool.h>

#define ROM_START 0x200
#define FONT_START 0x00

#define D_HEIGHT 32
#define D_WIDTH  64

typedef size_t (*keydown_fn_t)(char);

struct CHIP8 {
	uint8_t  memory[4096];
	bool     display[D_WIDTH * D_HEIGHT];
	size_t   PC;
	uint16_t I;
	uint16_t stack[4096];
	size_t   SC;
	uint8_t  delay_tmr;
	uint8_t  sound_tmr;
	uint8_t  vregs[16];
	bool     redraw;
	ssize_t  wait_key;

	keydown_fn_t keydown_fn;
};

struct CHIP8_inst {
	uint16_t  op;
	uint8_t    P;
	uint8_t    X;
	uint8_t    Y;
	uint8_t    N;
	uint8_t   NN;
	uint16_t NNN;
};

void chip8_init(struct CHIP8 *chip8, keydown_fn_t keydown);
void chip8_load(struct CHIP8 *chip8, char *data, size_t sz);
struct CHIP8_inst chip8_next(struct CHIP8 *chip8, size_t where);
void chip8_step(struct CHIP8 *chip8);
