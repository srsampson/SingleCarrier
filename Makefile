# Makefile for single carrier QPSK modem

SRC=src/fir.c src/constants.c src/qpsk_mod.c
HEADER=headers/fir.h headers/optparse.h headers/qpsk_internal.h

qpsk_mod: ${SRC} ${HEADER}
	gcc -std=c11 -Iheaders ${SRC} -o qpsk_mod -Wall -lm

# generate scatter diagram PNG   TODO
#test_scatter: qpsk_demod
#	./qpsk_demod 2>scatter.txt
#	DISPLAY="" octave-cli -qf --eval "load scatter.txt; plot(scatter(1000:2000,1),scatter(1000:2000,2),'+'); print('scatter.png','-dpng')"
