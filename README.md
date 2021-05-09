#### QPSK Modem
A Single Carrier 1600 Baud Coherent QPSK Modem

#### Description
The design is driven by the FreeDV 2020 Specifications which were originally designed for OFDM and multiple carriers. The 2020 mode has a set number of FEC bits, codec bits, and pilots. So the idea was to increase the baud rate, and just send the same data serially instead of in parallel.

1600 Baud QPSK or 2-bits per symbol is 3000 bit/sec. The pilots use BPSK for both coherency and in this modem can also be used for syncronization. One (1) frame of BPSK pilots/sync, and eight (8) frames of QPSK data.

#### Building from git clone & scatter diagram tests

```
$ make qpsk
$ make test_scatter
```
Then view `scatter.png`

#### Development
The PSK signal may be both offset in frequency, and in phase, between multiple stations. Thus to get a good scatter diagram, you need to be tuned-in. You can find the frequency using the phase information.

What I'm thinking of, is using the BPSK preamble in which the phase of each symbol and the start of the data packet is determined. Using a tracking filter to lock-on to the phase and frequency. Then as each QPSK data symbol comes in, update the tracking filter with the measured phase error. Thus the tracking filter is the heart of the demodulator being successful.
