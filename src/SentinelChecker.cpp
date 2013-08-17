/**
 * @author Gabriel Ulici
 *
 * \section Copyright
 * Copyright (c) 2012 Night Ideas Lab (NIL)
 * 
 */

#include "SentinelChecker.h"
#include <alcommon/alproxy.h>
#include <alcommon/albroker.h>
#include <alcommon/almodule.h>
#include <alerror/alerror.h>
#include <almemoryfastaccess/almemoryfastaccess.h>
#include <alproxies/almotionproxy.h>
#include <althread/alcriticalsection.h>
#include <althread/almutex.h>
#include <alvalue/alvalue.h>
#include <boost/shared_ptr.hpp>
#include <rdkcore/sharedmemory/sharedmemory.h>
#include <utils/FileDatReader.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <qi/log.hpp>
#include <qi/os.hpp>
#include <fstream>
#include <iostream>

using namespace std;
using namespace AL;
using namespace Legged;
using RDK2::SharedMemory;

namespace SPQR
{
	SentinelChecker::SentinelChecker(boost::shared_ptr<ALBroker> broker, const string& name) : ALModule(broker,name), shmGameController(0), shmMotion(0),
																							   fCallbackMutex(ALMutex::createALMutex()), isGettingUp(false)
	{
		setModuleDescription("This module is used to manages some task for the NAO.");
		
		functionName("onSimpleClickOccured",getName(),"Method called when the chest button is pressed.");
		BIND_METHOD(SentinelChecker::onSimpleClickOccured)
		
		functionName("onDoubleClickOccured",getName(),"Method called when the chest button is pressed.");
		BIND_METHOD(SentinelChecker::onDoubleClickOccured)
		
		functionName("onTripleClickOccured",getName(),"Method called when the chest button is pressed.");
		BIND_METHOD(SentinelChecker::onTripleClickOccured)
		
		functionName("onLeftBumperPressed",getName(),"Method called when the right bumper is pressed.");
		BIND_METHOD(SentinelChecker::onLeftBumperPressed)
		
		functionName("onRightBumperPressed",getName(),"Method called when the right bumper is pressed.");
		BIND_METHOD(SentinelChecker::onRightBumperPressed)
	}
	
	SentinelChecker::~SentinelChecker()
	{
		/** Unsubscribe from an event */
		fMemoryProxy.unsubscribeToEvent("onSimpleClickOccured","SentinelChecker");
		fMemoryProxy.unsubscribeToEvent("onDoubleClickOccured","SentinelChecker");
		fMemoryProxy.unsubscribeToEvent("onTripleClickOccured","SentinelChecker");
		fMemoryProxy.unsubscribeToEvent("onLeftBumperPressed","SentinelChecker");
		fMemoryProxy.unsubscribeToEvent("onRightBumperPressed","SentinelChecker");
		
		if (shmMotion != 0) delete shmMotion;
		if (shmGameController != 0) delete shmGameController;
	}
	
