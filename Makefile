all:
.SILENT:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

CC:=gcc -c -MMD -O2 -Isrc -I../rabbit/src -Werror -Wimplicit
LD:=gcc -L../rabbit/out
LDPOST:=-lrabbit -lX11 -lGLX -lGL -lpthread -lpulse -lpulse-simple -lm -lz

CFILES:=$(shell find src -name '*.c')
OFILES:=$(patsubst src/%.c,mid/%.o,$(CFILES))
-include $(OFILES:.o=.d)

mid/%.o:src/%.c;$(PRECMD) $(CC) -o $@ $<

DATASRCFILES:=$(shell find src/data -type f)
DATAMIDFILES:=\
  $(patsubst %.png,%, \
  $(patsubst src/data/%,mid/data/%,$(DATASRCFILES)) \
)
all:$(DATAMIDFILES)
mid/data/%:src/data/%.png;$(PRECMD) ../rabbit/out/rabbit imagec --dst=$@ --data=$^
mid/data/%.mid:src/data/%.mid;$(PRECMD) cp $< $@
mid/data/sfxar:src/data/sfxar;$(PRECMD) cp $< $@

EXE_MAIN:=out/chetyorska
all:$(EXE_MAIN)
$(EXE_MAIN):$(OFILES);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)

run:$(EXE_MAIN) $(DATAMIDFILES);$(EXE_MAIN)

clean:;rm -rf mid out
