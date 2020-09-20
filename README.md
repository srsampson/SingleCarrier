#### SingleCarrier Modem
A QPSK Single Carrier 1600 Baud Coherent Modem for Voice and Data

1600 Baud QPSK or 3000 bit/sec
Uses BPSK Pilots for Coherency and Syncronization

One (1) frame of BPSK pilots (also used for sync), and eight (8) frames of QPSK data.

#### Examples
This spectrum produced with: $ ./qpsk  

![My image](https://raw.githubusercontent.com/srsampson/SingleCarrier/master/spectrum-filtered.png)

FIR filter with 1200 Hz center Frequency, .35 alpha Root Raised Cosine Filter

Here's the waveform idea in the time domain:

![My image](https://raw.githubusercontent.com/srsampson/SingleCarrier/master/time-domain.png)

Here's what it looks like on the FreeDV display:

![My image](https://raw.githubusercontent.com/srsampson/SingleCarrier/master/waveform.png)