	void SentinelChecker::init()
	{
		pthread_t getupStatusThreadId, getupThreadId, startingAgentThreadId, stiffnessThreadId;
		
		try
		{
			/** Create a proxy */
			fMemoryProxy = ALMemoryProxy(getParentBroker());
			memoryProxy = getParentBroker()->getMemoryProxy();
			motionProxy = getParentBroker()->getMotionProxy();
			fAudioDeviceProxy = ALAudioDeviceProxy(getParentBroker());
			
			/** Disable the default actions of the Chest Button */
			fSentinelProxy.enableDefaultActionDoubleClick(false);
			fSentinelProxy.enableDefaultActionSimpleClick(false);
			fSentinelProxy.enableDefaultActionTripleClick(false);
			
			/** Retrive the data from Memory. */
			fStateSimple = fMemoryProxy.getData("ALSentinel/SimpleClickOccured");
			fStateDouble = fMemoryProxy.getData("ALSentinel/DoubleClickOccured");
			fStateTriple = fMemoryProxy.getData("ALSentinel/TripleClickOccured");
			//fHead = fMemoryProxy.getData("MiddleTactilTouched");
			
			/** Subscribe to event
			*	Arguments:
			*		- name of the event
			*		- name of the module to be called for the callback
			*		- name of the bound method to be called on event
			*/
			fMemoryProxy.subscribeToEvent("ALSentinel/SimpleClickOccured","SentinelChecker","onSimpleClickOccured");
			fMemoryProxy.subscribeToEvent("ALSentinel/DoubleClickOccured","SentinelChecker","onDoubleClickOccured");
			fMemoryProxy.subscribeToEvent("ALSentinel/TripleClickOccured","SentinelChecker","onTripleClickOccured");
			//fMemoryProxy.subscribeToEvent("MiddleTactilTouched","SentinelChecker","onSimpleClickOccured");
			fMemoryProxy.subscribeToEvent("LeftBumperPressed","SentinelChecker","onLeftBumperPressed");
			fMemoryProxy.subscribeToEvent("RightBumperPressed","SentinelChecker","onRightBumperPressed");
			
			shmMotion = new SharedMemory(SharedMemory::MOTION,sizeof(RdkMotionData));
			
			//if (shmMotion->pluggingSuccessful()) cout << "Succesfully plugged in Shared Memory for RdkMotionData" << endl;
			//else throw ALError(getName(),"SentinelChecker()","Cannot create Shared Memory RdkMotionData. \nModule will abort.");
			
			//shmGameController = new SharedMemory(SharedMemory::GAMECONTROLLER,sizeof(RdkGameController));
			
			//if (shmGameController->pluggingSuccessful()) cout << "Succesfully plugged in Shared Memory for RdkGameController" << endl;
			//else throw ALError(getName(),"SentinelChecker()","Cannot create Shared Memory RdkGameController. \nModule will abort.");
			
			/** Setting the volume. */
			fAudioDeviceProxy.setOutputVolume(90);
			
			/** Stiffness manager when the robot is falling down and check if get up is needed */
			//pthread_create(&stiffnessThreadId,0,(void*(*)(void*)) zeroStiffnessThreadFn,this);
			pthread_create(&getupThreadId,0,(void*(*)(void*)) getupThreadFn,this);
			pthread_create(&getupStatusThreadId,0,(void*(*)(void*)) getupStatusUpdaterThreadFn,this);
			
			/** Automatic startup of ragent2 */
			//pthread_create(&startingAgentThreadId,0,(void*(*)(void*)) startingAgentThreadFn,this);
			
			loadGetupConfigurations();
			setRobotName();
		}
		catch (const ALError& e)
		{
			qiLogError("SentinelChecker") << e.what() << endl;
		}
	}
	
	void SentinelChecker::onSimpleClickOccured()
	{
		qiLogInfo("SentinelChecker") << "Executing callback method on ALSentinel/SimpleClickOccured event" << endl;
		
		/**
		* As long as this is defined, the code is thread-safe.
		*/
		ALCriticalSection section(fCallbackMutex);
		
		/**
		* Check that the ChestButton is pressed.
		*/
		fStateSimple =  fMemoryProxy.getData("ALSentinel/SimpleClickOccured");
		
		if (!fStateSimple) return;
		
		try
		{
			const vector<float>& stiffnessesJoints = motionProxy->getStiffnesses("Body");
			
			isSit = true;
			
			for (vector<float>::const_iterator it = stiffnessesJoints.begin(); it != stiffnessesJoints.end(); it++)
			{
				if (*it != 0)
				{
					isSit = false;
					
					break;
				}
			}
			
// 			RdkGameController* gameController = static_cast<RdkGameController*>(shmGameController->getEntry());
			
			if (isSit)
			{
				fTtsProxy = ALTextToSpeechProxy(getParentBroker());
				fTtsProxy.post.say("Standing on my own two feet.");
				
// 				strcpy(gameController->state,"play");
				
				stand();
			}
			else
			{
				fTtsProxy = ALTextToSpeechProxy(getParentBroker());
				fTtsProxy.post.say("Sitting on my ass.");
				
// 				strcpy(gameController->state,"penalized");
				
				sit();
			}
		}
		catch (const ALError& e)
		{
			qiLogError("SentinelChecker") << e.what() << endl;
		}
	}
	
