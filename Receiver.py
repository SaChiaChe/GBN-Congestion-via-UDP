import socket
import sys
import struct
from func.MessageHandler import *
from func.SetIP import *

if len(sys.argv) != 5:
	print("Format: Receiver.py <Receiver_Port> <Agent_IP> <Agent_Port> <OutputFile>")
	exit(1)

Receiver = socket.gethostbyname(socket.gethostname()), int(sys.argv[1])
Agent = SetIP(sys.argv[2]), int(sys.argv[3])
print("Receiver:", Receiver, "Agent:", Agent)

Socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
Socket.bind(("127.0.0.1", Receiver[1]))
print("==========Socket Ready==========")

Filename = sys.argv[4]
File = open(Filename, "wb")

Buffer = b""
BufferSize = 32
CurBufferSize = 0
CurSeqNum = 0
while True:
	Data, addr = Socket.recvfrom(1024)
	Header, Message = UnpackSegment(Data)
	if Header[5]:
		# Got ACK from Sender
		continue
	if not Header[3]:
		# Got Data from Sender
		if Header[1] != CurSeqNum+1:
			# Missing Segment
			print("drop\tdata\t#%d" % Header[1])
			Socket.sendto(PackSegment(0, 0, CurSeqNum, 0, 0, 1, b""), Agent)
			print("send\tack\t#%d" % CurSeqNum)

			if CurBufferSize == BufferSize:
				#if Buffer is full, flush
				File.write(Buffer)
				Buffer = b""
				CurBufferSize = 0
				print("flush")
			continue

		elif CurBufferSize < BufferSize:
			# Buffer is not full
			print("recv\tdata\t#%d" % Header[1])
			Buffer += Message
			CurBufferSize += 1
			CurSeqNum += 1
			# Send ACK
			Socket.sendto(PackSegment(0, 0, Header[1], 0, 0, 1, b""), Agent)
			print("send\tack\t#%d" % Header[1])

		else:
			# Buffer overflow
			print("drop\tdata\t#%d" % Header[1])

			# flush
			File.write(Buffer)
			Buffer = b""
			CurBufferSize = 0
			print("flush")
			continue

	else:
		# Got fin
		print("recv\tfin")
		# Send finACK
		Socket.sendto(PackSegment(0, 0, 0, 1, 0, 1, b""), Agent)
		print("send\tfinack")

		# flush
		File.write(Buffer)
		Buffer = b""
		CurBufferSize = 0
		print("flush")
		break

File.close()
Socket.close()