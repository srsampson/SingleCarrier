### Single Carrier QPSK Modem
A Single Carrier 1600 Baud QPSK Coherent Modem for Voice and Data

#### Description
The design is driven by the FreeDV 2020 Specifications which were originally designed for OFDM and multiple carriers. The 2020 mode has a set number of FEC bits, codec bits, and pilots. So the idea was to increase the baud rate, and just send the same data serially instead of in parallel.

1600 Baud QPSK or 2-bits per symbol is 3000 bit/sec. The pilots use BPSK for both Coherency and in this modem can also be used for syncronization. One (1) frame of BPSK pilots/sync, and eight (8) frames of QPSK data.

#### Examples
This spectrum produced with: $ ./modulate  

![My image](https://raw.githubusercontent.com/srsampson/SingleCarrier/master/spectrum-filtered.png)

FIR filter with 1200 Hz center Frequency, .35 alpha Root Raised Cosine Filter

Here's the waveform idea in the time domain:

![My image](https://raw.githubusercontent.com/srsampson/SingleCarrier/master/time-domain.png)

Here's what it looks like on the FreeDV display:

![My image](https://raw.githubusercontent.com/srsampson/SingleCarrier/master/waveform.png)

This demodulator spectrum (after FIR filtering) produced with: $ ./demodulate  

![My image](https://raw.githubusercontent.com/srsampson/SingleCarrier/master/demod.png)
