### QPSK Modem
A Single Carrier 1600 Baud Coherent QPSK Modem for Voice and Data

#### Description
The design is driven by the FreeDV 2020 Specifications which were originally designed for OFDM and multiple carriers. The 2020 mode has a set number of FEC bits, codec bits, and pilots. So the idea was to increase the baud rate, and just send the same data serially instead of in parallel.

1600 Baud QPSK or 2-bits per symbol is 3000 bit/sec. The pilots use BPSK for both coherency and in this modem can also be used for syncronization. One (1) frame of BPSK pilots/sync, and eight (8) frames of QPSK data.

I've also added a raw AX.25 packet layer, which uses slightly changed algorithms.

#### Testing Program
I've created a program called ```qpsk```

To compile and create the program, download the:
```
export-qpsk.zip
```
and burst it into an empty directory, then type:
```
make
```
Then execute it using:
```
$ padsp ./qpsk
```
This starts the program which runs in a loop looking for either soundcard audio, or pseudo-terminal input.

This requires ax25-tools to be loaded, as well as:
```
sudo apt-get install osspd
```
The latter creates a /dev/dsp device for simple modem control.
#### Transmit Side
When you run the program it should output:
```
Pseudo Terminal to connect with: /dev/pts/1
Sound write sample size 16
Sound read sample size 16
Number of write channels 1
Number of read channels 1
Write sample rate 8000
Read sample rate 8000
Full duplex settable
Full duplex capability 1
```
It then runs in a loop. If you open another window and run the ```tncattach``` tool by Mark Qvist.
```
sudo ./tncattach -T -H 127.0.0.1 -P 33340 -i 44.78.0.4/24 -e -m 554
```
Where your IP and MTU of 554 octets would be set. You can use ```ifconfig``` and ```route``` to see the changes.

I'm  It uses a TCP/IP port for a virtual KISS interface. Also, you don't need the legacy AX.25 Tools.

https://unsigned.io/ethernet-and-ip-over-packet-radio-tncs/

Mark also has a Python 3 tool called ```Reticulum``` which looks like a pretty neat multi-hop networking application. While it was designed to be a secure network, he says it can also operate in several encryptionless modes.

https://unsigned.io/projects/reticulum/

Well anyway, sometimes TCP/IP is overkill. We normally just want to transport data, and in most cases only locally, and not worldwide.

#### Receive Side
I haven't done any debugging on the receive side yet.

To see what the scrambled and unscrambled modem audio sounds like, I put a couple WAV files in the ```docs``` directory.


