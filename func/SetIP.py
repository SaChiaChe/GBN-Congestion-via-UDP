import socket

def CheckIPv4(S):
	Splited = S.split(".")
	if len(Splited) != 4:
		return False
	for i in Splited:
		if not i.isdigit():
			return False
	return True

def SetIP(S):
	if S == "0.0.0.0" or S == "local" or S == "localhost":
		return ("127.0.0.1")
	elif not CheckIPv4(S):
		try:
			IP = socket.gethostbyname(S)
		except:
			print("Fail to get host IP")
			exit(0)
		return IP
	else:
		return S