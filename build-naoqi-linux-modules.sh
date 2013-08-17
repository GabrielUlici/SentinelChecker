#!/bin/bash - 
#===============================================================================
#	
#	FILE:  build-naoqi-linux-modules.sh
#	
#		USAGE:  ./build-naoqi-linux-modules.sh
#	
#	DESCRIPTION:
#	
#		OPTIONS:  ---
#		REQUIREMENTS:  ---
#		BUGS:  ---
#		NOTES:  ---
#		AUTHOR: Gabriel Ulici (NIL), <ulicigabriel@gmail.com>
#		COMPANY: 
#		CREATED: 24/04/12 12:07:22 CET
#		REVISION:  ---
#===============================================================================
set -o nounset                              # Treat unset variables as an error

qibuild clean build-naoqi-linux-release -f
qibuild init --force
qibuild configure -c naoqi-linux SentinelChecker --release
qibuild make -c naoqi-linux SentinelChecker --release

cp build-naoqi-linux-release/sdk/lib/naoqi/libSentinelChecker.so ${AL_DIR}/lib
