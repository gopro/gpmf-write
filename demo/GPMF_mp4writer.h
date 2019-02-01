/*! @file GPMF_mp4writer.h
*
*  @brief Way-way Too Crude MP4|MOV writer
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

#ifndef _GPMF_MP4WRITER_H
#define _GPMF_MP4WRITER_H

#ifdef __cplusplus
extern "C" {
#endif


#define BYTESWAP32(a)			(((a&0xff)<<24)|((a&0xff00)<<8)|((a>>8)&0xff00)|((a>>24)&0xff))
#define ALLOC_PAYLOADS			1024
typedef struct mp4object
{
	uint32_t *metasizes;
	uint32_t metasize_alloc;
	uint32_t metasize_count;
	uint32_t time_base;
	uint32_t duration;
	uint32_t totalsize;
	FILE *mediafp;
} mp4object;


size_t OpenMP4Export(char *filename, uint32_t time_base);

uint32_t ExportPayload(size_t handle, uint32_t *payload, uint32_t payload_size);

void CloseExport(size_t handle);



#ifdef __cplusplus
}
#endif

#endif
