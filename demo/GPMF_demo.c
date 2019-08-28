/*! @file GPMF_demo.c
 *
 *  @brief Demo to extract GPMF from an MP4
 *
 *  @version 1.0.1
 *
 *  (C) Copyright 2017 GoPro Inc (http://gopro.com/).
 *
 *  Licensed under either:
 *  - Apache License, Version 2.0, http://www.apache.org/licenses/LICENSE-2.0  
 *  - MIT license, http://opensource.org/licenses/MIT
 *  at your option.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "../GPMF_common.h"
#include "../GPMF_writer.h"
#include "GPMF_parser.h"
#include "GPMF_mp4writer.h"

//#define REALTICK

#define ENABLE_SNR_A	0
#define ENABLE_SNR_B	0
#define ENABLE_SNR_C	0
#define ENABLE_SNR_D	0
#define ENABLE_SNR_DISP	1
#define ENABLE_SNR_GYRO	0
#define ENABLE_SNR_FACE	0
#define ENABLE_SNR_ISOE	1

#define DELAY_US 1000000

extern void PrintGPMF(GPMF_stream *);

#if !_WINDOWS
#define sprintf_s(a,b,c) sprintf(a,c)
#endif

#pragma pack(push)
#pragma pack(1)		//GPMF sensor data structures are always byte packed.

#if 0
typedef struct sensorAdata  // Example 10-byte pack structure.
{
	uint32_t flags;
	uint8_t ID[6];
} sensorAdata;
#else
typedef struct sensorAdata  // Example 10-byte pack structure.
{
	uint32_t FOURCC;
	float value;
} sensorAdata;
#endif

#pragma pack(pop)

int main(int argc, char *argv[])
{
	size_t gpmfhandle = 0;
	size_t mp4_handle = 0;
	int32_t ret = GPMF_OK;

	if (argc != 2)
	{
		printf("usage: %s <file_with_GPMF.MP4|MOV>\n", argv[0]);
		return -1;
	}

	srand(0);

	mp4_handle = OpenMP4Export(argv[1], 1000, 1001);

	gpmfhandle = GPMFWriteServiceInit();
	if (gpmfhandle && mp4_handle)
	{
		size_t handleT = 0;
		char buffer[6*8192];
		uint32_t *payload=NULL, payload_size=0, samples, i;
		uint32_t faketime,fakedata;
//		uint32_t tmp;
		uint32_t count = 0;
		uint8_t bdata[40] = { 0 };
		uint16_t sdata[40] = { 0 }, signal = 0;
		char txt[80];
		uint32_t err;
		sensorAdata Adata[10];

#if ENABLE_SNR_A
		size_t handleA = 0;
		char sensorA[4 * 8192];
		handleA = GPMFWriteStreamOpen(gpmfhandle, GPMF_CHANNEL_TIMED, GPMF_DEVICE_ID_CAMERA, "MyCamera", sensorA, sizeof(sensorA));
		if (handleA == 0) goto cleanup;
#endif
#if ENABLE_SNR_B
		size_t handleB = 0;
		char sensorB[2*4096];
		handleB = GPMFWriteStreamOpen(gpmfhandle, GPMF_CHANNEL_TIMED, GPMF_DEVICE_ID_CAMERA, "MyCamera", sensorB, sizeof(sensorB));
		if (handleB == 0) goto cleanup;
#endif
#if ENABLE_SNR_C
		size_t handleC = 0;
		char sensorC[4096];
		handleC = GPMFWriteStreamOpen(gpmfhandle, GPMF_CHANNEL_TIMED, GPMF_DEVICE_ID_CAMERA, "MyCamera", sensorC, sizeof(sensorC));
		if (handleC == 0) goto cleanup;
#endif
#if ENABLE_SNR_D
		size_t handleD = 0;
		char sensorD[4096];
		handleD = GPMFWriteStreamOpen(gpmfhandle, GPMF_CHANNEL_TIMED, GPMF_DEVICE_ID_CAMERA, "MyCamera", sensorD, sizeof(sensorD));
		if (handleD == 0) goto cleanup;
#endif
#if ENABLE_SNR_DISP
		size_t handleE = 0;
		char SensorDISP[24096];
		handleE = GPMFWriteStreamOpen(gpmfhandle, GPMF_CHANNEL_TIMED, GPMF_DEVICE_ID_CAMERA, "MyCamera", SensorDISP, sizeof(SensorDISP));
		if (handleE == 0) goto cleanup;
#endif
#if ENABLE_SNR_GYRO
		size_t handleGYRO = 0;
		char sensorGYRO[24096];
		handleGYRO = GPMFWriteStreamOpen(gpmfhandle, GPMF_CHANNEL_TIMED, GPMF_DEVICE_ID_CAMERA, "MyCamera", sensorGYRO, sizeof(sensorGYRO));
		if (handleGYRO == 0) goto cleanup;
#endif
#if ENABLE_SNR_FACE
		size_t handleFACE = 0;
		char sensorFACE[4096];
		handleFACE = GPMFWriteStreamOpen(gpmfhandle, GPMF_CHANNEL_TIMED, GPMF_DEVICE_ID_CAMERA, "MyCamera", sensorFACE, sizeof(sensorFACE));
		if (handleFACE == 0) goto cleanup;
#endif
#if ENABLE_SNR_ISOE
		size_t handleISOE = 0;
		char sensorISOE[4096];
		handleISOE = GPMFWriteStreamOpen(gpmfhandle, GPMF_CHANNEL_TIMED, GPMF_DEVICE_ID_CAMERA, "MyCamera", sensorISOE, sizeof(sensorISOE));
		if (handleISOE == 0) goto cleanup;
#endif

		char sensorT[4096];
		handleT = GPMFWriteStreamOpen(gpmfhandle, GPMF_CHANNEL_SETTINGS, GPMF_DEVICE_ID_CAMERA, "Global", sensorT, sizeof(sensorT));
		if (handleT == 0) goto cleanup;


		//Initialize sensor stream with any sticky data

#if ENABLE_SNR_A
		sprintf_s(txt, 80, "Sensor A");
   		GPMFWriteStreamStore(handleA, GPMF_KEY_STREAM_NAME, GPMF_TYPE_STRING_ASCII, (uint32_t)strlen(txt), 1, txt, GPMF_FLAGS_STICKY);
   		//sprintf_s(txt, 80, "LB[6]"); // matching sensorAdata
		sprintf_s(txt, 80, "Ff"); // matching sensorAdata
		GPMFWriteStreamStore(handleA, GPMF_KEY_TYPE, GPMF_TYPE_STRING_ASCII, (uint32_t)strlen(txt), 1, &txt, GPMF_FLAGS_STICKY);
#endif

#if ENABLE_SNR_B
		sprintf_s(txt, 80, "Sensor B");
		GPMFWriteStreamStore(handleB, GPMF_KEY_STREAM_NAME, GPMF_TYPE_STRING_ASCII, (uint32_t)strlen(txt), 1, txt, GPMF_FLAGS_STICKY);
		//tmp = 555;
		//GPMFWriteStreamStore(handleB, GPMF_KEY_SCALE, GPMF_TYPE_UNSIGNED_LONG, sizeof(tmp), 1, &tmp, GPMF_FLAGS_STICKY);
		//fdata[0] = 123.456f; fdata[1] = 74.56f; fdata[2] = 98.76f;
		//GPMFWriteStreamStore(handleB, STR2FOURCC("MyCC"), GPMF_TYPE_FLOAT, sizeof(float), 3, fdata, GPMF_FLAGS_STICKY);
#endif

#if ENABLE_SNR_C
		sprintf_s(txt, 80, "Sensor C");
		//	sprintf_s(txt, 80, "Sensor C - Compressed");
		GPMFWriteStreamStore(handleC, GPMF_KEY_STREAM_NAME, GPMF_TYPE_STRING_ASCII, (uint32_t)strlen(txt), 1, txt, GPMF_FLAGS_STICKY);
		//	tmp = 1; // quantize by a larger number for more compress, use 1 for lossless (but it may not compress much.)//
		//	GPMFWriteStreamStore(handleC, GPMF_KEY_QUANTIZE, GPMF_TYPE_UNSIGNED_LONG, sizeof(tmp), 1, &tmp, GPMF_FLAGS_STICKY);
#endif

#if ENABLE_SNR_D
		sprintf_s(txt, 80, "Sensor D");
		//	sprintf_s(txt, 80, "Sensor C - Compressed");
		GPMFWriteStreamStore(handleD, GPMF_KEY_STREAM_NAME, GPMF_TYPE_STRING_ASCII, (uint32_t)strlen(txt), 1, txt, GPMF_FLAGS_STICKY);
		//	tmp = 1; // quantize by a larger number for more compress, use 1 for lossless (but it may not compress much.)//
		//	GPMFWriteStreamStore(handleC, GPMF_KEY_QUANTIZE, GPMF_TYPE_UNSIGNED_LONG, sizeof(tmp), 1, &tmp, GPMF_FLAGS_STICKY);
#endif
#if ENABLE_SNR_DISP
		sprintf_s(txt, 80, "Sensor DISP");
		//	sprintf_s(txt, 80, "Sensor C - Compressed");
		GPMFWriteStreamStore(handleE, GPMF_KEY_STREAM_NAME, GPMF_TYPE_STRING_ASCII, (uint32_t)strlen(txt), 1, txt, GPMF_FLAGS_STICKY);
		uint32_t tmp = 1; // quantize by a larger number for more compress, use 1 for lossless (but it may not compress much.)//
		GPMFWriteStreamStore(handleE, GPMF_KEY_QUANTIZE, GPMF_TYPE_UNSIGNED_LONG, sizeof(tmp), 1, &tmp, GPMF_FLAGS_STICKY);
#endif
#if ENABLE_SNR_GYRO
		sprintf_s(txt, 80, "Sensor GYRO");
		GPMFWriteStreamStore(handleGYRO, GPMF_KEY_STREAM_NAME, GPMF_TYPE_STRING_ASCII, (uint32_t)strlen(txt), 1, txt, GPMF_FLAGS_STICKY);
#endif
#if ENABLE_SNR_FACE
		sprintf_s(txt, 80, "Sensor FACE");
		GPMFWriteStreamStore(handleFACE, GPMF_KEY_STREAM_NAME, GPMF_TYPE_STRING_ASCII, (uint32_t)strlen(txt), 1, txt, GPMF_FLAGS_STICKY);
#endif
#if ENABLE_SNR_ISOE
		sprintf_s(txt, 80, "Sensor ISOE");
		GPMFWriteStreamStore(handleISOE, GPMF_KEY_STREAM_NAME, GPMF_TYPE_STRING_ASCII, (uint32_t)strlen(txt), 1, txt, GPMF_FLAGS_STICKY);
#endif

		//Flush any stale data before starting video capture.
		GPMFWriteGetPayload(gpmfhandle, GPMF_CHANNEL_TIMED, (uint32_t *)buffer, sizeof(buffer), &payload, &payload_size);


		uint32_t val[8] = { 0x12345678, 1, 2, 3, 4, 5, 6, 7 };


		GPMFWriteStreamStore(handleT, STR2FOURCC("FMWR"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 15,
			(void *)"HD?.xx.xx.xx", GPMF_FLAGS_NONE);

		/*lens info*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("LINF"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 48,
			(void *)"Lens info                                       ", GPMF_FLAGS_NONE);

		/*camera info*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("CINF"), GPMF_TYPE_UNSIGNED_BYTE,
			sizeof(uint8_t), 16,
			(void *)&val[0], GPMF_FLAGS_NONE);

		/*Camera Serial Number*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("CASN"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 15,
			(void *)"casn stuff                                       ", GPMF_FLAGS_NONE);

		/*Model info*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("MINF"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 30,
			(void *)"minf stuff                                       ", GPMF_FLAGS_NONE);

		/*muid*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("MUID"), GPMF_TYPE_UNSIGNED_LONG,
			sizeof(uint32_t), 8,
			(void *)val, GPMF_FLAGS_NONE);
		
		/*Camera flat mode*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("CMOD"), GPMF_TYPE_UNSIGNED_BYTE,
			sizeof(uint8_t), 1,
			(void *)&val[0], GPMF_FLAGS_NONE);

		/*Media type*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("MTYP"), GPMF_TYPE_UNSIGNED_BYTE,
			sizeof(uint8_t), 1,
			(void *)&val[0], GPMF_FLAGS_NONE);

		/*Orientation*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("OREN"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 1,
			(void *)"U", GPMF_FLAGS_NONE);

		/*Digital zoom enable*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("DZOM"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 1,
			(void *)"O", GPMF_FLAGS_NONE);

		/*Digital zoom setting*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("DZST"), GPMF_TYPE_UNSIGNED_LONG,
			sizeof(uint32_t), 1,
			(void *)&val[0], GPMF_FLAGS_NONE);

		/*spot meter*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("SMTR"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 1,
			(void *)"P", GPMF_FLAGS_NONE);

		/*protune*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("PRTN"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 1,
			(void *)"T", GPMF_FLAGS_NONE);

		/*protune white balance*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("PTWB"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 8,
			(void *)&val[0], GPMF_FLAGS_NONE);

		/*protune sharpness*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("PTSH"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 6,
			(void *)"ptsh  ", GPMF_FLAGS_NONE);

		/*protune color*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("PTCL"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 6,
			(void *)"ptcl  ", GPMF_FLAGS_NONE);

		/*exposure time*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("EXPT"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 10,
			(void *)"expt stuff", GPMF_FLAGS_NONE);

		/*protune ISO Max*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("PIMX"), GPMF_TYPE_UNSIGNED_LONG,
			sizeof(uint32_t), 1,
			(void *)&val[0], GPMF_FLAGS_NONE);

		/*protune ISO Min*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("PIMN"), GPMF_TYPE_UNSIGNED_LONG,
			sizeof(uint32_t), 1,
			(void *)&val[0], GPMF_FLAGS_NONE);

		/*protune EV*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("PTEV"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 6,
			(void *)"ptev  ", GPMF_FLAGS_NONE);

		/*rate*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("RATE"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 8,
			(void *)"rate ", GPMF_FLAGS_NONE);

		/*photo resolution*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("PRES"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 8,
			(void *)"pres  ", GPMF_FLAGS_NONE);

		/*photo Force HDR ON, Super Photo (MFNR, LTM,  Normal Still, HDR*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("PHDR"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 8,
			(void *)"phdr  ", GPMF_FLAGS_NONE);

		/*photo RAW*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("PRAW"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 1,
			(void *)"r", GPMF_FLAGS_NONE);

		/*photo highlight*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("HFLG"), GPMF_TYPE_UNSIGNED_BYTE,
			sizeof(uint8_t), 1,
			(void *)&val[0], GPMF_FLAGS_NONE);

		/*Preview lens*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("PVUL"), GPMF_TYPE_STRING_ASCII,
			sizeof(uint8_t), 8,
			(void *)"pvul  ", GPMF_FLAGS_NONE);

		/*Shutter offset*/
		GPMFWriteStreamStore(handleT, STR2FOURCC("SOFF"), GPMF_TYPE_UNSIGNED_LONG,
			sizeof(uint32_t), 1,
			(void *)&val[0], GPMF_FLAGS_NONE);


		GPMFWriteStreamStore(handleT, STR2FOURCC("accm"), GPMF_TYPE_UNSIGNED_LONG,
			sizeof(uint32_t), 1,
			(void *)&val[1], GPMF_FLAGS_STICKY_ACCUMULATE);

		GPMFWriteStreamStore(handleT, STR2FOURCC("accm"), GPMF_TYPE_UNSIGNED_LONG,
			sizeof(uint32_t), 1,
			(void *)&val[1], GPMF_FLAGS_STICKY_ACCUMULATE);

		GPMFWriteStreamStore(handleT, STR2FOURCC("accm"), GPMF_TYPE_UNSIGNED_LONG,
			sizeof(uint32_t), 1,
			(void *)&val[1], GPMF_FLAGS_STICKY_ACCUMULATE);

		GPMFWriteStreamStore(handleT, STR2FOURCC("accm"), GPMF_TYPE_UNSIGNED_LONG,
			sizeof(uint32_t), 1,
			(void *)&val[1], GPMF_FLAGS_STICKY_ACCUMULATE);


		//Flush any stale data before starting video capture.
		GPMFWriteGetPayload(gpmfhandle, GPMF_CHANNEL_SETTINGS, (uint32_t *)buffer, sizeof(buffer), &payload, &payload_size);


		uint64_t tick = 111, firsttick, payloadtick, nowtick;
		uint64_t timestamp = 111;
