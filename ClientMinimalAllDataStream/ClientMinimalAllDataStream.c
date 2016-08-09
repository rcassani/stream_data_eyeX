/*
* This is an executable that obtains Fixations, Gaze point and eye Position information from the eyeX hardware and send this information to
* remote (or local) server using TCP/IP connection. The program is based in the example MinimalGazeDataStream example provided by Tobii
*
* Raymundo Cassani @ MuSAE Lab, INRS-EMT Canada
* raymundo.cassani@gmail.com
* 2016
*
* Usage:
* ./ClientMinimalAllDataStream SERVER_IP PORT
* e.g. :
* ./ClientMinimalAllDataStream 127.0.0.1 35000

##################################################
###############  Original Header #################
##################################################
* This an example that demonstrates how to connect to the EyeX Engine and subscribe to the fixation data stream.
*
* Copyright 2013-2014 Tobii Technology AB. All rights reserved.
# # # # # # # # # # # # # # # # # # # # # # # # #
*/

#ifndef WIN32_LEAN_AND_MEAN		//TCP
#define WIN32_LEAN_AND_MEAN     //TCP
#endif                          //TCP

/* 
If an #include line is needed for the Windows.h header file, this should be preceded with the
#define WIN32_LEAN_AND_MEAN macro. For historical reasons, the Windows.h header defaults to including
the Winsock.h header file for Windows Sockets 1.1.
The declarations in the Winsock.h header file will conflict with the declarations in the
Winsock2.h header file required by Windows Sockets 2.0.
The WIN32_LEAN_AND_MEAN macro prevents the Winsock.h from being included by the Windows.h header
http://msdn.microsoft.com/en-us/library/windows/desktop/ms737629%28v=vs.85%29.aspx 
*/


#include <Windows.h>    //EyeX
#include <winsock2.h>   //TCP
#include <ws2tcpip.h>   //TCP
#include <stdlib.h>     //TCP
#include <stdio.h>      //EyeX
#include <conio.h>      //EyeX
#include <assert.h>     //EyeX
#include "eyex\EyeX.h"  //EyeX

#pragma comment (lib, "Tobii.EyeX.Client.lib") //EyeX
#pragma comment(lib, "Ws2_32.lib")             //TCP
#pragma comment (lib, "Mswsock.lib")           //TCP
#pragma comment (lib, "AdvApi32.lib")          //TCP

// ID of the global interactor that provides our data stream; must be unique within the application.
static const TX_STRING InteractorId = "MuSAE Lab";

// global variables
static TX_HANDLE g_hGlobalInteractorSnapshot = TX_EMPTY_HANDLE;

SOCKET clientSocket = INVALID_SOCKET;         //TCP
BOOL streaming = FALSE;

/*
 * Returns a Socket object, which is the ClientSocket already connected to the specified Server ip:port
 */
SOCKET OpenClientSocket(char *ip, char* port)
{
	// Creating Socket
	WSADATA wsaData;        //structure is used to store Windows Sockets initialization information		
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		//printf("Error at WSAStartup(), error %d\n", iResult); 
		return INVALID_SOCKET;
	}
	struct addrinfo *result = NULL, *ptr = NULL, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo(ip, port, &hints, &result);
	if (iResult != 0)
	{
		//printf("Error at getaddrinfo(), error %d\n", iResult); 
		WSACleanup();
		return INVALID_SOCKET;
	}

	ptr = result;
	SOCKET theSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

	if (theSocket == INVALID_SOCKET)
	{
		//printf("Error at socket(), error %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return INVALID_SOCKET;
	}

	// Connect to the server
	iResult = connect(theSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		closesocket(theSocket);
		theSocket = INVALID_SOCKET;
	}
	freeaddrinfo(result);
	if (theSocket == INVALID_SOCKET)
	{
		printf("Unable to connect to server at %s : %s\n", ip, port);
		WSACleanup();
		return INVALID_SOCKET;
	}

	printf("Successful connection with server at %s : %s\n", ip, port);
	return theSocket;
}

/*
 * Closes the provided Socket
 */
void CloseClientSocket(SOCKET theSocket)
{
	closesocket(theSocket);
	WSACleanup();
}

/*
 * Formats an Float (32bits) from host to TCP/IP network byte order (which is big-endian)
 * and sends it thought the Socket
 */
void SendFloatClientSocket(SOCKET theSocket, float value)
{
	int htonvalue[] = { htonf(value) };
	send(theSocket, (char*)htonvalue, sizeof(int), 0);
}

