# SerialDiskCopy
## Info
SerialDiskCopy is a Macintosh application, that can be used to backup volumes over the serial port.

I created it, because I couldn't retrieve data I had on a exotic SCSI 40 pin harddisk used in the old PowerBook series (e.g. PowerBook 140, 145B, etc.)
The only interface to current PCs I had was the floppy drive, but this does not allow for good data retrieval. With my script, it's possible to copy the original disk on block level, mount it in an emulator like Basilisk II and even do some data recovery of deleted files with the old Norton Disk Utilities.
 
The solution is based on 2 components:
- A native Macintosh application (based on MPW SIOW) to transfer volumes onto the serial port
- A Python based script that receives the backup data on the serial port
 
In this repo, you find following data:
- serialDiskCopy.sit -> StuffIt 5.5 compressed version of the compiled Macintosh backup application and the MPW source files
- serialDiskReceiver.py -> the Python script to receive the backup data (i tested it on a Ubuntu 16 machine)
- floppy.bin -> a HFS formated floppy containing a minimal system 7 and the backup application (can be used as a boot device and to run the backup app from)
- howto.gif -> a gif, that shows the usage of the applications
- sources -> source files of the application to browse on Github (if you wan't to compile the app yourself, you need to extract the sources in the serialDiskCopy.sit file on a Macintosh instead, otherwise the files have the wrong encoding and you will miss the resource forks.)

Transfer is limited to 56k caused by the Macintosh serial port speed limit. So 40 MB take aproximatley 2 hour. Back to modem speed ;)
 
Plase note that the application is far from beeing polished and compatible with every system, I only tested it on my setup. But still I think it can act as a good base for other persons that need to backup their disks in a similar fashion.

## HowTo
<img src="https://raw.githubusercontent.com/nerdprojects/serial-disk-copy/main/howto.gif"/>
 
## Thanks
I want to thank David Shayer, the original author of the Hard Disk ToolKit and SEdit. These tools were used on the old Macs to do low level disk operations. David and SEdit helped me a lot when developing the application! (He even provided me the original source code of SEdit :-). Thanks also go out to Stephan Brumme for his crc32 implementation I used to calculate checksums to detect data transfer errors.
 
Original forum post from 2017:
https://68kmla.org/bb/index.php?threads/disk-backup-over-serial-port.8078
