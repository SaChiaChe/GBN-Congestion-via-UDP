import struct

Struct_Header = struct.Struct("6i")
Struct_Segment = struct.Struct("{}s1000s".format(Struct_Header.size))

def PackHeader(length, seqNumber, ackNumber, fin, syn, ack):
	for i in [length, seqNumber, ackNumber, fin, syn, ack]:
		if type(i) != int:
			print("Invalid format")
			return -1

	Header = Struct_Header.pack(length, seqNumber, ackNumber, fin, syn, ack)
	return Header

def FillUp(Data):
	while len(Data) < 1000:
		Data += b" "
	return Data

def PackSegment(length, seqNumber, ackNumber, fin, syn, ack, Data):
	Header = PackHeader(length, seqNumber, ackNumber, fin, syn, ack)
	Segment = Struct_Segment.pack(Header, FillUp(Data))
	return Segment

def UnpackSegment(Data):
	PackedHeader, Message = Struct_Segment.unpack(Data)
	Header = Struct_Header.unpack(PackedHeader)
	return Header, Message[:Header[0]]

	
