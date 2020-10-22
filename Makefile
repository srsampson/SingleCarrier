# Makefile for single carrier QPSK modem

SRC=src/qpsk.c src/filter.c
HEADER=headers/filter_coef.h  headers/filter.h  headers/qpsk.h

qpsk: ${SRC} ${HEADER}
	gcc -Iheaders ${SRC} -DTEST_SCATTER -o qpsk -Wall -lm

# generate scatter diagram PNG
test_scatter: qpsk
	./qpsk 2>scatter.txt
	DISPLAY="" octave-cli -qf --eval "load scatter.txt; plot(scatter(1:1000,1),scatter(1:1000,2),'+'); print('scatter.png','-dpng')"