#ifdef REALTICK
		LARGE_INTEGER tt;
		QueryPerformanceCounter(&tt);
		firsttick = tick = tt.QuadPart;
#else
		firsttick = tick;
#endif
		nowtick = tick;

		int scen_samples = 0;
		int gyro_samples = 0;

		for (faketime = 0; faketime < 25; faketime++)
		{
			uint32_t delta[10] = { 33366, 33367, 33367, 33366, 33367, 33367, 33366, 33367, 33367, 33366 };
			uint32_t data_per = 30;// +(rand() & 1);

			payloadtick = tick;
			for (fakedata = 0; fakedata < data_per; fakedata++)
			{
				int sensor = rand() & 3;
#ifdef REALTICK
				QueryPerformanceCounter(&tt);
				tick = tt.QuadPart;
#endif
				//sensor = 1;
				//sensor = 2;

				sensor = 0;// fakedata & 1;
				switch(sensor)
				{
					case 0: //pretend no data	
					{
						short sdata[100],k;
						long ldata[5];
						static uint32_t count = 0, isocount = 0,gps = 0;

#if ENABLE_SNR_B
						int smps = 1;// (rand() % 10) + 15;
						for (k = 0; k < smps; k++)
						{
							sdata[k * 3 + 0] = count;
							sdata[k * 3 + 1] = count;
							sdata[k * 3 + 2] = count++;
						}
						//timestamp = 67000 + tick + (rand() % 10) * 100;
						//if (count < 250 || count > 849)
							//if (count > 1000 && count < 350) k = 0; 
						err = GPMFWriteStreamStoreStamped(handleB, STR2FOURCC("GYRO"), GPMF_TYPE_UNSIGNED_SHORT, sizeof(uint16_t) * 3, k, sdata, GPMF_FLAGS_NONE, timestamp);
#endif

#if ENABLE_SNR_D
						if ((fakedata % 2) == 0)
						{
							ldata[0] = gps;
							ldata[1] = gps;
							ldata[2] = gps;
							ldata[3] = gps;
							ldata[4] = gps++;

							//timestamp = 67000 + tick + (rand() % 10) * 100;
							//if (count < 250 || count > 849)
								//if (count > 1000 && count < 350) k = 0; 

							static uint64_t lastts = 0;
							int64_t sts = (int64_t)timestamp;
							int tsoffset = ((rand() & 0x7fff) - 16383) * 2;
							//int tsoffset = ((rand() & 0x7fff) - 16383) / 10;
							if (sts + tsoffset < 0)
								sts = 100;
							else
								sts += tsoffset;


							uint64_t newts = sts;
							if (newts <= lastts)
								newts = lastts + 100;
						
							lastts = newts;

							err = GPMFWriteStreamStoreStamped(handleD, STR2FOURCC("GPS5"), GPMF_TYPE_SIGNED_LONG, sizeof(uint32_t) * 1, 1, ldata, GPMF_FLAGS_NONE, newts);
						}
#endif


#if ENABLE_SNR_DISP
						uint8_t disp[640];
						static uint8_t val = 0;
						if ((fakedata & 1) == 0)
						{
							for (k = 0; k < 640; k++)
							{
								if ((rand() & 7) == 0) val++;
								disp[k] = val;
							}
							err = GPMFWriteStreamStoreStamped(handleE, STR2FOURCC("DISP"), GPMF_TYPE_UNSIGNED_BYTE, sizeof(uint8_t), 640, disp, GPMF_FLAGS_GROUPED, timestamp);
						}
#endif

#if ENABLE_SNR_GYRO
						short gyro[20*3];
						{
							static short val = 0;
							{
								uint64_t ltimestamp = timestamp;
								int randoff = 30000 + (rand() & 0x3ff) - 0x1ff;

								for (k = 0; k < 5; k++)
								{
									gyro[k * 3 + 0] = val;
									gyro[k * 3 + 1] = val;
									gyro[k * 3 + 2] = val++;
								}
								ltimestamp = timestamp + randoff;

								//if(fakedata == faketime && fakedata == 0)
								//	err = GPMFWriteStreamStoreStamped(handleGYRO, STR2FOURCC("GYRO"), GPMF_TYPE_SIGNED_SHORT, sizeof(uint16_t) * 3, 20, gyro, GPMF_FLAGS_NONE, timestamp-1);
								err = GPMFWriteStreamStoreStamped(handleGYRO, STR2FOURCC("GYRO"), GPMF_TYPE_SIGNED_SHORT, sizeof(uint16_t) * 3, 5, gyro, GPMF_FLAGS_NONE, ltimestamp);
							}
						}
#endif

#if ENABLE_SNR_FACE
						short face[20 * 3] = { 0 };
						if(timestamp > (DELAY_US-66667))
							err = GPMFWriteStreamStoreStamped(handleFACE, STR2FOURCC("FACE"), GPMF_TYPE_SIGNED_SHORT, sizeof(uint16_t), 0, face, GPMF_FLAGS_GROUPED, timestamp- (DELAY_US - 66667));
#endif
#if ENABLE_SNR_ISOE
						static unsigned short isoe = 100;
						err = GPMFWriteStreamStoreStamped(handleISOE, STR2FOURCC("ISOE"), GPMF_TYPE_SIGNED_SHORT, sizeof(uint16_t), 1, &isoe, GPMF_FLAGS_NONE, timestamp);
						isoe++;
#endif
						{
							float fcount = (float)(count) / 100.0f;
							err = 0;
							//samples = 1 + (rand() % 3); //1-3 values
							//samples = 2;
							samples = 6;

							//SNOW,0.14, URBA,0.27, INDO,0.30, WATR,0.13, VEGE,0.08, BEAC,0.08
							//for (i = 0; i < samples; i++)

#if ENABLE_SNR_A 
							{
								Adata[0].FOURCC = STR2FOURCC("SNOW");
								Adata[0].value = fcount;
								Adata[1].FOURCC = STR2FOURCC("URBA");
								Adata[1].value = fcount;
								Adata[2].FOURCC = STR2FOURCC("INDO");
								Adata[2].value = fcount;
								Adata[3].FOURCC = STR2FOURCC("WATR");
								Adata[3].value = fcount;
								Adata[4].FOURCC = STR2FOURCC("VEGE"); 
								Adata[4].value = fcount;
								Adata[5].FOURCC = STR2FOURCC("BEAC");
								Adata[5].value = fcount;
							}
							scen_samples++;

							if (scen_samples < 15 || scen_samples > 47)
							{
								if (scen_samples < 5 || (scen_samples > 45 && scen_samples < 50) || (scen_samples > 55 && scen_samples < 58)) samples = 0; else count++;
								err = GPMFWriteStreamStoreStamped(handleA, STR2FOURCC("SnrA"), GPMF_TYPE_COMPLEX, sizeof(sensorAdata), samples, Adata, GPMF_FLAGS_GROUPED, timestamp);
							}
#endif


#if ENABLE_SNR_C
							uint8_t bval = rand();
							err = GPMFWriteStreamStoreStamped(handleC, STR2FOURCC("CTRS"), GPMF_TYPE_UNSIGNED_BYTE, 1, 1, &bval, GPMF_FLAGS_NONE, tick); bval = rand();
							err = GPMFWriteStreamStoreStamped(handleC, STR2FOURCC("SHRP"), GPMF_TYPE_UNSIGNED_BYTE, 1, 1, &bval, GPMF_FLAGS_NONE, tick); bval = rand();
							err = GPMFWriteStreamStoreStamped(handleC, STR2FOURCC("MOTN"), GPMF_TYPE_UNSIGNED_BYTE, 1, 1, &bval, GPMF_FLAGS_NONE, tick); bval = rand();
							err = GPMFWriteStreamStoreStamped(handleC, STR2FOURCC("3BDH"), GPMF_TYPE_UNSIGNED_BYTE, 1, 1, &bval, GPMF_FLAGS_NONE, tick); bval = rand();
							err = GPMFWriteStreamStoreStamped(handleC, STR2FOURCC("3BDV"), GPMF_TYPE_UNSIGNED_BYTE, 1, 1, &bval, GPMF_FLAGS_NONE, tick);
#endif
						}


						//if (faketime == 0 && fakedata == 3) {
						//	GPMFWriteFlushWindow(gpmfhandle, GPMF_CHANNEL_TIMED, 33367*3); // Flush partial second
						//}

					}
						break;
/*
					case 1: //pretend Sensor A data
#if ENABLE_SNR_A
						//samples = 1 + (rand() % 3); //1-3 values
						//samples = 2;
						samples = 1;
						for (i = 0; i < samples; i++)
						{
							Adata[i].flags = count++;
							Adata[i].ID[0] = 1;
							Adata[i].ID[1] = 2;
							Adata[i].ID[2] = 3;
							Adata[i].ID[3] = 4;
							Adata[i].ID[4] = 5;
							Adata[i].ID[5] = 6;
						}
						//err = GPMFWriteStreamStoreStamped(handleA, STR2FOURCC("SnrA"), GPMF_TYPE_COMPLEX, sizeof(sensorAdata), samples, Adata, GPMF_FLAGS_NONE, tick);
						err = GPMFWriteStreamStoreStamped(handleA, STR2FOURCC("SnrA"), GPMF_TYPE_COMPLEX, sizeof(sensorAdata), samples, Adata, GPMF_FLAGS_NONE|GPMF_FLAGS_STORE_ALL_TIMESTAMPS, tick);
						//err = GPMFWriteStreamStore(handleA, STR2FOURCC("SnrA"), GPMF_TYPE_COMPLEX, sizeof(sensorAdata), samples, Adata, GPMF_FLAGS_NONE);
						if (err)
						{
							printf("err = %d\n", err);
						}
#endif
						break;*/

					case 1: //pretend Sensor A data
#if ENABLE_SNR_A
					{
						uint64_t ltime = tick + 100; // .1 second delayed
						float count = (float)(tick - 1000) / 100.0f;
						err = 0;
						//samples = 1 + (rand() % 3); //1-3 values
						//samples = 2;
						samples = 6;

						//SNOW,0.14, URBA,0.27, INDO,0.30, WATR,0.13, VEGE,0.08, BEAC,0.08
						//for (i = 0; i < samples; i++)
						{
							Adata[0].FOURCC = STR2FOURCC("SNOW");
							Adata[0].value = count;
							Adata[1].FOURCC = STR2FOURCC("URBA");
							Adata[1].value = count;
							Adata[2].FOURCC = STR2FOURCC("INDO");
							Adata[2].value = count;
							Adata[3].FOURCC = STR2FOURCC("WATR");
							Adata[3].value = count;
							Adata[4].FOURCC = STR2FOURCC("VEGE");
							Adata[4].value = count;
							Adata[5].FOURCC = STR2FOURCC("BEAC");
							Adata[5].value = count;
						}

						err = GPMFWriteStreamStoreStamped(handleA, STR2FOURCC("SnrA"), GPMF_TYPE_COMPLEX, sizeof(sensorAdata), samples, Adata, GPMF_FLAGS_GROUPED | GPMF_FLAGS_STORE_ALL_TIMESTAMPS, ltime);
					
						if (err)
						{
							printf("err = %d\n", err);
						}
					}
#endif
					break;

					case 2: //pretend Sensor B data
#if ENABLE_SNR_B
						{
							static uint16_t scount = 1;
							//samples = 0 + (rand() % 4); //0-3 values
							samples = 1 + (rand() % 3); //1-3 values
							//samples = 1;
							for (i = 0; i < (int)samples; i++) sdata[i] = scount;// (uint32_t)rand() & 0xffffff;
							scount++;
							//err = GPMFWriteStreamStoreStamped(handleB, STR2FOURCC("SnrB"), GPMF_TYPE_UNSIGNED_SHORT, sizeof(uint16_t), samples, sdata, GPMF_FLAGS_NONE, tick);
							err = GPMFWriteStreamStoreStamped(handleB, STR2FOURCC("SnrB"), GPMF_TYPE_UNSIGNED_SHORT, sizeof(uint16_t), samples, sdata, GPMF_FLAGS_GROUPED, tick);
							//err = GPMFWriteStreamStoreStamped(handleB, STR2FOURCC("SnrB"), GPMF_TYPE_UNSIGNED_SHORT, sizeof(uint16_t), samples, sdata, GPMF_FLAGS_STORE_ALL_TIMESTAMPS, tick);
							//err = GPMFWriteStreamStore(handleB, STR2FOURCC("SnrB"), GPMF_TYPE_UNSIGNED_SHORT, sizeof(uint16_t), samples, sdata, GPMF_FLAGS_NONE);
							//err = GPMFWriteStreamStore(handleB, STR2FOURCC("SnrB"), GPMF_TYPE_UNSIGNED_SHORT, sizeof(uint16_t), samples, sdata, GPMF_FLAGS_GROUPED);
							if (err)
							{
								printf("err = %d\n", err);
							}
						}
#endif
						break;

					case 3: //pretend Sensor C data, high frequency, demoing compression
#if ENABLE_SNR_C
						samples = 10 + (rand() % 30); //10-40 values
						for (i = 0; i < samples; i++) { sdata[i] = signal + (uint16_t)(rand() & 0x7); signal++; } // signal and noise
						//err = GPMFWriteStreamStoreStamped(handleC, STR2FOURCC("SnrC"), GPMF_TYPE_UNSIGNED_SHORT, sizeof(uint16_t), samples, sdata, GPMF_FLAGS_NONE, tick);
						err = GPMFWriteStreamStore(handleC, STR2FOURCC("SnrC"), GPMF_TYPE_UNSIGNED_SHORT, sizeof(uint16_t), samples, sdata, GPMF_FLAGS_NONE);
						if (err)
						{
							printf("err = %d\n", err);
						}
#endif
						break;
				}
#ifndef REALTICK
				//tick += samples * 10;
				//tick += 100 + (rand() & 17);

				uint64_t lastts = timestamp;
				//timestamp += delta[fakedata%10];

				//if ((rand() % 50) == 1)
				//	timestamp += 5000;

				/*static int inc = 1;
				if ((rand() % 15) == 1)
				{
					if ((timestamp-lastts) < 100100)
						inc++;
					else
						inc--;
				}
				timestamp += inc;
				*/

				timestamp = tick = payloadtick + (fakedata + 1) * 1000000 / data_per;


			//	timestamp = lastts + 33366;// +(rand() % 21) - 10;
			//	tick += 33366;

				//
				//	tick += 9;
#else
				Sleep(2 * samples); // << to help test the time stamps.
#endif
			}

			//nowtick = payloadtick + (tick - payloadtick) * 8 / 10; // test by reading out only the last half samples
			nowtick = tick - DELAY_US;
		//	nowtick = tick - 1000110; // test by reading out only the last half samples
		//	nowtick = tick - (rand()%100000); // test by reading out only the last half samples
			//nowtick += 100; // test by reading out only the last half samples
			if (nowtick > 1000000)
		//	if (nowtick > 20000)
			{
				//if(nowtick < 1500000)
				//	GPMFWriteFlushWindow(gpmfhandle, GPMF_CHANNEL_TIMED, 34000); // Flush partial second

				//GPMFWriteGetPayloadWindow(gpmfhandle, GPMF_CHANNEL_TIMED, (uint32_t *)buffer, sizeof(buffer), &payload, &payload_size, nowtick);
				GPMFWriteGetPayloadAndSession(gpmfhandle, GPMF_CHANNEL_TIMED, (uint32_t *)buffer, sizeof(buffer), NULL, NULL, &payload, &payload_size, 1, nowtick);
				//GPMFWriteGetPayloadAndSession(gpmfhandle, GPMF_CHANNEL_TIMED, (uint32_t *)buffer, sizeof(buffer), NULL, NULL, &payload, &payload_size, 5, nowtick);
				//GPMFWriteGetPayload(gpmfhandle, GPMF_CHANNEL_TIMED, (uint32_t *)buffer, sizeof(buffer), &payload, &payload_size);

				printf("payload_size = %d\n", payload_size);
				ExportPayload(mp4_handle, payload, payload_size);
			}
//			else
//			{
//				GPMFWriteFlushWindow(gpmfhandle, GPMF_CHANNEL_TIMED, nowtick+50000); // Flush partial second
//			}
	/*
			GPMFWriteGetPayloadWindow(gpmfhandle, GPMF_CHANNEL_TIMED, (uint32_t *)buffer, sizeof(buffer), &payload, &payload_size, nowtick+1);

			printf("payload_size = %d\n", payload_size);
			ExportPayload(mp4_handle, payload, payload_size);
			*/
			//Using the GPMF_Parser, output some of the contents
		/*	GPMF_stream gs;
			if (GPMF_OK == GPMF_Init(&gs, payload, payload_size))
			{
				GPMF_ResetState(&gs);
				do
				{ 
					PrintGPMF(&gs);  // printf current GPMF KLV
				} while (GPMF_OK == GPMF_Next(&gs, GPMF_RECURSE_LEVELS));
			}
			printf("\n");
		*/
		}

	cleanup:

		if (mp4_handle) CloseExport(mp4_handle); 
#if ENABLE_SNR_A
		if (handleA) GPMFWriteStreamClose(handleA);
#endif
#if ENABLE_SNR_B
		if (handleB) GPMFWriteStreamClose(handleB);
#endif

		GPMFWriteServiceClose(gpmfhandle);
	}


	return ret;
}
