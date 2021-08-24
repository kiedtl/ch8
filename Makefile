CMD      = @

VERSION  = 0.1.0
NAME     = ch8
SRC      = chip8.c util.c
TERMBOX  = third_party/termbox/bin/termbox.a
OBJ      = $(SRC:.c=.o)

WARNING  = -Wall -Wpedantic -Wextra -Wold-style-definition -Wmissing-prototypes \
	   -Winit-self -Wfloat-equal -Wstrict-prototypes -Wredundant-decls \
	   -Wendif-labels -Wstrict-aliasing=2 -Woverflow -Wformat=2 -Wtrigraphs \
	   -Wmissing-include-dirs -Wno-format-nonliteral -Wunused-parameter \
	   -Wincompatible-pointer-types \
	   -Werror=implicit-function-declaration -Werror=return-type

DEF      = -DVERSION=\"$(VERSION)\" -D_XOPEN_SOURCE=1000 -D_DEFAULT_SOURCE
INCL     = -Ithird_party/ -Ithird_party/termbox/src
CC       = cc
CFLAGS   = -Og -g $(DEF) $(INCL) $(WARNING) -funsigned-char
LD       = bfd
LDFLAGS  = -fuse-ld=$(LD) -L/usr/include -lm

.PHONY: all
all: $(NAME)

.PHONY: run
run: $(NAME)
	$(CMD)./$(NAME)

%.o: %.c
	@printf "    %-8s%s\n" "CC" $@
	$(CMD)$(CC) -c $< -o $@ $(CFLAGS)

$(OBJ): chip8.h

$(NAME): tui_main.c $(OBJ) $(TERMBOX)
	@printf "    %-8s%s\n" "CCLD" $@
	$(CMD)$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(TERMBOX):
	make -C third_party/termbox

.PHONY: clean
clean:
	rm -rf $(NAME) $(OBJ)

.PHONY: deepclean
deepclean: clean
	rm -rf $(TERMBOX)
