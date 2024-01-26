import socket
UDP_IP = "10.0.0.115"
UDP_PORT = 4210
MESSAGE = b"win_dir\n"

print("UDP target IP: %s" % UDP_IP)
print("UDP target port: %s" % UDP_PORT)
print("message: %s" % MESSAGE)

sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM) # UDP

sock.sendto(MESSAGE, (UDP_IP, UDP_PORT))

#sock.bind((UDP_IP, UDP_PORT))

#while True:
data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes
print("received message: %s" % data)
