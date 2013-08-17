/**
 * @author Gabriel Ulici
 *
 * \section Copyright
 * Copyright (c) 2012 Night Ideas Lab (NIL)
 * 
 * \section Version
 * Version : 1.0
 *
 * \section Description
 * This module controls the chest button simple, double, triple click
 */

#include "SentinelChecker.h"
#include <alcommon/albroker.h>
#include <alcommon/albrokermanager.h>
#include <alcommon/almodule.h>
#include <alcommon/altoolsmain.h>
#include <boost/shared_ptr.hpp>
#include <signal.h>

using namespace AL;

#ifdef SENTINELCHECKER_IS_REMOTE
	#define ALCALL
#else
	#ifdef _WIN32
		#define ALCALL __declspec(dllexport)
	#else
		#define ALCALL
	#endif
#endif

extern "C"
{
	ALCALL int _createModule(boost::shared_ptr<ALBroker> pBroker)
	{
		// Init broker with the main broker instance from the parent executable.
		ALBrokerManager::setInstance(pBroker->fBrokerManager.lock());
		ALBrokerManager::getInstance()->addBroker(pBroker);
		ALModule::createModule<SPQR::SentinelChecker>(pBroker,"SentinelChecker");
		
		return 0;
	}
	
	ALCALL int _closeModule()
	{
		return 0;
	}
}

#ifdef SENTINELCHECKER_IS_REMOTE

int main(int argc, char *argv[])
{
	// Pointer to createModule.
	TMainType sig;
	sig = &_createModule;
	
	// Call main.
	ALTools::mainFunction("SentinelChecker",argc,argv,sig);
}

#endif
