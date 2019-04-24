/*! @file mp4writer.c
*
*  @brief Way-way Too Crude MP4|MOV writer (just to demo GPMF with a MP4/MOV file)
*
*  @version 1.0.0
*
*  (C) Copyright 2019 GoPro Inc (http://gopro.com/).
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*
*/

/* This is not a hack writer for MP4/MOV with GPMF data only. */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "GPMF_mp4binaryheaders.h"
#include "GPMF_mp4writer.h"




size_t OpenMP4Export(char *filename, uint32_t file_time_base, uint32_t payload_duration)
{
	mp4object *mp4 = (mp4object *)malloc(sizeof(mp4object));
	if (mp4 == NULL) return 0;

	memset(mp4, 0, sizeof(mp4object));

#ifdef _WINDOWS
	fopen_s(&mp4->mediafp, filename, "wb+");
#else
	mp4->mediafp = fopen(filename, "wb");
#endif

	mp4->metasizes = malloc(ALLOC_PAYLOADS*4);
	mp4->metasize_alloc = ALLOC_PAYLOADS;
	if (mp4->mediafp && mp4->metasizes)
	{
		mp4->payload_duration = payload_duration;

		fwrite(hdr, 1, hdr_size, mp4->mediafp);
		for (uint32_t i = 0; i < moov_rate_offsets; i++)
		{
			uint32_t *lptr = (uint32_t *)&moov[moov_byte_rate_offsets[i]];
			*lptr = BYTESWAP32(file_time_base);
		}
		for (uint32_t i = 0; i < moov_duration_offsets; i++)
		{
			uint32_t *lptr = (uint32_t *)&moov[moov_byte_duration_offsets[i]];
			*lptr = 0;
		}
		for (uint32_t i = 0; i < moov_payload_count_offsets; i++)
		{
			uint32_t *lptr = (uint32_t *)&moov[moov_byte_payload_count_offsets[i]];
			*lptr = 0;
		}
	}
	else
	{
		if (mp4->mediafp) fclose(mp4->mediafp);
		if (mp4->metasizes) free(mp4->metasizes);
		free(mp4);
		mp4 = NULL;
	}


	return (size_t)mp4;
}



uint32_t ExportPayload(size_t handle, uint32_t *payload, uint32_t payload_size)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return 0;

	if (mp4->mediafp)
	{
		if (mp4->metasizes && mp4->metasize_count + 1 > mp4->metasize_alloc)
		{
			mp4->metasize_alloc += ALLOC_PAYLOADS;
			mp4->metasizes = realloc(mp4->metasizes, mp4->metasize_alloc * 4);
		}
		mp4->metasizes[mp4->metasize_count] = BYTESWAP32(payload_size);
		mp4->metasize_count++;
		mp4->total_duration += mp4->payload_duration;
		mp4->totalsize += payload_size;
		
		return (uint32_t)fwrite(payload, 1, payload_size, mp4->mediafp);
	}

	return 0;
}



void CloseExport(size_t handle)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return;

	if (mp4->mediafp)
	{
		for (uint32_t i = 0; i < moov_duration_offsets; i++)
		{
			uint32_t *lptr = (uint32_t *)&moov[moov_byte_duration_offsets[i]];
			*lptr = BYTESWAP32(mp4->total_duration);
		}
		for (uint32_t i = 0; i < moov_payload_count_offsets; i++)
		{
			uint32_t *lptr = (uint32_t *)&moov[moov_byte_payload_count_offsets[i]];
			*lptr = BYTESWAP32(mp4->metasize_count);
		}
		for (uint32_t i = 0; i < moov_size_offsets; i++)
		{
			uint32_t *lptr = (uint32_t *)&moov[moov_byte_size_offsets[i]];
			uint32_t offset = BYTESWAP32(*lptr) + mp4->metasize_count * 4;
			*lptr = BYTESWAP32(offset);
		}

		fwrite(moov, 1, moov_size, mp4->mediafp);
		fwrite(mp4->metasizes, 1, mp4->metasize_count * 4, mp4->mediafp);
		fwrite(stco, 1, stco_size, mp4->mediafp);

		fseek(mp4->mediafp, mdat_byte_size_offsets, 0);
		
		uint32_t wtotal = mp4->totalsize + 8; // +8 mdat atom header size
		wtotal = BYTESWAP32(wtotal);
		fwrite(&wtotal, 1, 4, mp4->mediafp);

		fclose(mp4->mediafp);
	}

	if (mp4->metasizes) free(mp4->metasizes), mp4->metasizes = 0;

	free(mp4);
}