	void SentinelChecker::onDoubleClickOccured()
	{
		qiLogInfo("SentinelChecker") << "Executing callback method on ALSentinel/DoubleClickOccured event" << endl;
		
		/**
		* As long as this is defined, the code is thread-safe.
		*/
		ALCriticalSection section(fCallbackMutex);
		
		/**
		* Check that the DoubleClick is pressed.
		*/
		fStateDouble = fMemoryProxy.getData("ALSentinel/DoubleClickOccured");
		
		if (!fStateDouble) return;
		
		try
		{
			fTtsProxy = ALTextToSpeechProxy(getParentBroker());
			fTtsProxy.post.say("Stiffness removed.");
			
			motionProxy->stiffnessInterpolation("Body",0.0,1.0);
		}
		catch (const ALError& e)
		{
			qiLogError("SentinelChecker") << e.what() << endl;
		}
	}
	
	void SentinelChecker::onTripleClickOccured()
	{
		qiLogInfo("SentinelChecker") << "Executing callback method on ALSentinel/TripleClickOccured event" << endl;
		
		/**
		* As long as this is defined, the code is thread-safe.
		*/
		ALCriticalSection section(fCallbackMutex);
		
		ostringstream textToSay;
		
		/**
		* Check that the TripleClick is pressed.
		*/
		fStateTriple = fMemoryProxy.getData("ALSentinel/TripleClickOccured");
		
		if (!fStateTriple) return;
		
		try
		{ 
			// Makes the volume at maximum 100 %.
			fAudioDeviceProxy.setOutputVolume(100);
			
			// This tells the IP and the battery level.
			fSentinelProxy.presentation(true);
			
			fStateBatteryLevel = fMemoryProxy.getData("Device/SubDeviceList/Battery/Charge/Sensor/Value");
			
			textToSay.clear();
			textToSay.str("");
			textToSay << string("at ") << (fStateBatteryLevel * 100) << string(" percent");
			
			fTtsProxy = ALTextToSpeechProxy(getParentBroker());
			fTtsProxy.say(textToSay.str());
			
			// Makes the volume at 65 %.
			fAudioDeviceProxy.setOutputVolume(65);
		}
		catch (const ALError& e)
		{
			qiLogError("SentinelChecker") << e.what() << endl;
		}
	}
	
	void SentinelChecker::onLeftBumperPressed()
	{
		qiLogInfo("SentinelChecker") << "Executing callback method on left bumper event" << std::endl;
		
		/**
		* As long as this is defined, the code is thread-safe.
		*/
		ALCriticalSection section(fCallbackMutex);
		
		/**
		* Check that the bumper is pressed.
		*/
		fState =  fMemoryProxy.getData("LeftBumperPressed");
		
		if (fState > 0.5)
		{
// 			RdkGameController* gameController = static_cast<RdkGameController*>(shmGameController->getEntry());
			
// 			if (strcmp(gameController->teamColor,"blue") == 0) strcpy(gameController->teamColor,"red");
// 			else strcpy(gameController->teamColor,"blue");
		}
	}
	
	void SentinelChecker::onRightBumperPressed()
	{
		qiLogInfo("SentinelChecker") << "Executing callback method on right bumper event" << std::endl;
		
		/**
		* As long as this is defined, the code is thread-safe.
		*/
		ALCriticalSection section(fCallbackMutex);
		
		/**
		* Check that the bumper is pressed.
		*/
		fState =  fMemoryProxy.getData("RightBumperPressed");
		
		if (fState > 0.5)
		{
// 			RdkGameController* gameController = static_cast<RdkGameController*>(shmGameController->getEntry());
			
// 			gameController->isKickOff = !gameController->isKickOff;
		}
	}
	