/*
 * Initializes g_hGlobalInteractorSnapshot with an interactor that has the Fixation, Gaze and Position Data behavior.
 */
BOOL InitializeGlobalInteractorSnapshot(TX_CONTEXTHANDLE hContext)
{
	TX_HANDLE hInteractor = TX_EMPTY_HANDLE;
	TX_HANDLE hBehavior = TX_EMPTY_HANDLE;
	TX_FIXATIONDATAPARAMS paramsF = { TX_FIXATIONDATAMODE_SENSITIVE };
	TX_GAZEPOINTDATAPARAMS paramsG = { TX_GAZEPOINTDATAMODE_LIGHTLYFILTERED };
	
	BOOL success;

	success = txCreateGlobalInteractorSnapshot(
		hContext,
		InteractorId,
		&g_hGlobalInteractorSnapshot,
		&hInteractor) == TX_RESULT_OK;
	success &= txCreateFixationDataBehavior(hInteractor, &paramsF) == TX_RESULT_OK;
	success &= txCreateGazePointDataBehavior(hInteractor, &paramsG) == TX_RESULT_OK;
	success &= txCreateInteractorBehavior(hInteractor, &hBehavior, TX_BEHAVIORTYPE_EYEPOSITIONDATA) == TX_RESULT_OK;

	txReleaseObject(&hBehavior);
	txReleaseObject(&hInteractor);

	return success;
}

/*
 * Callback function invoked when a snapshot has been committed.
 */
void TX_CALLCONVENTION OnSnapshotCommitted(TX_CONSTHANDLE hAsyncData, TX_USERPARAM param)
{
	// check the result code using an assertion.
	// this will catch validation errors and runtime errors in debug builds. in release builds it won't do anything.

	TX_RESULT result = TX_RESULT_UNKNOWN;
	txGetAsyncDataResultCode(hAsyncData, &result);
	assert(result == TX_RESULT_OK || result == TX_RESULT_CANCELLED);
}

/*
 * Callback function invoked when the status of the connection to the EyeX Engine has changed.
 */
void TX_CALLCONVENTION OnEngineConnectionStateChanged(TX_CONNECTIONSTATE connectionState, TX_USERPARAM userParam)
{
	switch (connectionState) {
	case TX_CONNECTIONSTATE_CONNECTED: {
			BOOL success;
			printf("The connection state is now CONNECTED (We are connected to the EyeX Engine)\n");
			// commit the snapshot with the global interactor as soon as the connection to the engine is established.
			// (it cannot be done earlier because committing means "send to the engine".)
			success = txCommitSnapshotAsync(g_hGlobalInteractorSnapshot, OnSnapshotCommitted, NULL) == TX_RESULT_OK;
			if (!success) {
				printf("Failed to initialize the data stream.\n");
			}
			else
			{
				printf("Waiting for fixation data to start streaming...\n");
			}
		}
		break;

	case TX_CONNECTIONSTATE_DISCONNECTED:
		printf("The connection state is now DISCONNECTED (We are disconnected from the EyeX Engine)\n");
		break;

	case TX_CONNECTIONSTATE_TRYINGTOCONNECT:
		printf("The connection state is now TRYINGTOCONNECT (We are trying to connect to the EyeX Engine)\n");
		break;

	case TX_CONNECTIONSTATE_SERVERVERSIONTOOLOW:
		printf("The connection state is now SERVER_VERSION_TOO_LOW: this application requires a more recent version of the EyeX Engine to run.\n");
		break;

	case TX_CONNECTIONSTATE_SERVERVERSIONTOOHIGH:
		printf("The connection state is now SERVER_VERSION_TOO_HIGH: this application requires an older version of the EyeX Engine to run.\n");
		break;
	}
}

/*
 * Handles an event from the fixation data stream.
 */
