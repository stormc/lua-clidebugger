SOURCE=libclidebugger.c
OBJECT=$(SOURCE:.c=.o)
SOLIB=$(SOURCE:.c=.so)
_CFLAGS = $(CFLAGS) -std=c99 -Wall -Wextra -Winline -Wpedantic
#CC=clang

all: $(SOURCE) $(SOLIB)

$(SOLIB): $(OBJECT)
	@$(CC) $(LDFLAGS) -shared $(OBJECT) -o $@
	@rm $(OBJECT)
	@strip $@
	@chmod a-x $@

.c.o:
	@$(CC) $(_CFLAGS) -fpic -c $< -o $@

clean:
	@rm $(SOLIB)
