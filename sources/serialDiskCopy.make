OBJS = serialDiskCopy.c.o

serialDiskCopy ƒƒ serialDiskCopy.make {OBJS}
	Link ∂
		-o serialDiskCopy -d -sym on ∂
		serialDiskCopy.c.o ∂
		-t 'APPL' ∂
		-c 'siow' ∂
		-mf ∂
		-model far ∂
		"{CLibraries}StdCLib.o" ∂
		"{Libraries}SIOW.far.o" ∂
		"{Libraries}MacRuntime.o" ∂
		"{Libraries}IntEnv.o" ∂
		"{Libraries}Interface.o" ∂
		-o serialDiskCopy
		
serialDiskCopy.c.o ƒ serialDiskCopy.make serialDiskCopy.c
	 SC -r  serialDiskCopy.c
	 
serialDiskCopy ƒƒ "{RIncludes}"SIOW.r
	Rez -a "{RIncludes}"SIOW.r -o serialDiskCopy
