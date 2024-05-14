import socket
import argparse
import sys
import time
import threading

HOSTIP = 'localhost'
HOSTPORT = 60001

def recv(sock):
    data = sock.recv(1)
    buf = b""
    while data.decode("utf-8") != "\n":
        buf += data
        data = sock.recv(1)
    return buf

def water(sock):
	while True:
		time.sleep(1)
		sock.send(b"WATER\n")
		time.sleep(100)
	
def turnon(sock):
	sock.send(b"TURNON\n")


def handle_client(conn):
	while True:
		data = conn.recv(1024)
		time.sleep(0.2)
		if not data:
  			break
		print(f"Received:  {data.decode()}")
		processLightLevel(data, conn)
				
			
	conn.close()
	
def turnOnLightbulbs(conn):
	conn.send(b"LIGHTBULBS\n");
	
def turnOffLightbulbs(conn):
	conn.send(b"OFFLIGHTBULBS\n");
	
	
def processLightLevel(data, conn):
	lightlevel = data.decode().split("LIGHTSENSOR")
	strlightLevel = ""
	if len(lightlevel) > 1:
		for elem in lightlevel:
			for char in elem:
				if char.isdigit():
					strlightLevel += char
		if strlightLevel != "":
			lightLevel = int(strlightLevel)
			if lightLevel > 40:
				turnOnLightbulbs(conn)
			if lightLevel <= 40:
				turnOffLightbulbs(conn)


def main(ip, port):
	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	sock.connect((ip, port))
	
	waterThread = threading.Thread(target=water, args=(sock, ))
	turnonThread = threading.Thread(target=turnon, args=(sock,))
	client_thread = threading.Thread(target=handle_client, args=(sock,))
	waterThread.start()
	turnonThread.start()
	client_thread.start()


if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument("--ip", dest="ip", type=str)
    parser.add_argument("--port", dest="port", type=int)
    args = parser.parse_args()

    main(args.ip, args.port)

