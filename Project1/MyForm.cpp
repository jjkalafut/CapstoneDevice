#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "MyForm.h"
#include "rtpsession.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtppacket.h"
#include "Audioclient.h"
#include "Audiopolicy.h"
#include "Mmdeviceapi.h"
#ifndef WIN32
	#include <netinet/in.h>
	#include <arpa/inet.h>
#else
	#include <winsock2.h>
#endif // WIN32
#include "..\..\CapstoneHeaders\pandaheader.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
using namespace Project1;
using namespace jrtplib;


// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
				if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
				if ((punk) != NULL)  \
								{ (punk)->Release(); (punk) = NULL; }

static RTPSession sess;
char device_name[30] = "Shannon's Laptop\0";
uint8_t p_control = 0;
bool stop_stream = true;
bool rec_init = false;
uint8_t my_device_idx;
RTPIPv4Address my_address;
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

//
// This function checks if there was a RTP error. If so, it displays an error
// message and exists.
//

void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		Sleep(4000);
		exit(-1);
	}
}

HRESULT LoadData(uint32_t len, uint8_t * data, DWORD * flags){
	printf("waiting for data... \n");
	sess.BeginDataAccess();
	if (sess.GotoFirstSourceWithData())
	{
		do{
			RTPPacket *pack;
			uint32_t buff_idx = 0;
			while ((pack = sess.GetNextPacket()) != NULL)
			{
				uint16_t header;
				header = pack->GetExtensionID();

				uint16_t idx = 0;
				uint8_t placed = FALSE;

				// You can examine the data here
				//printf("Got packet !\n");
				if (header == AUDIO){
					printf("Playing Data \n");
					pandaPacketData * pd = (pandaPacketData *)pack->GetPayloadData();
					while (buff_idx < len && idx < pd->data_len){
						memcpy(&data[buff_idx], &pd->data[idx], 1);
						idx++;
						buff_idx++;
						
					}
				}
				else{
					//printf(" got pack with diff header??");
				}
				if (buff_idx == len){
					sess.EndDataAccess();
					return 0;
				}
				// we don't longer need the packet, so
				// we'll delete it		
				sess.DeletePacket(pack);
			}
			*flags = AUDCLNT_BUFFERFLAGS_SILENT;
			sess.EndDataAccess();
			return 0;
		}while (sess.GotoNextSourceWithData());
	}
	else{
		sess.EndDataAccess();
		*flags = AUDCLNT_BUFFERFLAGS_SILENT;
		return 0;
	}
}
HRESULT PlayAudioStream()
{
	HRESULT hr;
	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	REFERENCE_TIME hnsActualDuration;
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDevice *pDevice = NULL;
	IAudioClient *pAudioClient = NULL;
	IAudioRenderClient *pRenderClient = NULL;
	WAVEFORMATEX *pwfx = NULL;
	UINT32 bufferFrameCount;
	UINT32 numFramesAvailable;
	UINT32 numFramesPadding;
	BYTE *pData;
	DWORD flags = 0;

	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);
	EXIT_ON_ERROR(hr)

		hr = pEnumerator->GetDefaultAudioEndpoint(
		eRender, eConsole, &pDevice);
	EXIT_ON_ERROR(hr)

		hr = pDevice->Activate(
		IID_IAudioClient, CLSCTX_ALL,
		NULL, (void**)&pAudioClient);
	EXIT_ON_ERROR(hr)

		hr = pAudioClient->GetMixFormat(&pwfx);
	EXIT_ON_ERROR(hr)

		hr = pAudioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		0,
		hnsRequestedDuration,
		0,
		pwfx,
		NULL);
	EXIT_ON_ERROR(hr)

		// Tell the audio source which format to use.
		//hr = pMySource->SetFormat(pwfx);
	//EXIT_ON_ERROR(hr)

		// Get the actual size of the allocated buffer.
		hr = pAudioClient->GetBufferSize(&bufferFrameCount);
	EXIT_ON_ERROR(hr)

		hr = pAudioClient->GetService(
		IID_IAudioRenderClient,
		(void**)&pRenderClient);
	EXIT_ON_ERROR(hr)

		// Grab the entire buffer for the initial fill operation.
		hr = pRenderClient->GetBuffer(bufferFrameCount, &pData);
	EXIT_ON_ERROR(hr)

		// Load the initial data into the shared buffer.
		hr = LoadData(bufferFrameCount, pData, &flags);
	EXIT_ON_ERROR(hr)

		hr = pRenderClient->ReleaseBuffer(bufferFrameCount, flags);
	EXIT_ON_ERROR(hr)

		// Calculate the actual duration of the allocated buffer.
		hnsActualDuration = (double)REFTIMES_PER_SEC *
		bufferFrameCount / pwfx->nSamplesPerSec;

	hr = pAudioClient->Start();  // Start playing.
	EXIT_ON_ERROR(hr)

		// Each loop fills about half of the shared buffer.
		while (flags != AUDCLNT_BUFFERFLAGS_SILENT)
		{
		// Sleep for half the buffer duration.
		Sleep((DWORD)(hnsActualDuration / REFTIMES_PER_MILLISEC / 2));

		// See how much buffer space is available.
		hr = pAudioClient->GetCurrentPadding(&numFramesPadding);
		EXIT_ON_ERROR(hr)

			numFramesAvailable = bufferFrameCount - numFramesPadding;

		// Grab all the available space in the shared buffer.
		hr = pRenderClient->GetBuffer(numFramesAvailable, &pData);
		EXIT_ON_ERROR(hr)

			// Get next 1/2-second of data from the audio source.
			hr = LoadData(numFramesAvailable, pData, &flags);
		EXIT_ON_ERROR(hr)

			hr = pRenderClient->ReleaseBuffer(numFramesAvailable, flags);
		EXIT_ON_ERROR(hr)
		}

	// Wait for last data in buffer to play before stopping.
	Sleep((DWORD)(hnsActualDuration / REFTIMES_PER_MILLISEC / 2));

	hr = pAudioClient->Stop();  // Stop playing.
	EXIT_ON_ERROR(hr)

	Exit:
	CoTaskMemFree(pwfx);
	SAFE_RELEASE(pEnumerator)
		SAFE_RELEASE(pDevice)
		SAFE_RELEASE(pAudioClient)
		SAFE_RELEASE(pRenderClient)

		return hr;
}
/********************************************************************
*						GET Packets to ID new Sources
*
*
*
********************************************************************/
void MyForm::pollPackets(){
	
	if (my_device_idx == NUM_MAX_CLIENTS){
		signupPacket sp;
		uint32_t addr = my_address.GetIP();
		memcpy(&sp.address, &addr, 4);
		strcpy_s(&sp.name[0], STR_BUFF_SZ, &device_name[0]);

		sess.SendPacketEx(&sp, sizeof(signupPacket), SET_DEV, 0, 0);
	}

	sess.BeginDataAccess();
	// check incoming packets
	if (sess.GotoFirstSourceWithData())
	{
		do
		{
			RTPPacket *pack;

			while ((pack = sess.GetNextPacket()) != NULL)
			{
				uint16_t header;
				header = pack->GetExtensionID();

				uint16_t idx = 0;
				uint8_t placed = FALSE;
				// You can examine the data here
				//printf("Got packet !\n");
				switch (header){
				case PANDA_ACK: { 
					memcpy_s(&my_device_idx, 1, pack->GetPayloadData(), 1); 
					printf("My idx is %u \n", my_device_idx);
					break; 
				}

				case PANDA_NACK: { printf(" NACK RECIEVED \n"); break; }
				case SET_ID:{
					memcpy(&my_device_idx, pack->GetPayloadData(), 1);
					break;
				}

				
				default: { printf("can't ID packet"); break; }
				}

				// we don't longer need the packet, so
				// we'll delete it				
				sess.DeletePacket(pack);
			}
		} while (sess.GotoNextSourceWithData());
	}

	sess.EndDataAccess();
}


