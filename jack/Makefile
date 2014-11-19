all: metronome simple_client midi_dump gensquare jsynthosc midils formant biquad

biquad: biquad.c
	gcc -o biquad biquad.c -lm `pkg-config --cflags --libs jack`

formant: formant.c
	gcc -o formant formant.c `pkg-config --cflags --libs jack`

midils: midils.c
	gcc -ggdb -o midils midils.c `pkg-config --cflags --libs jack`

jsynthosc: jsynthosc.c
	gcc -ggdb -o jsynthosc jsynthosc.c -lm -lpthread `pkg-config --cflags --libs jack`

midi_dump: midi_dump.c
	gcc -o midi_dump midi_dump.c -lpthread `pkg-config --cflags --libs jack`

metronome: metro.c
	gcc -o metronome metro.c -lm `pkg-config --cflags --libs jack`

simple_client: simple_client.c
	gcc -o simple_client simple_client.c `pkg-config --cflags --libs jack`

gensquare: gensquare.c
	gcc -o gensquare gensquare.c `pkg-config --cflags --libs jack`

clean:
	rm -f metronome simple_client midi_dump gensquare jsynthosc
