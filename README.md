#### SingleCarrier
A QPSK Single Carrier Testing Program

1600 Baud QPSK or 3000 bit/sec with BPSK pilots. This experiment sends 33 BPSK Pilots and 31 QPSK random data 500 times in a loop.

In the future, this will change, as what we really want is one frame of BPSK pilots, and 8 frames of QPSK data. The BPSK pilots are used to make this a coherent modem, where the pilots are compared to what was received and unfiltered.

This spectrum produced with: $ ./qpsk  

![My image](https://raw.githubusercontent.com/srsampson/SingleCarrier/master/spectrum-filtered.png)

FIR filter with 1200 Hz center Frequency, .35 alpha Root Raised Cosine Filter

Here's the time domain:

![My image](https://raw.githubusercontent.com/srsampson/SingleCarrier/master/time-domain.png)

Here's what it looks like on the FreeDV display:

![My image](https://raw.githubusercontent.com/srsampson/SingleCarrier/master/waveform.png)

