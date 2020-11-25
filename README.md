#### QPSK Modem
A Single Carrier 1600 Baud Coherent QPSK Voice Modem

#### Description
The design is driven by the FreeDV 2020 Specifications which were originally designed for OFDM and multiple carriers. The 2020 mode has a set number of FEC bits, codec bits, and pilots. So the idea was to increase the baud rate, and just send the same data serially instead of in parallel.

1600 Baud QPSK or 2-bits per symbol is 3000 bit/sec. The BPSK pilots are used for coherency. One (1) frame of BPSK pilots, and eight (8) frames of QPSK data.

#### Building from git clone

```
$ mkdir pskvoice
$ cd pskvoice
$ unzip ../export-pskvoice.zip
$ cd pskvoice
$ make
```
