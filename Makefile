# Makefile for single carrier QPSK modem

SRC=src/qpsk.c
HEADER=headers/filter_coef.h headers/qpsk.h

qpsk: ${SRC} ${HEADER}
	gcc -std=c11 -Iheaders ${SRC} -DTEST_SCATTER -o qpsk -Wall -lm

# generate scatter diagram PNG
test_scatter: qpsk
	./qpsk 2>scatter.txt
	DISPLAY="" octave-cli -qf --eval "load scatter.txt; plot(scatter(1000:2000,1),scatter(1000:2000,2),'+'); print('scatter.png','-dpng')"
