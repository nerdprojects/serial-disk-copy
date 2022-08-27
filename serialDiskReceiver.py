import serial, time, struct, binascii

# 2400 baud, 8 data bits, 1 stop bits, no parity, no flow control.
comm = serial.Serial("/dev/ttyS0", 57600, 8, "N", 2, timeout=None)

# create binary file
outputFile = open("serialDiskCopy-data.bin", "wb+")


#print "listening, waiting for DISK header"
print "ready, waiting for serial data..."
while True:

	crcErrors = []

#	inputBytes = comm.read(1)
#	outputFile.write(inputBytes)
#	continue

	# read DISK header bytes
	inputBytes = ""
	while inputBytes != "DISK":
		inputBytes = comm.read(4)
	print "received DISK header"

	# read disk size as long
	inputBytes = comm.read(4)
	diskSize = struct.unpack(">l", inputBytes)[0]
	print "received disk size: "+str(diskSize)

	print "disk transfer started"

	startTime = time.time()
	totalReadBytes = 0
	while totalReadBytes < diskSize:

		# read DATA header bytes
		inputBytes = ""
		while inputBytes != "DATA":
			inputBytes = comm.read(4)
			#print "waining for data, received "+inputBytes
		#print "received DATA header"

		# read data size as long
		inputBytes = comm.read(4)
		dataSize = struct.unpack(">l", inputBytes)[0]
		#print "received data size: "+str(dataSize)

		# read data
		dataBytes = comm.read(dataSize)
	
		# read crc as unsigned int
		inputBytes = comm.read(4)
		dataCrc = struct.unpack(">I", inputBytes)[0]
		#print "received crc: "+str(dataCrc)

		# calc crc
		refCrc = binascii.crc32(dataBytes)
		refCrc = refCrc % (1<<32) # convert the signed crc to unsigned
	
		if dataCrc != refCrc:
			crcErrors.append(str(totalReadBytes)+" - "+str(dataSize))
			print "CRC DOES NOT MATCH"
		#else:
			#print "CRC matches"

		totalReadBytes += dataSize
		outputFile.write(dataBytes)

		process = 100.0 / diskSize * totalReadBytes
		elapsedSeconds = time.time()-startTime
		bytesPerSecond = float(totalReadBytes)/float(elapsedSeconds)
		etaSeconds = (diskSize - totalReadBytes) / bytesPerSecond 
		print str(int(process))+"% - "+str(round(bytesPerSecond/1000*8,3))+"kb/s - ETA "+str(round(etaSeconds/60,2))+" min - received "+str(dataSize)+" bytes from "+str(totalReadBytes-dataSize)+" to "+str(totalReadBytes)+" of total "+str(diskSize)

	# read DONE footer bytes
	inputBytes = ""
	while inputBytes != "DONE":
		inputBytes = comm.read(4)
	#print "received DONE footer"
	print ""
	print str(len(crcErrors))+" errors occured"
	for error in crcErrors:
		print error
	print ""
	print "we are done..."
	break;
