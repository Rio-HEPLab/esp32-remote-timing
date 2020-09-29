import socket
import time
import datetime
import struct
import numpy as np
import threading
import select
import sys

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

class UDP_Process: 
	def __init__(self):
		self._running = True
	def restart(self):
		self._running = True
	def stop(self):
		self._running = False
		
	def run(self):
		log("OK","UDP Control Thread started!")
		eventCounter=0
		while self._running:
			ready = select.select([sock], [], [], 1)
			if ready[0]:
				data, remote = sock.recvfrom(16) # buffer size is 16 bytes
				if (data == b'CALIB'):
					log("INFO","Entering on Calibration Mode!")
					refTime = millis()-start
					sock.sendto(struct.pack("<I",refTime),remote)
					log("INFO","Ref. time: %d"%(refTime))
					startTime = millis()-start
					log("INFO","Start time: %d"%(startTime))
					log("INFO","Waiting Return Package from %s"%(remote[0]))
					while True:
						if (sock.recv != None):
							data, Client_remote = sock.recvfrom(16)
							if (Client_remote==remote):
								log("OK","Received data: %s"%(data.decode(encoding='UTF-8',errors='strict')))
								break
					stopTime = millis()-start
					delayVar = int((stopTime - startTime)/2)
					log("INFO","Delay calculated: %d"%(delayVar))
					sock.sendto(struct.pack("<I",delayVar),remote)
					log("INFO","Delay packet sent!")
				else:
					try:
						data=struct.unpack_from('<BBBBBllB',data)
						MACAddress="{:X}:{:X}:{:X}:{:X}:{:X}:{:X}".format(data[0],data[1],data[2],data[3],data[4],data[5])
						timestamp=data[6]
						timecounter=data[7]
						state=data[8]
						mydata[eventCounter]=(MACAddress,timestamp,timecounter,state)
						eventCounter+=1
						log("OK","MAC: {} - Timestamp={} - Time Counter={} - State={:01b}".format(MACAddress,timestamp,timecounter,state))
					except:
						log("FAIL","Wrong data format - Problem found to decode!")
						sys.exit(1)
		log("OK","UDP Control Thread stoped!")
def log(mode, text):
	now = datetime.datetime.now()
	if (mode=="HD"):
		print(bcolors.HEADER+text+bcolors.ENDC+"\n")
	if (mode=="INFO"):
		print(str(now)+" - "+bcolors.OKBLUE+text+bcolors.ENDC+"\n")
	if (mode=="OK"):
		print(str(now)+" - "+bcolors.OKGREEN+text+bcolors.ENDC+"\n")
	if (mode=="FAIL"):
		print(str(now)+" - "+bcolors.FAIL+text+bcolors.ENDC+"\n")
	if (mode=="WARNING"):
		print(str(now)+" - "+bcolors.WARNING+text+bcolors.ENDC+"\n")
	
def millis():
	return int(round(time.time() * 1000))

start=millis()

log("HD","\n------------ UDP Master Python Control ------------")

UDP_IP = "127.0.0.1"
UDP_PORT = 3333

sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP
sock.bind(("", UDP_PORT))
#mydata={}
mydata=np.zeros((10000,4))

processUDP=UDP_Process()
t=threading.Thread(target=processUDP.run)
log("WARNING","UDP thread created! Waiting to start...")

while True:
	inputCMD = input('\t\tWaiting Commands...\n\n')
	if inputCMD.lower()=="exit":
		if t.is_alive():
			log("WARNING","Stoping the thread and closing program...")
			processUDP.stop()
			t.join()
		sys.exit(0)
	elif inputCMD.lower()=="stop":
		if t.is_alive():
			log("WARNING","Stoping the thread...")
			processUDP.stop()
			t.join()
		else:
			log("WARNING","Thread not running. Nothing to do...")
	elif inputCMD.lower()=="start":
		t=threading.Thread(target=processUDP.run)
		processUDP.restart()
		t.start()
	elif inputCMD.lower()=="help":
		log("HD","*****   Valid options   *****\nstart\t\tStart the program initializing the main thread to process the UDP protocol\nstop\t\tStop the UDP Control thread\nexit\t\tStop the main thread and close the program")
	else:
		log("FAIL","Command not found! Type help to get a list of valid commands...")