//send audio packets to stream
void MyForm::stream(){
	if (!rec_init){
		CoInitialize(NULL);
	}
		//printf("called record stream");
	PlayAudioStream();
}
void end_stream(){
	if (stop_stream = false){
		stop_stream = true;
		CoUninitialize();
		rec_init = false;
	}
}
void MyForm::stop_stream(){
	end_stream();
}

void MyForm::set_address(uint32_t addr){
	//printf("setting IP to %i \n", addr);
	my_address.SetIP(addr);
}

[STAThreadAttribute]
int main(array<System::String ^> ^args)
{
#ifdef WIN32
	WSADATA dat;
	WSAStartup(MAKEWORD(2, 2), &dat);
#endif // WIN32

	uint16_t portbase, destport;
	uint32_t destip;
	int status;

	my_device_idx = NUM_MAX_CLIENTS; 
	

	// Enabling Windows XP visual effects before any controls are created
	//MessageBox::Show("Oppening a Port to Listen...");
	Application::EnableVisualStyles();
	Application::SetCompatibleTextRenderingDefault(false);

	//establish rtp protocols
	inet_pton(AF_INET, "192.168.1.5", &destip);
	destip = ntohl(destip);

	portbase = 6002;
	destport = 6000;

	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;

	sessparams.SetOwnTimestampUnit(1.0 / 10.0);
	sessparams.SetAcceptOwnPackets(true);
	transparams.SetPortbase(portbase);
	status = sess.Create(sessparams, &transparams);
	checkerror(status);
	sess.SetDefaultPayloadType(0);
	checkerror(status);
	sess.SetDefaultMark(0);
	checkerror(status);
	sess.SetDefaultTimestampIncrement(1);
	checkerror(status);

	RTPIPv4Address addr(destip, destport);

	status = sess.AddDestination(addr);
	checkerror(status);

	//status = send_upd_packet();
	//checkerror(status);



	//find all decives
	//status = ask_devices();
	//checkerror(status);

	// Create the main window and run it
	Application::Run(gcnew MyForm());

	sess.BYEDestroy(RTPTime(10, 0), 0, 0);

#ifdef WIN32
	WSACleanup();
#endif // WIN32
	return 0;
}