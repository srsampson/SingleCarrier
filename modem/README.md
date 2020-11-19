### QPSK Modem Application

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
or:
```
$ padsp ./qpsk -ns
```
For no bit-scrambler.

This starts the program which runs in a loop looking for either soundcard audio, or network input.

This requires osspd to be loaded, as well:
```
sudo apt-get install osspd
```
The latter creates a /dev/dsp device for simple modem control.
#### Transmit Side
When you run the program it should output:
```
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

Here's what the spectrum looks like, with a 1100 Hz center frequency:

<img src="docs/spectrum.png" width="200">

#### Notes
You can also use the ```-v``` option of ```tncattach``` to see some debug activity. Also, you can run ```wireshark``` on the ```tnc0``` interface and watch the TCP/IP packets.

Sometimes the ```qpsk``` program gets a segmentation violation when tncattach is started. I haven't found where this is, but I suspect it is in the queue code.

#### Receive Side
I haven't done any debugging on the receive side yet.

To see what the scrambled and unscrambled modem audio sounds like, I put a couple WAV files in the ```docs``` directory.

