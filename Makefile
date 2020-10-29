# Makefile for single carrier QPSK modem

SRC=src/fir.c src/constants.c
HEADER=headers/fir.h headers/optparse.h headers/qpsk_internal.h

qpsk: ${SRC} ${HEADER} src/qpsk.c
	gcc -std=c11 -Iheaders ${SRC} src/qpsk.c -DTEST_SCATTER -o qpsk -Wall -lm
	
qpsk_get_test_bits: src/qpsk_get_test_bits.c
	gcc -std=c11 -Iheaders src/qpsk_get_test_bits.c -o qpsk_get_test_bits -Wall 

qpsk_mod: ${SRC} ${HEADER} src/qpsk_mod.c
	gcc -std=c11 -Iheaders ${SRC} src/qpsk_mod.c -o qpsk_mod -Wall -lm

# generate scatter diagram PNG
test_scatter: qpsk
	./qpsk 2>scatter.txt
	DISPLAY="" octave-cli -qf --eval "load scatter.txt; plot(scatter(1000:2000,1),scatter(1000:2000,2),'+'); print('scatter.png','-dpng')"
