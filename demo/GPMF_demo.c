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

#if !_WINDOWS
#define sprintf_s(a,b,c) sprintf(a,c)
#endif

#pragma pack(push)
#pragma pack(1)		//GPMF sensor data structures are always byte packed.

typedef struct sensorAdata  // Example 10-byte pack structure.
{
	uint32_t flags;
	uint8_t ID[6];
} sensorAdata;

#pragma pack(pop)

int main(int argc, char *argv[])
{
	size_t gpmfhandle = 0;
	int32_t ret = GPMF_OK;
	uint32_t *payload = NULL; //buffer to store GPMF samples from the MP4.

	//if (argc != 2)
	//{
	//	printf("usage: %s <file_with_GPMF>\n", argv[0]);
	//	return -1;
	//}

	if (gpmfhandle = GPMFWriteServiceInit())
	{
		size_t handleA;
		size_t handleB;
		char buffer[8192];
		char sensorA[4096];
		uint32_t *payload, payload_size, samples, i;
		uint32_t tmp,faketime,fakedata;
		float fdata[4];
		uint32_t Ldata[4], count = 0;
		char txt[80];
		uint32_t err;
		sensorAdata Adata[4];

		handleA = GPMFWriteStreamOpen(gpmfhandle, GPMF_CHANNEL_TIMED, GPMF_DEVICE_ID_CAMERA, "MyCamera", sensorA, sizeof(sensorA));
		if (handleA == 0) goto cleanup;

		handleB = GPMFWriteStreamOpen(gpmfhandle, GPMF_CHANNEL_TIMED, GPMF_DEVICE_ID_CAMERA, "MyCamera", NULL, 0);
		if (handleB == 0) goto cleanup;

		//Initialize sensor stream with any sticky data
		sprintf_s(txt, 80, "Sensor A");
		GPMFWriteStreamStore(handleA, GPMF_KEY_STREAM_NAME, GPMF_TYPE_STRING_ASCII, strlen(txt), 1, &txt, GPMF_FLAGS_STICKY);
		sprintf_s(txt, 80, "LB[6]"); // matching sensorAdata
		GPMFWriteStreamStore(handleA, GPMF_KEY_TYPE, GPMF_TYPE_STRING_ASCII, strlen(txt), 1, &txt, GPMF_FLAGS_STICKY);


		sprintf_s(txt, 80, "Sensor B");
		GPMFWriteStreamStore(handleB, GPMF_KEY_STREAM_NAME, GPMF_TYPE_STRING_ASCII, strlen(txt), 1, &txt, GPMF_FLAGS_STICKY);
		tmp = 555;
		GPMFWriteStreamStore(handleB, GPMF_KEY_SCALE, GPMF_TYPE_UNSIGNED_LONG, sizeof(tmp), 1,  &tmp, GPMF_FLAGS_STICKY);
		fdata[0] = 123.456f; fdata[1] = 74.56f; fdata[2] = 98.76f;
		GPMFWriteStreamStore(handleB, STR2FOURCC("MyCC"), GPMF_TYPE_FLOAT, sizeof(float), 3, fdata, GPMF_FLAGS_STICKY);


		//Flush any stale data before starting video capture.
		GPMFWriteGetPayload(gpmfhandle, GPMF_CHANNEL_TIMED, (uint32_t *)buffer, sizeof(buffer), &payload, &payload_size);

		for (faketime = 0; faketime < 10; faketime++)
		{
			for (fakedata = 0; fakedata < 50; fakedata++)
			{
				switch(rand() & 3)
				{
					case 0: //pretend no data
						break;
					case 1: //pretend Sensor A data
					case 2: //pretend Sensor A data
						samples = 1 + (rand() % 3); //1-4 values
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
						err = GPMFWriteStreamStore(handleA, STR2FOURCC("SnrA"), GPMF_TYPE_COMPLEX, sizeof(sensorAdata), samples, Adata, GPMF_FLAGS_NONE);
						if (err) printf("err = %d\n", err);
						break;
					case 3: //pretend Sensor B data
						samples = 1 + (rand() % 3); //1-4 values
						for (i = 0; i < samples; i++) Ldata[i] = (uint32_t)rand() & 0xffffff;
						err = GPMFWriteStreamStore(handleB, STR2FOURCC("SnrB"), GPMF_TYPE_UNSIGNED_LONG, sizeof(uint32_t), samples, Ldata, GPMF_FLAGS_NONE);
						if (err) printf("err = %d\n", err); 
						break;
				}
			}
			GPMFWriteGetPayload(gpmfhandle, GPMF_CHANNEL_TIMED, (uint32_t *)buffer, sizeof(buffer), &payload, &payload_size);

			printf("payload_size = %d\n", payload_size);

			//Using the GPMF_Parser, output some of the contents
			GPMF_stream gs;
			if (GPMF_OK == GPMF_Init(&gs, payload, payload_size))
			{
				GPMF_ResetState(&gs);
				do
				{ 
					extern void PrintGPMF(GPMF_stream *);
					PrintGPMF(&gs);  // printf current GPMF KLV
				} while (GPMF_OK == GPMF_Next(&gs, GPMF_RECURSE_LEVELS));
			}
			printf("\n");

		}

cleanup:
		GPMFWriteStreamClose(handleA);
		GPMFWriteStreamClose(handleB);
	

		GPMFWriteServiceClose(gpmfhandle);
	}


	return ret;
}
