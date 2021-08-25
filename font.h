#ifndef FONT_H
#define FONT_H

#define FONT_HEIGHT  6
#define FONT_WIDTH   3
#define FONT_FOUNDRY "robey_lag_net"
#define FONT_FAMILY  "TomThumb"

#include <stdint.h>

static const uint8_t font_data[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // null
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // start of heading
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // start of text
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // end of text
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // end of transmission
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // enquiry
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // acknowledge
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // bell
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // backspace
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // character tabulation
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // line feed (lf)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // line tabulation
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // form feed (ff)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // carriage return (cr)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // shift out
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // shift in
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // data link escape
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // device control one
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // device control two
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // device control three
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // device control four
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // negative acknowledge
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // synchronous idle
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // end of transmission block
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // cancel
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // end of medium
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // substitute
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // escape
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // information separator four
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // information separator three
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // information separator two
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // information separator one
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ' '
	0x00, 0x80, 0x80, 0x80, 0x00, 0x80, // '!'
	0x00, 0x00, 0x00, 0x00, 0xA0, 0xA0, // '"'
	0x00, 0xA0, 0xE0, 0xA0, 0xE0, 0xA0, // '#'
	0x00, 0x60, 0xC0, 0x60, 0xC0, 0x40, // '$'
	0x00, 0x80, 0x20, 0x40, 0x80, 0x20, // '%'
	0x00, 0xC0, 0xC0, 0xE0, 0xA0, 0x60, // '&'
	0x00, 0x00, 0x00, 0x00, 0x80, 0x80, // '''
	0x00, 0x40, 0x80, 0x80, 0x80, 0x40, // '('
	0x00, 0x80, 0x40, 0x40, 0x40, 0x80, // ')'
	0x00, 0x00, 0x00, 0xA0, 0x40, 0xA0, // '*'
	0x00, 0x00, 0x00, 0x40, 0xE0, 0x40, // '+'
	0x00, 0x00, 0x00, 0x00, 0x40, 0x80, // ','
	0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, // '-'
	0x00, 0x00, 0x00, 0x00, 0x00, 0x80, // '.'
	0x00, 0x20, 0x20, 0x40, 0x80, 0x80, // '/'
	0x00, 0x60, 0xA0, 0xA0, 0xA0, 0xC0, // '0'
	0x00, 0x40, 0xC0, 0x40, 0x40, 0x40, // '1'
	0x00, 0xC0, 0x20, 0x40, 0x80, 0xE0, // '2'
	0x00, 0xC0, 0x20, 0x40, 0x20, 0xC0, // '3'
	0x00, 0xA0, 0xA0, 0xE0, 0x20, 0x20, // '4'
	0x00, 0xE0, 0x80, 0xC0, 0x20, 0xC0, // '5'
	0x00, 0x60, 0x80, 0xE0, 0xA0, 0xE0, // '6'
	0x00, 0xE0, 0x20, 0x40, 0x80, 0x80, // '7'
	0x00, 0xE0, 0xA0, 0xE0, 0xA0, 0xE0, // '8'
	0x00, 0xE0, 0xA0, 0xE0, 0x20, 0xC0, // '9'
	0x00, 0x00, 0x00, 0x80, 0x00, 0x80, // ':'
	0x00, 0x00, 0x40, 0x00, 0x40, 0x80, // ';'
	0x00, 0x20, 0x40, 0x80, 0x40, 0x20, // '<'
	0x00, 0x00, 0x00, 0xE0, 0x00, 0xE0, // '='
	0x00, 0x80, 0x40, 0x20, 0x40, 0x80, // '>'
	0x00, 0xE0, 0x20, 0x40, 0x00, 0x40, // '?'
	0x00, 0x40, 0xA0, 0xE0, 0x80, 0x60, // '@'
	0x00, 0x40, 0xA0, 0xE0, 0xA0, 0xA0, // 'A'
	0x00, 0xC0, 0xA0, 0xC0, 0xA0, 0xC0, // 'B'
	0x00, 0x60, 0x80, 0x80, 0x80, 0x60, // 'C'
	0x00, 0xC0, 0xA0, 0xA0, 0xA0, 0xC0, // 'D'
	0x00, 0xE0, 0x80, 0xE0, 0x80, 0xE0, // 'E'
	0x00, 0xE0, 0x80, 0xE0, 0x80, 0x80, // 'F'
	0x00, 0x60, 0x80, 0xE0, 0xA0, 0x60, // 'G'
	0x00, 0xA0, 0xA0, 0xE0, 0xA0, 0xA0, // 'H'
	0x00, 0xE0, 0x40, 0x40, 0x40, 0xE0, // 'I'
	0x00, 0x20, 0x20, 0x20, 0xA0, 0x40, // 'J'
	0x00, 0xA0, 0xA0, 0xC0, 0xA0, 0xA0, // 'K'
	0x00, 0x80, 0x80, 0x80, 0x80, 0xE0, // 'L'
	0x00, 0xA0, 0xE0, 0xE0, 0xA0, 0xA0, // 'M'
	0x00, 0xA0, 0xE0, 0xE0, 0xE0, 0xA0, // 'N'
	0x00, 0x40, 0xA0, 0xA0, 0xA0, 0x40, // 'O'
	0x00, 0xC0, 0xA0, 0xC0, 0x80, 0x80, // 'P'
	0x00, 0x40, 0xA0, 0xA0, 0xE0, 0x60, // 'Q'
	0x00, 0xC0, 0xA0, 0xE0, 0xC0, 0xA0, // 'R'
	0x00, 0x60, 0x80, 0x40, 0x20, 0xC0, // 'S'
	0x00, 0xE0, 0x40, 0x40, 0x40, 0x40, // 'T'
	0x00, 0xA0, 0xA0, 0xA0, 0xA0, 0x60, // 'U'
	0x00, 0xA0, 0xA0, 0xA0, 0x40, 0x40, // 'V'
	0x00, 0xA0, 0xA0, 0xE0, 0xE0, 0xA0, // 'W'
	0x00, 0xA0, 0xA0, 0x40, 0xA0, 0xA0, // 'X'
	0x00, 0xA0, 0xA0, 0x40, 0x40, 0x40, // 'Y'
	0x00, 0xE0, 0x20, 0x40, 0x80, 0xE0, // 'Z'
	0x00, 0xE0, 0x80, 0x80, 0x80, 0xE0, // '['
	0x00, 0x00, 0x00, 0x80, 0x40, 0x20, // '\'
	0x00, 0xE0, 0x20, 0x20, 0x20, 0xE0, // ']'
	0x00, 0x00, 0x00, 0x00, 0x40, 0xA0, // '^'
	0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, // '_'
	0x00, 0x00, 0x00, 0x00, 0x80, 0x40, // '`'
	0x00, 0x00, 0xC0, 0x60, 0xA0, 0xE0, // 'a'
	0x00, 0x80, 0xC0, 0xA0, 0xA0, 0xC0, // 'b'
	0x00, 0x00, 0x60, 0x80, 0x80, 0x60, // 'c'
	0x00, 0x20, 0x60, 0xA0, 0xA0, 0x60, // 'd'
	0x00, 0x00, 0x60, 0xA0, 0xC0, 0x60, // 'e'
	0x00, 0x20, 0x40, 0xE0, 0x40, 0x40, // 'f'
	0x00, 0x60, 0xA0, 0xE0, 0x20, 0x40, // 'g'
	0x00, 0x80, 0xC0, 0xA0, 0xA0, 0xA0, // 'h'
	0x00, 0x80, 0x00, 0x80, 0x80, 0x80, // 'i'
	0x20, 0x00, 0x20, 0x20, 0xA0, 0x40, // 'j'
	0x00, 0x80, 0xA0, 0xC0, 0xC0, 0xA0, // 'k'
	0x00, 0xC0, 0x40, 0x40, 0x40, 0xE0, // 'l'
	0x00, 0x00, 0xE0, 0xE0, 0xE0, 0xA0, // 'm'
	0x00, 0x00, 0xC0, 0xA0, 0xA0, 0xA0, // 'n'
	0x00, 0x00, 0x40, 0xA0, 0xA0, 0x40, // 'o'
	0x00, 0xC0, 0xA0, 0xA0, 0xC0, 0x80, // 'p'
	0x00, 0x60, 0xA0, 0xA0, 0x60, 0x20, // 'q'
	0x00, 0x00, 0x60, 0x80, 0x80, 0x80, // 'r'
	0x00, 0x00, 0x60, 0xC0, 0x60, 0xC0, // 's'
	0x00, 0x40, 0xE0, 0x40, 0x40, 0x60, // 't'
	0x00, 0x00, 0xA0, 0xA0, 0xA0, 0x60, // 'u'
	0x00, 0x00, 0xA0, 0xA0, 0xE0, 0x40, // 'v'
	0x00, 0x00, 0xA0, 0xE0, 0xE0, 0xE0, // 'w'
	0x00, 0x00, 0xA0, 0x40, 0x40, 0xA0, // 'x'
	0x00, 0xA0, 0xA0, 0x60, 0x20, 0x40, // 'y'
	0x00, 0x00, 0xE0, 0x60, 0xC0, 0xE0, // 'z'
	0x00, 0x60, 0x40, 0x80, 0x40, 0x60, // '{'
	0x00, 0x80, 0x80, 0x00, 0x80, 0x80, // '|'
	0x00, 0xC0, 0x40, 0x20, 0x40, 0xC0, // '}'
	0x00, 0x00, 0x00, 0x00, 0x60, 0xC0, // '~'
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ''
};

#endif