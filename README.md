### Single Carrier QPSK Modem
A Single Carrier 1600 Baud Coherent QPSK Modem for Voice and Data

#### Description
The design is driven by the FreeDV 2020 Specifications which were originally designed for OFDM and multiple carriers. The 2020 mode has a set number of FEC bits, codec bits, and pilots. So the idea was to increase the baud rate, and just send the same data serially instead of in parallel.

1600 Baud QPSK or 2-bits per symbol is 3000 bit/sec. The pilots use BPSK for both coherency and in this modem can also be used for syncronization. One (1) frame of BPSK pilots/sync, and eight (8) frames of QPSK data.

#### Development
Currently working on the low level code. The modulation/demodulation seems to work. Requires fine timing code.

Changed the input/output to real part only PCM, as most radios are just single channel audio input. This results in a bit of smearing on scatter diagram.

#### Building from git clone & scatter diagram tests

```
$ make test_scatter
```
Then view `scatter.png`
