# GBN & Congestion via UDP
A practice of reliable data transfer with GBN(Go Back N)
, and congestion control with UDP.

## How to run

First compile SuperAgent.c
```
gcc SuperAgent.c -o SuperAgent.exe
```

Run Sender.py Reciever.py SuperAgent.exe in the same host
```
python Sender.py <Sender_Port> <Agent_IP> <Agent_Port> <InputFile>
```
```
python Receiver.py <Receiver_Port> <Agent_IP> <Agent_Port> <OutputFile>
```
```
./SuperAgent.exe <sender IP> <recv IP> <sender port> <agent port> <recv port> <loss_rate>
```

## Description

### Sender.py
The Sender will first segment the file into 1Kb size and add the header.
Then it will send packets of congestion window size to the agent,
and wait for the same amount of ACKs.
If the same amount of ACKs aren't received in timeout or duplicate ACK occurs(indicating there is data loss), 
then it will set threshold to half of current congestion window size and set congestion window to zero.
Otherwize, increase congestion window size and keep sending packets.
When all packets are sent, send a fin packet and wait for finACK, then end the program.

### Receiver.py
The Receiver waits for packets and send ACKs back if the buffer is not full.
If out ordered packet are received, or when the buffer is full, the Receiver will drop the packet.
If the buffer is full, on receiving a packet, the receiver will flush the data in to the desired file.
When received fin, send finACK and end program.

### SuperAgent.c
Agent receives from both Sender and Receiver, and drops data with probability <loss_rate>.
It will also check if GBN and congestion control behaves correctly.

## Built With

* Python 3.6.0 :: Anaconda custom (64-bit)

## Authors

* **SaKaTetsu** - *Initial work* - [SaKaTetsu](https://github.com/SaKaTetsu)