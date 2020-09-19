#### SingleCarrier
A QPSK Single Carrier Testing Program

1600 Baud QPSK or 3000 bit/sec with BPSK pilots.

One (1) frame of BPSK pilots, and eight (8) frames of QPSK data. The BPSK pilots are used to make this a coherent modem, where the transmitted pilots are compared to what was received.

This spectrum produced with: $ ./qpsk  

![My image](https://raw.githubusercontent.com/srsampson/SingleCarrier/master/spectrum-filtered.png)

FIR filter with 1200 Hz center Frequency, .35 alpha Root Raised Cosine Filter

Here's the waveform idea in the time domain:

![My image](https://raw.githubusercontent.com/srsampson/SingleCarrier/master/time-domain.png)

Here's what it looks like on the FreeDV display:

![My image](https://raw.githubusercontent.com/srsampson/SingleCarrier/master/waveform.png)