void OnFixationDataEvent(TX_HANDLE hFixationDataBehavior)
{
	float data[6] =   { 0, 0, 0, 0, 0, 0 };
	int iResult;

	TX_FIXATIONDATAEVENTPARAMS eventParams;
	TX_FIXATIONDATAEVENTTYPE eventType;
	char* eventDescription;

	if (txGetFixationDataEventParams(hFixationDataBehavior, &eventParams) == TX_RESULT_OK) {
		/* Every time there is an event, send a package*/
		//TCP	Preparing data package = #bytes, BehaviorType, Fixation_EventType, X, Y, Timestamp
		
		data[0] = 20; // Number of following bytes (5 values * 4 bytes / value)
		data[1] = eventParams.Timestamp; // Seconds since the System Boot Time
		data[2] = 7;  // Behavior Type, 7 for Fixation

		eventType = eventParams.EventType;
		if (eventType == TX_FIXATIONDATAEVENTTYPE_DATA)   { data[3] = 3; }
		if (eventType == TX_FIXATIONDATAEVENTTYPE_END)    { data[3] = 2; }
		if (eventType == TX_FIXATIONDATAEVENTTYPE_BEGIN)  { data[3] = 1; }
			
		data[4] = eventParams.X;
		data[5] = eventParams.Y;

		eventDescription = (eventType == TX_FIXATIONDATAEVENTTYPE_DATA) ? "Data"
			: ((eventType == TX_FIXATIONDATAEVENTTYPE_END) ? "End"
				: "Begin");
		printf("Fixation %s: (%.1f, %.1f) time %.0f ms\n", eventDescription, eventParams.X, eventParams.Y, eventParams.Timestamp);

		if (streaming)
		{
			for (int a = 0; a < 6; a++) //Up tp a < number of elements
			{
				SendFloatClientSocket(clientSocket, data[a]);
			}
		}
	} 
	else 
	{
		printf("Failed to interpret fixation data event packet.\n");
	}
}

/*
* Handles an event from the Gaze Point data stream.
*/
void OnGazeDataEvent(TX_HANDLE hGazeDataBehavior)
{
	float data[5]   = { 0, 0, 0, 0, 0 };
	int iResult;

	TX_GAZEPOINTDATAEVENTPARAMS eventParams;
	if (txGetGazePointDataEventParams(hGazeDataBehavior, &eventParams) == TX_RESULT_OK) 
	{
		/* Every time there is an event, send a package*/
		//TCP	Preparing data package = #bytes, BehaviorType, X, Y, Timestamp
		data[0] = 16; // Number of following bytes (4 values * 4 bytes / value)
		data[1] = eventParams.Timestamp; // Seconds since the System Boot Time
		data[2] = 1;  // Behavior Type, 1 for Gaze
		data[3] = eventParams.X;
		data[4] = eventParams.Y;

		printf("Gaze Data: (%.1f, %.1f) time %.0f ms\n", eventParams.X, eventParams.Y, eventParams.Timestamp);

		if (streaming)
		{
			for (int a = 0; a < 5; a++) //Up tp a < number of elements
			{
				SendFloatClientSocket(clientSocket, data[a]);
			}
		}
	}
	else 
	{
		printf("Failed to interpret gaze data event packet.\n");
	}
}

/*
* Handles an event from the Gaze Point data stream.
*/
void OnPositionDataEvent(TX_HANDLE hGazeDataBehavior)
{	
	float data[9]   = { 0, 0, 0, 0, 0, 0, 0, 0 ,0 };
	int iResult;

	TX_EYEPOSITIONDATAEVENTPARAMS eventParams;
	if (txGetEyePositionDataEventParams(hGazeDataBehavior, &eventParams) == TX_RESULT_OK) 
	{
		/* Every time there is an event, send a package*/			
		//TCP	Preparing data package = #bytes, BehaviorType, XL, YL, ZL, XR, YR, ZR, Timestamp
		data[0] = 32; // Number of following bytes (8 values * 4 bytes / value)
		data[1] = eventParams.Timestamp; // Seconds since the System Boot Time
		data[2] = 3;  // Behavior Type, 3 for Eye Position
		data[3] = eventParams.LeftEyeX;
		data[4] = eventParams.LeftEyeY;
		data[5] = eventParams.LeftEyeZ;
		data[6] = eventParams.RightEyeX;
		data[7] = eventParams.RightEyeY;
		data[8] = eventParams.RightEyeZ;
		
		printf("Eye Position: L(%.1f, %.1f, %.1f), R(%.1f, %.1f, %.1f), time %.0f ms\n", eventParams.LeftEyeX, eventParams.LeftEyeY, eventParams.LeftEyeZ, eventParams.RightEyeX, eventParams.RightEyeY, eventParams.RightEyeZ, eventParams.Timestamp);

		if (streaming)
		{
			for (int a = 0; a < 9; a++) //Up tp a < number of elements
			{
				SendFloatClientSocket(clientSocket, data[a]);
			}
		}	
	}
	else 
	{
		printf("Failed to interpret gaze data event packet.\n");	
	}
}

