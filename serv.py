#Based on the code given by teaching assistant Gorby Kabasele Ndonda

import socket
import argparse
import sys
import time
import threading

HOSTIP = 'localhost'
HOSTPORT = 60001 


def water(sock):
	"""
	Function used to send the instruction to water plants to the network every 100 seconds.
	"""
	while True:
		time.sleep(1)
		sock.send(b"WATER\n")
		time.sleep(100)
	

def handle_client(conn):
	"""
	Function used to receive data. In function of the data received, if it is a lightlevel, we process it.
	"""
	while True:
		data = conn.recv(1024)
		time.sleep(0.2)
		if not data:
  			break
		print(f"Received:  {data.decode()}")
		processLightLevel(data, conn)
				
			
	conn.close()
	
def turnOnLightbulbs(conn):
	"""
	Function used to command the gateway to turn on lightbulbs
	"""
	conn.send(b"LIGHTBULBS\n");

	
	
def processLightLevel(data, conn):
	"""
	Function used to process the lightlevel received from the gateway. If it is bigger than a certain level (here, 400, can be anything else), we order the gateway to turn on lightbulbs.
	"""
	lightlevel = data.decode().split("LIGHTSENSOR")
	strlightLevel = ""
	if len(lightlevel) > 1:
		for elem in lightlevel:
			for char in elem:
				if char.isdigit():
					strlightLevel += char
		if strlightLevel != "":
			lightLevel = int(strlightLevel)
			if lightLevel < 400:
				turnOnLightbulbs(conn)


def main(ip, port):
	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	sock.connect((ip, port))
	
	waterThread = threading.Thread(target=water, args=(sock, ))
	client_thread = threading.Thread(target=handle_client, args=(sock,))
	waterThread.start()
	client_thread.start()


if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument("--ip", dest="ip", type=str)
    parser.add_argument("--port", dest="port", type=int)
    args = parser.parse_args()

    main(args.ip, args.port)

