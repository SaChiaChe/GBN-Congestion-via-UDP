import socket
import sys
import struct
from func.MessageHandler import *
from func.SetIP import *

if len(sys.argv) != 5:
	print("Format: Sender.py <Sender_Port> <Agent_IP> <Agent_Port> <InputFile>")
	exit(1)

Sender = socket.gethostbyname(socket.gethostname()), int(sys.argv[1])
Agent = SetIP(sys.argv[2]), int(sys.argv[3])
print("Sender:", Sender, "Agent:", Agent)

Socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
Socket.bind(("127.0.0.1", Sender[1]))
print("==========Socket Ready==========")

print("==========Reading Data and Segmenting==========")
Filename = sys.argv[4]
File = open(Filename, "rb")
Segments = []
SeqNum = 1
Length_Segment = 1000
while True:
	Seg = File.read(Length_Segment)
	if not Seg:
		# fin
		Segments.append(PackSegment(len(Seg), SeqNum, 0, 1, 0, 0, Seg))
		break
	else:
		Segments.append(PackSegment(len(Seg), SeqNum, 0, 0, 0, 0, Seg))
	SeqNum += 1
File.close()
print("==========Finished Reading Data and Segmenting==========")

print("==========Start Trasmitting Data==========")
Threshhold = 16
WindowSize = 1
SegNum = 0
Timeout = 1
Socket.settimeout(Timeout)
PrevLastSent = 0
Fin = False
while not Fin:
	Flag_Timeout = False
	Flag_DropData = False
	# Send Data
	for i in range(min(WindowSize, len(Segments)-SegNum-1)):
		Socket.sendto(Segments[SegNum + i], Agent)
		if SegNum + i + 1 < len(Segments):
			if Flag_Timeout and SegNum + i <= PrevLastSent:
				print("resnd\tdata\t#%d,\twinSize = %d" % (SegNum + i + 1, WindowSize))
			else:
				print("send\tdata\t#%d,\twinSize = %d" % (SegNum + i + 1, WindowSize))

	PrevLastSent = SegNum + min(WindowSize, len(Segments)-SegNum-1)

	# Send all Data, now Send fin
	if SegNum == len(Segments) - 1:
		Socket.sendto(Segments[-1], Agent)
		print("send\tfin")
		try:
			Data, addr = Socket.recvfrom(1024)
			Header, Message = UnpackSegment(Data)
		except socket.timeout:
			print("time\tout,\t\tthreshhold = %d" % Threshhold)
			Flag_Timeout = True
			continue
		if Header[3]:
			print("recv\tfinack")
			Fin = True
			continue

	# Wait for ACK
	GotACK = []
	for i in range(min(WindowSize, len(Segments)-SegNum-1)):
		try:
			Data, addr = Socket.recvfrom(1024)
		except socket.timeout:
			Flag_Timeout = True
			break
		Header, Message = UnpackSegment(Data)
		if not Header[5]:
			# Got non-ACK from Receiver
			continue
		GotACK.append(Header[2])
		print("recv\tack\t#%d" % Header[2])
	
	if len(GotACK) > 0:
		SegNum = max(GotACK)
	if GotACK.count(SegNum) > 1:
		# Duplicate ACK
		Flag_DropData = True

	if Flag_Timeout or Flag_DropData:
		Threshhold = max(int(WindowSize/2), 1)
		print("time\tout,\t\tthreshhold = %d" % Threshhold)
		WindowSize = 1
	else:
		if WindowSize < Threshhold:
			WindowSize *= 2
		else:
			WindowSize += 1

Socket.close()