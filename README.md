### Single Carrier QPSK Modem
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
It then runs in a loop. If you open another window and run:
```
sudo kissattach /dev/pts/1 qpsk
```
It will attach a KISS port to the ```/dev/pts/XX``` that was given by ```qpsk```. If you don't delete this attachment, the qpsk will open a different pair the next time you run it. So I always delete the kissattach process before running.

The ```qpsk``` in the ```kissattach``` command is the port name in ```/etc/ax25/axports``` and mine looks like:
```
# /etc/ax25/axports
#
# The format of this file is:
#
# name callsign speed paclen window description
#
#1	OH2BNS-1	1200	255	2	144.675 MHz (1200  bps)
#2	OH2BNS-9	38400	255	7	TNOS/Linux  (38400 bps)
qpsk K5OKC-1  38400 255 7 QPSK/Linux  (38400 bps)
```
Due to the way Linux Kernel AX.25 works (or broken), we need an additional step. This is to shut off the CRC checksum on the PTY. I found the solution in direwolf: https://github.com/wb2osz/direwolf/blob/master/src/kiss_frame.c#L551
```
sudo kissparms -c 1 -p qpsk
```
If you don't do this, the port will include the CRC with only the first two packets, then stop. This drives me crazy.

At this point you can try connecting to a random callsign and listen to the scrambled audio:
```
$ sudo kissattach /dev/pts/1 qpsk
AX.25 port qpsk bound to device ax0

$ ax25_call qam k5okc w1aw
```
In debug you should see this being modulated:
```
AE6282AE4040E0966A9E968640613F (SABM)
AE6282AE4040E0966A9E968640613F (SABM)
AE6282AE4040E0966A9E9686406153 (DISC)
```
You could also give yourself an IP address:
```
sudo kissattach -i 44.78.78.1 -m 128 /dev/pts/2 qpsk
```
Where your IP and MTU of 128 octets would be set. You can use ```ifconfig``` and ```route``` to see the changes. Here's an example:
```
$ ping 44.5.5.5

A2A6A840404060966A9E9686406303CD000300CC07040001966A9E968640022C880805000000000000002C050505 (ICMP)
```
You can see why I chose to use a scrambler, as the long string of identical octets will create all sorts of tones and become hard to decode.

#### Receive Side
I haven't done any debugging on the receive side yet.

To see what the scrambled and unscrambled modem audio sounds like, I put a couple WAV files in the ```docs``` directory.