	void SentinelChecker::removeStiffnessThreadFn()
	{
		while (true) 
		{
			if (!isGettingUp)
			{
				angleX = memoryProxy->getData("Device/SubDeviceList/InertialSensor/AngleX/Sensor/Value",0);
				angleY = memoryProxy->getData("Device/SubDeviceList/InertialSensor/AngleY/Sensor/Value",0);
				
				// For front and back.
				if ((angleY > PROPERTY_REMOVE_STIFFNESS_Y) || (angleY < -PROPERTY_REMOVE_STIFFNESS_Y))
				{
					motionProxy->stiffnessInterpolation("Body",0.0,0.01);
				}
				else if ((angleX > PROPERTY_REMOVE_STIFFNESS_X) || (angleX < -PROPERTY_REMOVE_STIFFNESS_X))
				{
					motionProxy->stiffnessInterpolation("Body",0.0,0.01);
				}
			}
			
			usleep(20e3);
		}
	}

	void SentinelChecker::setJoints(const string& jointsString) const
	{
		ALValue jointsNames, jointsValues;
		
		stringstream jointsStringStream;
		string joint;
		double value;
		
		jointsStringStream << jointsString;
		
		while (!jointsStringStream.eof())
		{
			jointsStringStream >> joint >> value;
			
			jointsNames.arrayPush(joint);
			jointsValues.arrayPush(value);
		}
		
		motionProxy->angleInterpolationWithSpeed(jointsNames,jointsValues,0.3);
	}
	
	void SentinelChecker::sit() const
	{
		setJoints("LShoulderPitch 1.84536 \
				   LShoulderRoll 0.164096 \
				   LElbowYaw -1.47115 \
				   LElbowRoll -0.57214 \
				   LHipYawPitch -0.176368 \
				   LHipRoll 0.00310996 \
				   LHipPitch -0.774628 \
				   LKneePitch 2.11255 \
				   LAnklePitch -1.18944 \
				   LAnkleRoll 0.030722 \
				   RHipYawPitch -0.176368 \
				   RHipRoll 0.01078 \
				   RHipPitch -0.791586 \
				   RKneePitch 2.11255 \
				   RAnklePitch -1.1863 \
				   RAnkleRoll -0.044444 \
				   RShoulderPitch 1.80556 \
				   RShoulderRoll -0.19486 \
				   RElbowYaw 1.54776 \
				   RElbowRoll 0.517");
		
		usleep(1e6);
		
		motionProxy->stiffnessInterpolation("Body",0.0,1.0);
	}
	
	void SentinelChecker::stand() const
	{
		motionProxy->stiffnessInterpolation("Body",1.0,1.0);
		
		setJoints("LShoulderPitch 1.84 \
				   LShoulderRoll 0.199378 \
				   LElbowYaw -1.47882 \
				   LElbowRoll -0.546062 \
				   LHipYawPitch -0.00149204 \
				   LHipRoll 0.013848 \
				   LHipPitch -0.397264 \
				   LKneePitch 0.944902 \
				   LAnklePitch -0.54768 \
				   LAnkleRoll -0.00762804 \
				   RHipYawPitch -0.00149204 \
				   RHipRoll 0.01078 \
				   RHipPitch -0.397348 \
				   RKneePitch 0.943452 \
				   RAnklePitch -0.553732 \
				   RAnkleRoll -0.013764 \
				   RShoulderPitch 1.84 \
				   RShoulderRoll -0.214802 \
				   RElbowYaw 1.48027 \
				   RElbowRoll 0.490922");
	}
	
	void SentinelChecker::getup(const vector<vector<float> >& jointsValues, const vector<float>& jointsDurations)
	{
		for (unsigned int i = 0; i < jointsValues.size(); i++)
		{
			// Is is a blocking call.
			motionProxy->angleInterpolation("Body",jointsValues.at(i),jointsDurations.at(i),true);
		}
	}
	
