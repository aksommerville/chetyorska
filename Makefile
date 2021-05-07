all:
.SILENT:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

RABBITROOT:=../rabbit
RABBITBIN:=$(RABBITROOT)/out/rabbit
RABBITLIB:=$(RABBITROOT)/out/librabbit.a
RABBITHDR:=$(RABBITROOT)/src

CC:=gcc -c -MMD -O2 -Isrc -I$(RABBITHDR) -Werror -Wimplicit
LD:=gcc
LDPOST:=$(RABBITLIB) -lX11 -lGLX -lGL -lpthread -lpulse -lpulse-simple -lm -lz

CFILES:=$(shell find src -name '*.c')
OFILES:=$(patsubst src/%.c,mid/%.o,$(CFILES))
-include $(OFILES:.o=.d)

mid/%.o:src/%.c;$(PRECMD) $(CC) -o $@ $<

# `rabbit plan` needs these:
DATADST:=out/data
DATAMIDDIR:=mid/data
EXE_CLI:=$(RABBITBIN)
ifneq ($(MAKECMDGOALS),clean)
  include mid/data.mk
endif
DATASRCFILES:=$(shell find src/data -type f) # Need this to re-plan when a new file created.
mid/data.mk:$(DATASRCFILES);$(PRECMD) $(RABBITBIN) plan --dst=$@ --data=src/data
all:$(DATADST)

EXE_MAIN:=out/chetyorska
all:$(EXE_MAIN)
$(EXE_MAIN):$(OFILES);$(PRECMD) $(LD) -o $@ $^ $(LDPOST)

run:$(EXE_MAIN) $(DATADST);$(EXE_MAIN)

clean:;rm -rf mid out

edit-synth:;$(RABBITBIN) synth --data=src/data
