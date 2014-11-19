all: devmidiout seqdemo amidimux

devmidiout: devmidiout.c
	gcc -O -o devmidiout devmidiout.c # && strip devmidiout

seqdemo: seqdemo.c
	gcc seqdemo.c -o seqdemo -lasound

amidimux: amidimux.c
	gcc -o amidimux amidimux.c -lasound

clean:
	rm -fr devmidiout seqdemo amidimux