	void SentinelChecker::getupNeededThreadFn()
	{
		/** The file is created inside nao path: /var/volatile/tmp/naoqi-* (this star is an alpha-numeric representing the last naoqi session) **/	
		string ragent = "isRAgentRunning.txt";
		string naoConsole = "isNaoConsoleRunning.txt";
		ifstream file, file1;
		char buffer[4096];
		
		while (true)
		{
			// Just to suppress the compilation warning.
			if (system((string("ps -a | grep ragent2 | awk \'{print $4}\'") +  string(" > ") + ragent.c_str()).c_str()));
			if (system((string("ps -a | grep NaoConsole | awk \'{print $4}\'") +  string(" > ") + naoConsole.c_str()).c_str()));
			  
			file.open(ragent.c_str());
			file1.open(naoConsole.c_str());
			
			while (file.good())
			{
				if (file.eof()) break;
				
				file.getline(buffer,4096);
				
				if (strcasecmp(buffer,"ragent2") == 0) 
				{
					angleX = memoryProxy->getData("Device/SubDeviceList/InertialSensor/AngleX/Sensor/Value",0);
					angleY = memoryProxy->getData("Device/SubDeviceList/InertialSensor/AngleY/Sensor/Value",0);
					
					isGettingUp = false;
					
					if (angleY > PROPERTY_GET_UP_Y)
					{
						const pair<vector<vector<float> >,vector<float> >& getupConfiguration = getupMap.at(FaceDown);
						
						isGettingUp = true;
						
						usleep(200e3);
						
						motionProxy->stiffnessInterpolation("Body",0.8,0.01);
						
						usleep(8e6);
						
// 						getup(getupConfiguration.first,getupConfiguration.second);
					}
					else if (angleY < -PROPERTY_GET_UP_Y)
					{
						const pair<vector<vector<float> >,vector<float> >& getupConfiguration = getupMap.at(FaceUp);
						
						isGettingUp = true;
						
						usleep(200e3);
						
						motionProxy->stiffnessInterpolation("Body",0.8,0.01);
						
						usleep(8e6);
						
// 						getup(getupConfiguration.first,getupConfiguration.second);
					}
					else if (angleX > PROPERTY_GET_UP_X)
					{
						const pair<vector<vector<float> >,vector<float> >& getupConfiguration = getupMap.at(OnSideRight);
						
						isGettingUp = true;
						
						usleep(200e3);
						
						motionProxy->stiffnessInterpolation("Body",0.8,0.01);
						
						usleep(8e6);
						
// 						getup(getupConfiguration.first,getupConfiguration.second);
					}
					else if (angleX < -PROPERTY_GET_UP_X)
					{
						const pair<vector<vector<float> >,vector<float> >& getupConfiguration = getupMap.at(OnSideLeft);
						
						isGettingUp = true;
						
						usleep(200e3);
						
						motionProxy->stiffnessInterpolation("Body",0.8,0.01);
						
						usleep(8e6);
						
// 						getup(getupConfiguration.first,getupConfiguration.second);
					}
				}
			}
			
			while (file1.good())
			{
				if (file1.eof()) break;
				
				file1.getline(buffer,4096);
				
				if (strcasecmp(buffer,"NaoConsole") == 0) 
				{
					angleX = memoryProxy->getData("Device/SubDeviceList/InertialSensor/AngleX/Sensor/Value",0);
					angleY = memoryProxy->getData("Device/SubDeviceList/InertialSensor/AngleY/Sensor/Value",0);
					
					isGettingUp = false;
					
					if (angleY > PROPERTY_GET_UP_Y)
					{
						const pair<vector<vector<float> >,vector<float> >& getupConfiguration = getupMap.at(FaceDown);
						
						isGettingUp = true;
						
						usleep(200e3);
						
						motionProxy->stiffnessInterpolation("Body",0.8,0.01);
						
						usleep(8e6);
						
// 						getup(getupConfiguration.first,getupConfiguration.second);
					}
					else if (angleY < -PROPERTY_GET_UP_Y)
					{
						const pair<vector<vector<float> >,vector<float> >& getupConfiguration = getupMap.at(FaceUp);
						
						isGettingUp = true;
						
						usleep(200e3);
						
						motionProxy->stiffnessInterpolation("Body",0.8,0.01);
						
						usleep(8e6);
						
// 						getup(getupConfiguration.first,getupConfiguration.second);
					}
					else if (angleX > PROPERTY_GET_UP_X)
					{
						const pair<vector<vector<float> >,vector<float> >& getupConfiguration = getupMap.at(OnSideRight);
						
						isGettingUp = true;
						
						usleep(200e3);
						
						motionProxy->stiffnessInterpolation("Body",0.8,0.01);
						
						usleep(8e6);
						
// 						getup(getupConfiguration.first,getupConfiguration.second);
					}
					else if (angleX < -PROPERTY_GET_UP_X)
					{
						const pair<vector<vector<float> >,vector<float> >& getupConfiguration = getupMap.at(OnSideLeft);
						
						isGettingUp = true;
						
						usleep(200e3);
						
						motionProxy->stiffnessInterpolation("Body",0.8,0.01);
						
						usleep(8e6);
						
// 						getup(getupConfiguration.first,getupConfiguration.second);
					}
				}
			}
			
			file.close();
			file1.close();
			
			usleep(400e3);
		}
	}
	
