#!/bin/bash - 
#===============================================================================
#	
#	FILE:  buildcross-naoqi-nao-modules.sh
#	
#		USAGE:  ./buildcross-naoqi-nao-modules.sh
#	
#   DESCRIPTION:
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

qibuild clean build-naoqi-nao-release -f
qibuild init --force
qibuild configure -c naoqi-nao SentinelChecker --release
qibuild make -c naoqi-nao SentinelChecker --release
