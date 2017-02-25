# s-mptcp
A (simplified) client for acheiving communication/data transfer via the [MPTCP](https://tools.ietf.org/html/rfc6824) protocol

#### Meta
Kyle Carson
CS 176B
Winter 2017 UCSB

## Abstract
This program utilizes both traditional TCP protcol features (slow start, congestion avoidance, fast recovery, fast retransmit) as well as the scheduling guidelines outline by multi-path TCP.

## Usage
- The program requires both pthreads and the server-interface library. To build the project, simply run make, with the ouput executable being "smptcp"
- At the comand line, the program can be excuted by running:

```sh
[user@computer]$ ./smptcp [-h hostname] [-p port] [-f filename] [-n num_interfaces]
```

- An additional useful command includes inserting "time" before "./smptcp" to get some timing metrics for the client's performance

## Results
I originally set out to fully implement the TCP protocol up through TCP NewReno's multi-packet drop congestion features, however the server seems to have not been implemented according to the standard, making it too dificult to write a one-to-one mapping from the spec to code. Instead, I scaled back the packet retransmission code for a more simplistic, redundant approach.

For the scheduling of the different channels, I designate each channel a cwnd and ssthresh value, modifying them indpently of each other. When increasing any cwnd value, I use the algorithms outline in [RFC6824](https://tools.ietf.org/html/rfc6824) in order to get better overall scaling of the channels. In order to select which channel to actually send on, I simply round robin them to distribute the load as evenly as possible. While this may not be ideal in real-world scenarios where an underutilized channel might occur due to poor connection stability/;atency, the prior knowledge that all channels have a 33% packet drop rate and every packet has a random delay between 0 and 5 seconds makes those considerations trivial.

Additioanly, I set some hard limits beyond those outlined in the traditional TCP protocols to make sure not to overflow the server beyond what it could handle, resulting in a loss of connection.

The mp_recv function will throw two error during operation: Resource not tavialbe, and Connection refused. The former is a result of the timer I've applied to the socket connection, which simply indicates a timeout has occurred while waiting for a packet. The latter seems to occur whenever too much data is allowed into the network, and in some cases still happens, however it is handled by simply cleaning up and exiting the program.

The program includes verbose logging which consists of the actions take by the program, the contents of each packet sent and received, and the current state the system.

## References
(1) [RFC5681: TCP Congestion Control](https://tools.ietf.org/html/rfc5681)

(2) [RFC6824: Coupled Congestion Control for Multipath Transport Protocols](https://tools.ietf.org/html/rfc6824)

(3) [TCP Reno and Congestion Management](http://intronetworks.cs.luc.edu/current/html/reno.html)