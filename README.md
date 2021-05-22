#### QPSK Modem
A Single Carrier 1600 Baud Coherent QPSK Modem

#### Description
1600 Baud QPSK or 2-bits per symbol is 3000 bit/sec. The preamble uses BPSK for syncronization. One (1) frame of BPSK pilots/sync, and eight (8) frames of QPSK data.

#### Development
The PSK signal may be both offset in frequency and phase between multiple stations. Thus to get a good scatter diagram, you need to correct for both.

What I'm thinking of, is using the BPSK preamble in which the phase of each symbol and the start of the data packet is determined. Using a tracking filter to lock-on to the phase and frequency. Then as each QPSK data symbol comes in, update the tracking filter with the measured phase error. Thus the tracking filter is the heart of the demodulator being successful.
