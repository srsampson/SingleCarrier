### Single Carrier QPSK Modem
A Single Carrier 1600 Baud QPSK Coherent Modem for Voice and Data

#### Description
The design is driven by the FreeDV 2020 Specifications which were originally designed for OFDM and multiple carriers. The 2020 mode has a set number of FEC bits, codec bits, and pilots. So the idea was to increase the baud rate, and just send the same data serially instead of in parallel.

1600 Baud QPSK or 2-bits per symbol is 3000 bit/sec. The pilots use BPSK for both coherency and in this modem can also be used for syncronization. One (1) frame of BPSK pilots/sync, and eight (8) frames of QPSK data.

In order to center in the audio passband of the transmitter, I chose 1200 Hz. This gives a 400 Hz to 2 kHz spectrum, or 1600 Hz bandwidth. Using a Root Raised Cosine filter at the transmitter and receiver, we can improve the design for digital data. I selected a .35 beta which provides a nice narrow spectrum. This can be optimized later. 

A FIR web tool I used is at: https://www-users.cs.york.ac.uk/~fisher/mkfilter/racos.html

The values input were:

Sample Rate: 8000 Hz  
Corner Frequency: 800 Hz  
Beta: .35  
Impulse Length: 41  
Sqrt Response Raised Cosine filter type  

#### Development
I went through a series of tests using an oscillator to shift the spectrum up and down to the 1200 Hz center. I finally went back to the simpler DFT and IDFT algorithms, which produce and decode the five cycles.

Currently the FIR filter does not work, and I suspect it is too narrow, or too compressed at the 3 dB points.