	void SentinelChecker::loadGetupConfigurations()
	{
		try
		{
			getupMap.insert(make_pair(FaceDown,FileDatReader::readForAngleInterpolation("/home/nao/naoqi-configurations-files/getup/FaceDown")));
		}
		catch (runtime_error)
		{
			cerr << "File not found [/home/nao/naoqi-configurations-files/getup/FaceDown.dat]!" << endl;
		}
		
		try
		{
			getupMap.insert(make_pair(FaceUp,FileDatReader::readForAngleInterpolation("/home/nao/naoqi-configurations-files/getup/FaceUp")));
		}
		catch (runtime_error)
		{
			cerr << "File not found [/home/nao/naoqi-configurations-files/getup/FaceUp.dat]!" << endl;
		}
		
		try
		{
			getupMap.insert(make_pair(OnSideLeft,FileDatReader::readForAngleInterpolation("/home/nao/naoqi-configurations-files/getup/OnSideLeft")));
		}
		catch (runtime_error)
		{
			cerr << "File not found [/home/nao/naoqi-configurations-files/getup/OnSideLeft.dat]!" << endl;
		}
		
		try
		{
			getupMap.insert(make_pair(OnSideRight,FileDatReader::readForAngleInterpolation("/home/nao/naoqi-configurations-files/getup/OnSideRight")));
		}
		catch (runtime_error)
		{
			cerr << "File not found [/home/nao/naoqi-configurations-files/getup/OnSideRight.dat]!" << endl;
		}
	}
	
	// Start remove.
	void SentinelChecker::setRobotName()
	{
		ifstream f;
		char buffer[4096];
		
		f.open("/etc/hostname");
		
		if (f.good())
		{
			f.getline(buffer,4096);
			
			robotName = buffer;
		}
		
		f.close();
	}
	// End remove.
	
	void SentinelChecker::dummyThreadFn()
	{ 
		usleep(5e6);

		// Just to suppress the compilation warning.
		if (system((string("/home/nao/naoqi-configurations-files/dummy.sh")).c_str()));
	}
	
	void SentinelChecker::updateGetupStatus() const
	{
		while (true)
		{
			shmMotion->wait(NAOQI);
			
			RdkMotionData* motionData = static_cast<RdkMotionData*>(shmMotion->getEntry());
			
			motionData->isGettingUp = isGettingUp;
			
			shmMotion->signal(RDK);
		}
	}
}
