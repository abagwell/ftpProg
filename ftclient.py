#!/usr/bin/python

#Name: Andrew Bagwell
#File: ftclient.py
#Assignment: CS372
#Description: client program for simple FTP program for CS 372 course.

from __future__ import print_function  #I needed to import this to format printing


import sys
import socket
import os

def main(): 

	#command line validation

	if len(sys.argv) > 6 or len(sys.argv) < 5:
		print ("Invalid argument amount: 5 or 6 arguments required")
		print ("usage: python ftclient <server-hostname> <server-port> -l|-g [<filename>] <data-port>")
		sys.exit()

	remoteHost = sys.argv[1] #remote host will always be at the same index
	remotePort = int(sys.argv[2]) #remote port will always be at the same index
	command = sys.argv[3] #command will always be the same index 

	if len(sys.argv) == 6: 
		fileName = sys.argv[4]
		portNum = int(sys.argv[5])
		cmdMsg = sys.argv[3] + " " + sys.argv[4] + " " + sys.argv[5]

	else:
		portNum = int(sys.argv[4])
		cmdMsg = sys.argv[3] + " " + sys.argv[4] 

	#Connection set up
	#Set up Control Socket and send Commands over
	controlSocket = setupCSock(remoteHost, remotePort)
	controlSocket.send(cmdMsg)
	
	#use the commands from the command line to determine what to do

	if not (command == '-l' or command == '-g'):
		response = controlSocket.recv(50) 
		print (response)
		return

	if command == '-l':
		getDirectory(portNum)
		

	elif command == '-g':
		getFile(controlSocket, portNum, fileName, remoteHost, remotePort)
		
	controlSocket.close

#--------------------------------------
#This function sets up the control socket and returns that live socket to the program for use
#It takes a hostName and a port number as parameters
#---------------------------------------

def setupCSock(hostName, portPassed): 

	#create control connection socket
	cSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

	#connect to connection socket on server
	cSock.connect((hostName, portPassed))

	print ("Connected to ", hostName)

	return cSock

#-------------------------------------------------------------------------------
#This function sets up a live socket used for data transfer between client and server. 
#It returns that socket for use by the program. 
#-----------------------------------------------------------------------------

def setupDSock(port): 

	#create data connection socket
	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

	thisHost = socket.gethostname()

	#connection data connection socket
	sock.bind((thisHost, port))

	print ("FTP data connection established on port ", port)

	sock.listen(5)

	dataSock, addr = sock.accept()

	print ("FTP data session established with client")

	sock.close

	return dataSock

#--------------------------------------------------------------------
#This function takes a port number as a parameter, creates a socket 
#that will be connected to by the serverr and sent the file.
#-------------------------------------------------------------------

def getDirectory(portPassed):

	dataSocket = setupDSock(portPassed)
		
	while True: 
		dirContent = dataSocket.recv(1)
		print (dirContent, end='')
		if not dirContent:
			break
		dataSocket.close
	return

#--------------------------------------------------------------------------------------------------
#This function takes a control socket, the port number for the data tcp socket to be created, a filename 
#to be created (if necessary) on the server, and the server's hostname and port number. It then writes
#to file, completing the FTP process
#--------------------------------------------------------------------------------------------------

def getFile(controlSock, portPassed, fileN, remHost, remPort): 

	fileStatus = controlSock.recv(50)
	fileStatus = fileStatus.replace("\x00", "")

	if fileStatus != "OK": 
		print (fileStatus)
		return

	if os.path.exists(fileN) == True:		#https://docs.python.org/2/library/os.path.html#module-os.path
		dataSocket = setupDSock(portPassed)	
		while True:
			fileContent = dataSocket.recv(1)
			if not fileContent:
				break
		print ("file already exists in directory")
		dataSocket.close
		return

	else:									#http://www.informit.com/articles/article.aspx?p=2234249
		dataSocket = setupDSock(portPassed)
		print ("Receiving %s from %s:%d" % (fileN, remHost, remPort))
		newFile = open(fileN, 'wb')
		while True:
			fileContent = dataSocket.recv(1)
			newFile.write(fileContent)
			if not fileContent:
				break
		newFile.close()
		print ("File Transfer Complete")
		dataSocket.close

	return

#point of entry for .py script

if __name__ == "__main__":
    main()




