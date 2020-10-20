### Single Carrier QPSK Modem
A Single Carrier 1600 Baud QPSK Coherent Modem for Voice and Data

#### Description
The design is driven by the FreeDV 2020 Specifications which were originally designed for OFDM and multiple carriers. The 2020 mode has a set number of FEC bits, codec bits, and pilots. So the idea was to increase the baud rate, and just send the same data serially instead of in parallel.

1600 Baud QPSK or 2-bits per symbol is 3000 bit/sec. The pilots use BPSK for both coherency and in this modem can also be used for syncronization. One (1) frame of BPSK pilots/sync, and eight (8) frames of QPSK data.

#### Development
Currently working on the low level code. The modulation/demodulation seems to work, but I have to transmit I+Q.
The FIR filter refuses to work. It used to work, but I'm obviously doing something wrong.

I don't really like the design, as it is overly complex. However, with no FIR it does decode the pilots.

#### Building
To build the project, unzip the ```export-qpsk.zip``` file into an empty directory, ```cd qpsk``` into it, and type ```Make``` and it will build the project, leaving the compiled file in ```dist/Debug/GNU-Linux/``` directory.

This is based on its Netbeans 8.2 IDE I use for building.