/*
 * Callback function invoked when an event has been received from the EyeX Engine.
 */
void TX_CALLCONVENTION HandleEvent(TX_CONSTHANDLE hAsyncData, TX_USERPARAM userParam)
{
	TX_HANDLE hEvent = TX_EMPTY_HANDLE;
	TX_HANDLE hBehavior = TX_EMPTY_HANDLE;

    txGetAsyncDataContent(hAsyncData, &hEvent);

	// NOTE. Uncomment the following line of code to view the event object. The same function can be used with any interaction object.
	//OutputDebugStringA(txDebugObject(hEvent));
	
	if (txGetEventBehavior(hEvent, &hBehavior, TX_BEHAVIORTYPE_FIXATIONDATA) == TX_RESULT_OK) {
		OnFixationDataEvent(hBehavior);
		txReleaseObject(&hBehavior);
	}

	if (txGetEventBehavior(hEvent, &hBehavior, TX_BEHAVIORTYPE_GAZEPOINTDATA) == TX_RESULT_OK) {
		OnGazeDataEvent(hBehavior);
		txReleaseObject(&hBehavior);
	}
	
	if (txGetEventBehavior(hEvent, &hBehavior, TX_BEHAVIORTYPE_EYEPOSITIONDATA) == TX_RESULT_OK) {
		OnPositionDataEvent(hBehavior);
		txReleaseObject(&hBehavior);
	}


	// NOTE since this is a very simple application with a single interactor and a single data stream, 
	// our event handling code can be very simple too. A more complex application would typically have to 
	// check for multiple behaviors and route events based on interactor IDs.
	
	txReleaseObject(&hEvent);
}

/*
 * Application entry point.
 */
int main(int argc, char* argv[])
{
	char *ip = NULL;
	char *port = NULL;
	int iResult = 0;

	system("cls");

	// Check input arguments for IP and PORT and open client socket if required
	if (argc != 3)
	{
		printf("Usage: %s <IP> <PORT>\n", argv[0]);
		printf("EyeX data will not be streamed\n");
	}
	else
	{
		ip = argv[1];
		port = argv[2];
		printf("Trying connection with %s : %s\n", ip, port);
		clientSocket = OpenClientSocket(ip, port);
		if (clientSocket == INVALID_SOCKET)
		{
			printf("Error at connection with %s : %s\n", ip, port);
			printf("EyeX data will not be streamed\n");
		}
		else
		{
			streaming = TRUE;
		}
	}
	Sleep(2000);

	TX_CONTEXTHANDLE hContext = TX_EMPTY_HANDLE;
	TX_TICKET hConnectionStateChangedTicket = TX_INVALID_TICKET;
	TX_TICKET hEventHandlerTicket = TX_INVALID_TICKET;
	BOOL success;

	// initialize and enable the context that is our link to the EyeX Engine.
	success = txInitializeEyeX(TX_EYEXCOMPONENTOVERRIDEFLAG_NONE, NULL, NULL, NULL, NULL) == TX_RESULT_OK;
	success &= txCreateContext(&hContext, TX_FALSE) == TX_RESULT_OK;
	success &= InitializeGlobalInteractorSnapshot(hContext);
	success &= txRegisterConnectionStateChangedHandler(hContext, &hConnectionStateChangedTicket, OnEngineConnectionStateChanged, NULL) == TX_RESULT_OK;
	success &= txRegisterEventHandler(hContext, &hEventHandlerTicket, HandleEvent, NULL) == TX_RESULT_OK;
	success &= txEnableConnection(hContext) == TX_RESULT_OK;

	// let the events flow until a key is pressed.
	if (success) {
		printf("Initialization was successful.\n");
	} else {
		printf("Initialization failed.\n");
	}
	printf("Press X to exit...\n");
	_getch();
	printf("Exiting.\n");

	// disable and delete the context.
	txDisableConnection(hContext);
	txReleaseObject(&g_hGlobalInteractorSnapshot);
	txShutdownContext(hContext, TX_CLEANUPTIMEOUT_DEFAULT, TX_FALSE);
	txReleaseContext(&hContext);

	if (streaming)
	{
		SendFloatClientSocket(clientSocket, -1);
		CloseClientSocket(clientSocket);
	}

	Beep(300, 750);
	return 0;
}