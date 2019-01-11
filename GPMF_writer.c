/*! @file GPMF_writer.cpp
 *
 *	@brief GPMF formatter library
 *
 *	@version 1.0.1
 *	
 *	(C) Copyright 2017 GoPro Inc (http://gopro.com/).
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

#include <stdint.h>
#include <string.h>
#include "threadlock.h"
#include "GPMF_writer.h"

#ifdef DBG
 #if _WINDOWS
 #define DBG_MSG printf
 #else
 #define DBG_MSG gp_print
 #endif
#else
#define DBG_MSG(...)
#endif
#define ERR_MSG gp_print


#if !_WINDOWS
#define strcpy_s(a,b,c) strcpy(a,c)
#define strncpy_s(a,b,c,d) strncpy(a,c,d)
#endif

#define SCAN_GPMF_FOR_STATE		1		// use existing GPMF size fields rather then mirroring variables -- improves thread re-entrancy 

typedef struct GPMFWriterWorkspace
{
	device_metadata *metadata_devices[GPMF_CHANNEL_MAX];  // List of openned metadata sources
	LOCK metadata_device_list[GPMF_CHANNEL_MAX];	// Access lock to insurance for single access the metadata device list

	uint32_t *work_buf;
	int32_t work_buf_size;

	size_t extrn_hndl[GPMF_CHANNEL_MAX][GPMF_EXT_PERFORMATTED_STREAMS];
	uint32_t extrn_StrmFourCC[GPMF_CHANNEL_MAX][GPMF_EXT_PERFORMATTED_STREAMS];
	uint32_t extrn_StrmDeviceID[GPMF_CHANNEL_MAX][GPMF_EXT_PERFORMATTED_STREAMS];

	uint32_t *extrn_buffer[GPMF_CHANNEL_MAX][GPMF_EXT_PERFORMATTED_STREAMS];
	uint32_t extrn_buffer_size[GPMF_CHANNEL_MAX];
} GPMFWriterWorkspace;


int32_t GPMFWriteTypeSize(int type)
{
	int32_t ssize = -1;
	
	switch ((int)type)
	{
	case GPMF_TYPE_STRING_ASCII:		ssize = 1; break;
	case GPMF_TYPE_SIGNED_BYTE:			ssize = 1; break;
	case GPMF_TYPE_UNSIGNED_BYTE:		ssize = 1; break;

	// These datatype can always be stored in Big-Endian
	case GPMF_TYPE_SIGNED_SHORT:		ssize = 2; break;
	case GPMF_TYPE_UNSIGNED_SHORT:		ssize = 2; break;
	case GPMF_TYPE_FLOAT:				ssize = 4; break;
	case GPMF_TYPE_FOURCC:				ssize = 4; break;
	case GPMF_TYPE_SIGNED_LONG:			ssize = 4; break;
	case GPMF_TYPE_UNSIGNED_LONG:		ssize = 4; break;
	case GPMF_TYPE_Q15_16_FIXED_POINT:  ssize = 4; break;
	case GPMF_TYPE_Q31_32_FIXED_POINT:  ssize = 8; break;
	case GPMF_TYPE_DOUBLE:				ssize = 8; break;
	case GPMF_TYPE_SIGNED_64BIT_INT:	ssize = 8; break;
	case GPMF_TYPE_UNSIGNED_64BIT_INT:  ssize = 8; break;

	//All unknown or largeer than 8-bytes store as is:

	case GPMF_TYPE_GUID:				ssize = 16; break;
	case GPMF_TYPE_UTC_DATE_TIME:		ssize = 16; break;
	
	case GPMF_TYPE_COMPRESSED:			ssize = 1; break;  
	case GPMF_TYPE_COMPLEX:				ssize = -1; break;	// unsupported for structsize type
	case GPMF_TYPE_NEST:				ssize = -1; break;	// unsupported for structsize type
	default:							ssize = -1;  		// unsupported for structsize type
	}

	return ssize;
}


int32_t GPMFWriteEndianSize(int type)
{
	int32_t ssize = -1;

	switch ((int)type)
	{
	case GPMF_TYPE_STRING_ASCII:		ssize = 1; break;
	case GPMF_TYPE_SIGNED_BYTE:			ssize = 1; break;
	case GPMF_TYPE_UNSIGNED_BYTE:		ssize = 1; break;

	// These datatype can always be stored in Big-Endian
	case GPMF_TYPE_SIGNED_SHORT:		ssize = 2; break;
	case GPMF_TYPE_UNSIGNED_SHORT:		ssize = 2; break;
	case GPMF_TYPE_FLOAT:				ssize = 4; break;
	case GPMF_TYPE_FOURCC:				ssize = 1; break;
	case GPMF_TYPE_SIGNED_LONG:			ssize = 4; break;
	case GPMF_TYPE_UNSIGNED_LONG:		ssize = 4; break;
	case GPMF_TYPE_Q15_16_FIXED_POINT:  ssize = 4; break;
	case GPMF_TYPE_Q31_32_FIXED_POINT:  ssize = 8; break;
	case GPMF_TYPE_DOUBLE:				ssize = 8; break;
	case GPMF_TYPE_SIGNED_64BIT_INT:	ssize = 8; break;
	case GPMF_TYPE_UNSIGNED_64BIT_INT:  ssize = 8; break;

	case GPMF_TYPE_GUID:				ssize = 1; break; // Do not byte swap
	case GPMF_TYPE_UTC_DATE_TIME:		ssize = 1; break; // Do not byte swap

	//All unknown,complex or larger than 8-bytes store as is:
	default:							ssize = -1;  // unsupported for structsize type
	}

	return ssize;
}

uint32_t GetChunkSize(uint32_t size)
{
	uint32_t chunksize = 1;
	uint32_t chunks = size;


	while(chunks >= 65536)
	{
		chunksize <<= 1;
		chunks = (size + chunksize-1) / chunksize;
	} 

	return chunksize;
}

uint32_t GPMFWriteSetScratchBuffer( size_t ws_handle, uint32_t *buf, uint32_t buf_size)
{
    if (ws_handle == 0 || buf == NULL || buf_size == 0)
    {
	    return GPMF_ERROR_MEMORY;
    }

	GPMFWriterWorkspace *ws = (GPMFWriterWorkspace *)ws_handle;
    ws->work_buf = buf;
	ws->work_buf_size = buf_size;

	return GPMF_ERROR_OK;
}


size_t GPMFWriteStreamOpen(size_t ws_handle, uint32_t channel, uint32_t device_id, char *device_name, char *buffer, uint32_t buffer_size)
{
	device_metadata *dm, *prevdm, *nextdm;
	GPMFWriterWorkspace *ws = (GPMFWriterWorkspace *)ws_handle;
	uint32_t memory_allocated = 0;


	if(ws == NULL)  return 0;

	Lock(&ws->metadata_device_list[channel]);

	if (channel == GPMF_CHANNEL_SETTINGS)
	{
		if (buffer_size <= GPMF_GLOBAL_OVERHEAD)
		{
			if (buffer)
			{
				Unlock(&ws->metadata_device_list[channel]);
				return 0; // buffer offered too small
			}
			buffer_size = GPMF_GLOBAL_OVERHEAD + 1024; // Minimum size + 1KByte
		}
	}
	else
	{
		if (buffer_size <= GPMF_OVERHEAD)
		{
			if (buffer)
			{
				Unlock(&ws->metadata_device_list[channel]);
				return 0; // buffer offered too small
			}
			buffer_size = GPMF_OVERHEAD + 1024; // Minimum size + 1KByte
		}
	}

	if (buffer == NULL)
	{
		buffer = malloc(buffer_size);
		memory_allocated = 1;
		if (buffer == NULL)
		{
			Unlock(&ws->metadata_device_list[channel]);
			return 0; // buffer offered too small
		}
	}

	dm = (device_metadata*)buffer;
	memset(dm, 0, sizeof(device_metadata));
	CreateLock(&dm->device_lock);
	
	dm->channel = channel;
	dm->ws_handle = ws_handle;
	dm->memory_allocated = memory_allocated;

	if (channel == GPMF_CHANNEL_SETTINGS) // more sticky data is used for global settings and there is likely no aperoidic
	{
		strncpy_s(dm->device_name, sizeof(dm->device_name), device_name, sizeof(dm->device_name));
		dm->payload_sticky_buffer = (uint32_t *)(dm + 1);
		dm->payload_sticky_alloc_size = GPMF_GLOBAL_STICKY_PAYLOAD_SIZE; // use about half the buffer for sticky data
		dm->payload_sticky_curr_size = 0;
		dm->payload_sticky_buffer[0] = GPMF_KEY_END;
		memset(dm->payload_sticky_buffer, 0, dm->payload_sticky_alloc_size);

		dm->payload_aperiodic_buffer = (uint32_t *)(((char *)dm->payload_sticky_buffer) + dm->payload_sticky_alloc_size);
		dm->payload_aperiodic_alloc_size = GPMF_GLOBAL_APERIODIC_PAYLOAD_SIZE;  // just in case
		dm->payload_aperiodic_curr_size = 0;
		dm->payload_aperiodic_buffer[0] = GPMF_KEY_END;

		dm->payload_buffer = (uint32_t *)(((char *)dm->payload_aperiodic_buffer) + dm->payload_aperiodic_alloc_size);
		dm->payload_alloc_size = buffer_size - GPMF_GLOBAL_OVERHEAD;
		dm->payload_curr_size = 0;
		dm->payload_buffer[0] = GPMF_KEY_END;
		memset(dm->payload_buffer, 0, dm->payload_alloc_size);
	}
	else
	{
		strncpy_s(dm->device_name, sizeof(dm->device_name), device_name, sizeof(dm->device_name));
		dm->payload_sticky_buffer = (uint32_t *)(dm + 1);
		dm->payload_sticky_alloc_size = GPMF_STICKY_PAYLOAD_SIZE;
		dm->payload_sticky_curr_size = 0;
		dm->payload_sticky_buffer[0] = GPMF_KEY_END;
		memset(dm->payload_sticky_buffer, 0, dm->payload_sticky_alloc_size);

		dm->payload_aperiodic_buffer = (uint32_t *)(((char *)dm->payload_sticky_buffer) + dm->payload_sticky_alloc_size);
		dm->payload_aperiodic_alloc_size = GPMF_APERIODIC_PAYLOAD_SIZE;
		dm->payload_aperiodic_curr_size = 0;
		dm->payload_aperiodic_buffer[0] = GPMF_KEY_END;

		dm->payload_buffer = (uint32_t *)(((char *)dm->payload_aperiodic_buffer) + dm->payload_aperiodic_alloc_size);
		dm->payload_alloc_size = buffer_size - GPMF_OVERHEAD;
		dm->payload_curr_size = 0;
		dm->payload_buffer[0] = GPMF_KEY_END;
		memset(dm->payload_buffer, 0, dm->payload_alloc_size);
	}
	
	if(ws->metadata_devices[channel] == NULL) // This is the first device list
	{
		ws->metadata_devices[channel] = dm;
		dm->next = NULL;
		dm->prev = NULL;
	}
	else
	{
		// Add to the end of the list
		prevdm = nextdm = ws->metadata_devices[channel];
		do
		{
			prevdm = nextdm;
			nextdm = prevdm->next;
		} while(nextdm);

		
		prevdm->next = dm;
		dm->prev = prevdm;
	}

	if (device_id)
	{
		dm->device_id = device_id;
		if(device_id != GPMF_DEVICE_ID_PREFORMATTED)
			ws->metadata_devices[channel]->auto_device_id = device_id;
	}
	else
	{
		dm->device_id = ++ws->metadata_devices[channel]->auto_device_id;
	}
		
    if(device_id == GPMF_DEVICE_ID_PREFORMATTED) // use this stream to embed all external streams
    {
		int i;	
		uint32_t *extbuffer = &dm->payload_buffer[3];
		uint32_t strm_buffer_long_size = ((dm->payload_alloc_size-12) / GPMF_EXT_PERFORMATTED_STREAMS) >> 2;
		
		ws->extrn_buffer_size[channel] = strm_buffer_long_size * 4; // bytes per extern stream buffer
		
		for(i=0; i<GPMF_EXT_PERFORMATTED_STREAMS; i++)
		{
			ws->extrn_buffer[channel][i] = extbuffer;
			extbuffer += strm_buffer_long_size;
		}
    }

// Sort the list to make sure opens with the same device_id are next to each other (a storage efficiency.)
bubblesort:
	prevdm = nextdm = ws->metadata_devices[channel];
	do
	{
		prevdm = nextdm;
		nextdm = prevdm->next;
		
		if(prevdm && nextdm)
		{
			if(nextdm->device_id < prevdm->device_id)
			{
				device_metadata *pprevdm, *nnextdm, *highdm, *lowdm;
				
				lowdm = nextdm;
				highdm = prevdm;
				
				pprevdm = prevdm->prev;
				nnextdm = nextdm->next;
				
				if(pprevdm)
					pprevdm->next = lowdm;
					
				if(nnextdm)
					nnextdm->prev = highdm;
					
				lowdm->prev = pprevdm;
				lowdm->next = highdm;
				
				highdm->prev = lowdm;
				highdm->next = nnextdm;
				
				goto bubblesort;
			}
		}	
	} while(nextdm);

	Unlock(&ws->metadata_device_list[channel]);

	return (size_t)dm;
}



void *GPMFWriteStreamClose(size_t dm_handle) // return the ptr the buffer if needs to be freed.
{
	device_metadata *dm = (device_metadata *)dm_handle;
	if (dm)
	{
		GPMFWriterWorkspace *ws = (GPMFWriterWorkspace *)dm->ws_handle;
		device_metadata *prev, *next;
		uint32_t channel = dm->channel;

		Lock(&ws->metadata_device_list[channel]);
		Lock(&dm->device_lock);

		next = dm->next;
		prev = dm->prev;
		Unlock(&dm->device_lock);
		DeleteLock(&dm->device_lock);

		if (ws->metadata_devices[channel] == dm)
			ws->metadata_devices[channel] = NULL;

		// Repair the device list.
		if (prev)
		{
			if (ws->metadata_devices[channel] == NULL)
				ws->metadata_devices[channel] = prev;
			prev->next = next;
		}
		if (next)
		{
			if (ws->metadata_devices[channel] == NULL)
				ws->metadata_devices[channel] = next;
			next->prev = prev;
		}
		Unlock(&ws->metadata_device_list[channel]);

		if (dm->memory_allocated == 1)
		{
			free(dm);
			dm = NULL;
		}
	}

	return (void *)dm;
}

//DNEWMAN20160510 use the device buffer if there is plenty of unused memory. 
//In low memory situations (large number of samples in a stream, prevents using that stream buffer for formatting) use the global scratch g_work_buf;
static uint32_t *GetScratchBuf(device_metadata *dm, uint32_t requiredsize, uint32_t flags)
{
	if(dm)
	{
		GPMFWriterWorkspace *ws = (GPMFWriterWorkspace *)dm->ws_handle;
		uint32_t freebytes = dm->payload_alloc_size - dm->payload_curr_size;
		if(flags & GPMF_FLAGS_STICKY)
		{
			freebytes = dm->payload_sticky_alloc_size - dm->payload_sticky_curr_size;
			
			if(freebytes > requiredsize * 2)
			{
				uint32_t *buf = dm->payload_sticky_buffer;
				buf += dm->payload_sticky_alloc_size/4;
				buf -= (requiredsize/4) + 2;
				return buf; // use the end of the device sticky buffer for scratch memory
			}
		}
		
		freebytes = dm->payload_alloc_size - dm->payload_curr_size;
		
		if(freebytes > requiredsize * 2)
		{
			uint32_t *buf = dm->payload_buffer;
			buf += dm->payload_alloc_size/4;
			buf -= (requiredsize/4) + 2;
			return buf; // use the end of the device buffer for scratch memory
		}

		if (requiredsize < (uint32_t)ws->work_buf_size)
		{
			DBG_MSG("ws->work_buf used\n");
			return ws->work_buf; /// otherwise use the risky global buffer.
		}
	}

    return NULL;	
}


static void AccumulateSWAPPED(uint32_t typesize, uint32_t *newdata, uint32_t *currdata)
{
	switch (GPMF_SAMPLE_TYPE(typesize))
	{
	case GPMF_TYPE_UNSIGNED_LONG:
		{
			uint32_t in, cur;

			in = BYTESWAP32(*newdata);
			cur = BYTESWAP32(*currdata);
			cur += in;

			*newdata = BYTESWAP32(cur);
		}
		break;
	}
}

static uint32_t IncreasingSortOnType(void *input_data, void *output_data, char storage_type)
{
	switch (storage_type)
	{
		case GPMF_TYPE_STRING_ASCII:
		case GPMF_TYPE_SIGNED_BYTE:
			{
				char *in = (char *)input_data;
				char *out = (char *)output_data;

				if (*in <= *out)
					return 0;
			}
			break;
		case GPMF_TYPE_UNSIGNED_BYTE:
		{
			unsigned char *in = (unsigned char *)input_data;
			unsigned char *out = (unsigned char *)output_data;

			if (*in <= *out)
				return 0;
		}
		break;

		case GPMF_TYPE_SIGNED_SHORT:
		{
			short *in = (short *)input_data;
			short *out = (short *)output_data;

			if (BYTESWAP16(*in) <= BYTESWAP16(*out))
				return 0;
		}
		break;
		case GPMF_TYPE_UNSIGNED_SHORT:
		{
			unsigned short *in = (unsigned short *)input_data;
			unsigned short *out = (unsigned short *)output_data;

			if (BYTESWAP16(*in) <= BYTESWAP16(*out))
				return 0;
		}
		break;

		case GPMF_TYPE_SIGNED_LONG:
		{
			long *in = (long *)input_data;
			long *out = (long *)output_data;

			if (BYTESWAP32(*in) <= BYTESWAP32(*out))
				return 0;
		}
		break;
		case GPMF_TYPE_UNSIGNED_LONG:
		{
			unsigned long *in = (unsigned long *)input_data;
			unsigned long *out = (unsigned long *)output_data;

			if (BYTESWAP32(*in) <= BYTESWAP32(*out))
				return 0;
		}
		break;

		case GPMF_TYPE_FLOAT:
		{
			unsigned long in = *((unsigned long *)input_data);
			unsigned long out = *((unsigned long *)output_data);
			float *fin = (float *)&in;
			float *fout = (float *)&out;

			in = BYTESWAP32(in);
			out = BYTESWAP32(out);

			if (*fin <= *fout)
				return 0;

		}
		break;
	}
	return 1;

}


#if SCAN_GPMF_FOR_STATE
static uint32_t SeekEndGPMF(uint32_t *payload_buf, uint32_t alloc_size)
{
	uint32_t pos = 0;
	
	while(GPMF_VALID_FOURCC(payload_buf[pos]))
	{
		uint32_t packetsize = GPMF_DATA_PACKEDSIZE(payload_buf[pos+1]);
		uint32_t datasize = (packetsize+3)&~3;
		uint32_t nextpos = (8 + datasize) >> 2;
		
		if(nextpos == 0) // this shouldn't happen, so return the last valid position
		{
			payload_buf[pos] = GPMF_KEY_END;
			return pos*4;
		}
		
		if((pos+nextpos)*4 >= alloc_size)  // this shouldn't happen, so return the last valid position with the buffer limits.
		{
			payload_buf[pos] = GPMF_KEY_END;
			return pos*4;
		}
			
		if(payload_buf[pos+nextpos] == GPMF_KEY_END)
		{
			return ((pos*4) + 8 + packetsize);	
		}
		
		pos += nextpos;
	}
	
	return 0;
}
#endif

void AppendFormattedMetadata(device_metadata *dm, uint32_t *formatted, uint32_t bytelen, uint32_t flags, uint32_t sample_count, uint64_t microSecondTimeStamp)
{
	uint32_t count_msg[5];
	uint32_t tag = formatted[0], *payload_ptr;
	uint32_t typesize = formatted[1];
	uint32_t samples = GPMF_SAMPLES(typesize);
	uint32_t *payload_buf, *alloc_size, *curr_size;
	uint32_t curr_size_longs;
	uint32_t curr_size_bytes;
	uint8_t *payload_byte_ptr;

	if(!GPMF_VALID_FOURCC(tag)) return;
	
	if(!(flags & GPMF_FLAGS_LOCKED))  // Use this internal flags if called within a Lock()
		Lock(&dm->device_lock);

	if(microSecondTimeStamp != 0 && flags & GPMF_FLAGS_STORE_ALL_TIMESTAMPS)
	{
		uint32_t swap64timestamp[2];
		uint64_t *ptr64 = (uint64_t *)&swap64timestamp[0];
		uint32_t buf[5];
		uint32_t stampflags = (flags & GPMF_FLAGS_LOCKED) | GPMF_FLAGS_DONT_COUNT;

		*ptr64 = BYTESWAP64(microSecondTimeStamp);		
	//	microSecondTimeStamp = 0;  // with this commented out it will store both jitter removed and raw timestamps.

		buf[0] = GPMF_KEY_TIME_STAMPS;
		buf[1] = MAKEID('J', 8, 0, 1);
		buf[2] = swap64timestamp[0];
		buf[3] = swap64timestamp[1];
		buf[4] = GPMF_KEY_END;
		
		AppendFormattedMetadata(dm, buf, 16, stampflags, 1, 0); // Timing is Sticky, only one value per data stream, it is simpy updated if sent more than once.	
	}
	
again:
	if (flags & GPMF_FLAGS_STICKY)
	{   //sticky
		payload_buf = dm->payload_sticky_buffer;
		alloc_size = &dm->payload_sticky_alloc_size;
		curr_size = &dm->payload_sticky_curr_size;
	}
	else if (flags & GPMF_FLAGS_APERIODIC)
	{	//CV
		payload_buf = dm->payload_aperiodic_buffer;
		alloc_size = &dm->payload_aperiodic_alloc_size;
		curr_size = &dm->payload_aperiodic_curr_size;
	}
	else 
	{ 
		payload_buf = dm->payload_buffer;
		alloc_size = &dm->payload_alloc_size;
		curr_size = &dm->payload_curr_size;
		
		dm->last_nonsticky_fourcc = tag;
		dm->last_nonsticky_typesize = typesize;
		
		if (microSecondTimeStamp)
		{
			if (dm->totalTimeStampCount < MAX_TIMESTAMPS)
			{
				dm->microSecondTimeStamp[dm->totalTimeStampCount] = microSecondTimeStamp;
				dm->totalTimeStampCount++;
			}
		}
	}

#if SCAN_GPMF_FOR_STATE
	curr_size_bytes = SeekEndGPMF(payload_buf, *alloc_size);
#else
	curr_size_bytes = *curr_size;
#endif
	curr_size_longs = (curr_size_bytes+3)>>2;

	if(curr_size_bytes == 0 && bytelen < *alloc_size) // First samples
	{
		payload_buf[(bytelen)>>2] = GPMF_KEY_END; // clear non-aligned
		memcpy(payload_buf, formatted, bytelen);
		payload_buf[(bytelen+3)>>2] = GPMF_KEY_END; // add the terminator
		*curr_size = bytelen;
	}
	else if ((bytelen + curr_size_bytes + 4) < *alloc_size || flags & GPMF_FLAGS_SORTED) // append samples that fit
	{
		uint32_t curr_pos = 0;
		uint32_t curr_byte_pos = 0;
		payload_ptr = payload_buf; 

tryagain:
		while((tag != *payload_ptr || flags & GPMF_FLAGS_GROUPED) && *payload_ptr != GPMF_KEY_END) 
		{
			uint32_t tsize = *(payload_ptr+1);
			uint32_t offset = 2 + ((GPMF_DATA_SIZE(tsize))>>2);

			curr_byte_pos += 8 + ((GPMF_DATA_SIZE(tsize)));
			payload_ptr += offset, curr_pos += offset;
		}
		if (tag == *payload_ptr) // found existing metadata of the same tag to append
		{
			uint32_t currtypesize = *(payload_ptr+1);
			
			if (GPMF_SAMPLE_TYPE(currtypesize) == GPMF_TYPE_NEST)
			{
				uint32_t tsize = *(payload_ptr + 1);
				uint32_t offset = 2 + ((GPMF_DATA_SIZE(tsize)) >> 2);

				curr_byte_pos += 8 + ((GPMF_DATA_SIZE(tsize)));
				payload_ptr += offset, curr_pos += offset;
				goto tryagain;
			}

			if(flags & GPMF_FLAGS_STICKY)// sticky, updata the existing data, do not append
			{
				if (GPMF_DATA_PACKEDSIZE(currtypesize) == GPMF_DATA_PACKEDSIZE(typesize))
				{
					payload_ptr += 2;
					if (flags & GPMF_FLAGS_ACCUMULATE)
					{
						AccumulateSWAPPED(typesize, &formatted[2], payload_ptr);
					}
					memcpy(payload_ptr, &formatted[2], bytelen - 8);
					
					payload_ptr += (bytelen - 8) >> 2, curr_pos += (bytelen - 8) >> 2;
				}
				else // what do we do if the sticky size changes?
				{
					if (payload_ptr[(GPMF_DATA_SIZE(currtypesize)+8)>>2] == GPMF_KEY_END) // it is the last TAG in sticky
					{
						dm->payload_sticky_curr_size -= (GPMF_DATA_SIZE(currtypesize)+8);
						*payload_ptr = GPMF_KEY_END;
					}
					else
					{
						memmove(payload_ptr, payload_ptr+((GPMF_DATA_SIZE(currtypesize)+8)>>2), 4 + dm->payload_sticky_curr_size - (curr_byte_pos+GPMF_DATA_SIZE(currtypesize)+8));
						dm->payload_sticky_curr_size -= GPMF_DATA_SIZE(currtypesize) + 8;
						dm->payload_sticky_buffer[1 + (dm->payload_sticky_curr_size>>2)] = GPMF_KEY_END;
					}
					goto tryagain;
				}
			}
			else if (flags & GPMF_FLAGS_SORTED)
			{
				uint32_t pos;
				//uint32_t offset_bytes = 8 + GPMF_DATA_PACKEDSIZE(currtypesize);
				uint32_t newdata_longs = ((bytelen - 8 + 3 - (GPMF_DATA_SIZE(currtypesize) - GPMF_DATA_PACKEDSIZE(currtypesize))) >> 2);
				//uint32_t offset_longs = (offset_bytes + 3) >> 2;
				uint32_t newsamples = GPMF_SAMPLES(currtypesize) + samples;
				uint32_t newtypesize = MAKEID(GPMF_SAMPLE_TYPE(currtypesize), GPMF_SAMPLE_SIZE(currtypesize), newsamples >> 8, newsamples & 0xff);
				char type = GPMF_SAMPLE_TYPE(payload_ptr[1]);
				uint32_t stored_samples = GPMF_SAMPLES(payload_ptr[1]);
				uint32_t incoming_samples = GPMF_SAMPLES(formatted[1]);
				if (type == '?')
					type = dm->complex_type[0];

				if (incoming_samples != 1)
				{
					//TODO handle more than one sample
				}
				else
				{
					uint8_t *ptr = (uint8_t *)&payload_ptr[2];
					int packetsize = GPMF_DATA_PACKEDSIZE(currtypesize);
					for (pos = 0; pos < stored_samples; pos++)
					{
						if (IncreasingSortOnType((void *)&formatted[2], ptr, type))
						{
							char *src = (char *)ptr;

							if ((bytelen - 8 + curr_size_bytes + 4) < *alloc_size)
							{
								memmove(src + bytelen - 8, src, packetsize);
								memcpy(src, &formatted[2], bytelen - 8);

								payload_ptr = &payload_buf[curr_pos];

								payload_buf[curr_size_longs + newdata_longs] = GPMF_KEY_END; // move the terminator

								payload_ptr[1] = newtypesize;
								*curr_size = (curr_size_longs + newdata_longs) << 2;
								break;
							}
							else
							{
								memmove(src + bytelen - 8, src, packetsize - (bytelen-8));
								memcpy(src, &formatted[2], bytelen - 8);

								payload_ptr = &payload_buf[curr_pos];

								break;
							}
						}
						else
						{
							ptr += bytelen - 8;
							packetsize -= bytelen - 8;
						}
					}

					if (pos == stored_samples) // store at the end, if room
					{
						char *src = (char *)ptr;

						if ((bytelen - 8 + curr_size_bytes + 4) < *alloc_size)
						{
							memcpy(src, &formatted[2], bytelen - 8);

							payload_ptr = &payload_buf[curr_pos];

							payload_buf[curr_size_longs + newdata_longs] = GPMF_KEY_END; // move the terminator

							payload_ptr[1] = newtypesize;
							*curr_size = (curr_size_longs + newdata_longs) << 2;
						}
					}
				}
			}
			else
			{
				uint32_t pos;
				uint32_t offset_bytes = 8 + GPMF_DATA_PACKEDSIZE(currtypesize);
				uint32_t newdata_longs = ((bytelen - 8 + 3 - (GPMF_DATA_SIZE(currtypesize) - GPMF_DATA_PACKEDSIZE(currtypesize))) >> 2);
				uint32_t offset_longs = (offset_bytes + 3) >> 2;
				uint32_t newsamples = GPMF_SAMPLES(currtypesize) + samples;
				uint32_t newtypesize = MAKEID(GPMF_SAMPLE_TYPE(currtypesize), GPMF_SAMPLE_SIZE(currtypesize), newsamples>>8, newsamples&0xff);

				payload_ptr = &payload_buf[curr_pos];

				if (payload_buf[curr_size_longs] == GPMF_KEY_END)
					payload_buf[curr_size_longs + newdata_longs] = GPMF_KEY_END; // move the terminator

				if(payload_ptr[offset_longs] != GPMF_KEY_END) // Need to make room
				{
					curr_pos += offset_longs;
					//make room
					for (pos = curr_size_longs + newdata_longs; pos >= curr_pos+1; pos--)
						payload_buf[pos] = payload_buf[pos - newdata_longs];
				}

				// Insert/Append new data
				payload_ptr[1] = newtypesize;
				payload_byte_ptr = (uint8_t *)payload_ptr; pos = 0;
				payload_byte_ptr += offset_bytes; pos += offset_bytes;
				memcpy(payload_byte_ptr, &formatted[2], bytelen-8); 
				payload_byte_ptr += bytelen - 8; pos += bytelen - 8;
				if (pos & 3)
					payload_byte_ptr += 4 - (pos & 3);
				payload_ptr = (uint32_t *)payload_byte_ptr;
				*curr_size = ((*curr_size + 3)&~3) + newdata_longs * 4;
			}
		}
		else if(*payload_ptr == 0) //append new
		{
			if (curr_size_longs > 0 && flags & GPMF_FLAGS_ACCUMULATE) // Add first
			{
				uint32_t pos;
				uint32_t newdata_longs = ((bytelen + 3) >> 2);

				//make room
				for (pos = curr_size_longs + newdata_longs; pos >= newdata_longs; pos--)
					payload_buf[pos] = payload_buf[pos - newdata_longs];

				memcpy(payload_buf, formatted, bytelen); 
				*curr_size = (curr_size_longs + newdata_longs) << 2;
			}
			else
			{
				payload_ptr[(bytelen) >> 2] = GPMF_KEY_END; // clear non-aligned
				memcpy(payload_ptr, formatted, bytelen); 
				*curr_size = (curr_size_longs * 4) + bytelen;
				payload_ptr += (bytelen + 3) >> 2;
				*payload_ptr = GPMF_KEY_END; // add the terminator
			}
		}
	}
	else
	{
		DBG_MSG("AppendFormattedMetadata, data doesn't fit\n");
		// TODO extend the buffer size if is doesn't fit.
	}

	if (!(flags & GPMF_FLAGS_STICKY) && !(flags & GPMF_FLAGS_DONT_COUNT) && dm->channel != GPMF_CHANNEL_SETTINGS)
	{
		count_msg[0] = GPMF_KEY_TOTAL_SAMPLES;
		count_msg[1] = GPMF_MAKE_TYPE_SIZE_COUNT('L', 4, 1);
		
		if (GPMF_SAMPLE_TYPE(formatted[2]) == GPMF_TYPE_STRING_ASCII || (flags & GPMF_FLAGS_GROUPED) || (flags & GPMF_FLAGS_APERIODIC)) // Only count strings as one (not the number of characters) and Groupped payloads as one
			count_msg[2] = BYTESWAP32(1);
		else
			count_msg[2] = BYTESWAP32(sample_count);
		count_msg[3] = 0;

		tag = count_msg[0];
		typesize = count_msg[1];
		formatted = count_msg;
		sample_count = 1;
		bytelen = 12;	
		flags = GPMF_FLAGS_STICKY_ACCUMULATE | (flags & GPMF_FLAGS_LOCKED); //Preserve the LOCKED flag if set

		goto again;
	}
	
	if(!(flags & GPMF_FLAGS_LOCKED))
		Unlock(&dm->device_lock);
}




void GPMFWriteStreamReset(size_t dm_handle)
{
	device_metadata *dm = (device_metadata *)dm_handle;
	
	if(dm)
	{
		GPMFWriterWorkspace *ws = (GPMFWriterWorkspace *)dm->ws_handle;
		Lock(&dm->device_lock);	
		
		// Clear all non-stick data
		dm->payload_curr_size = 0;
	    *dm->payload_buffer = 0;	
	    
	    // Clear accumators in sticky data
	   	if(dm->payload_sticky_curr_size > 0) 
	   	{
		    uint32_t pos = 0;
		    uint32_t *sticky = dm->payload_sticky_buffer;

		    if(sticky)
		    {
			    while(pos < dm->payload_sticky_curr_size && GPMF_VALID_FOURCC(sticky[pos]))
			    {
				    if(sticky[pos] == GPMF_KEY_TOTAL_SAMPLES)
				    	sticky[pos + 2] = 0;
				    	
			    	if(sticky[pos] == GPMF_KEY_EMPTY_PAYLOADS)
				    	sticky[pos + 2] = 0;
				    
				    pos += 2 + (GPMF_DATA_SIZE(sticky[pos+1])>>2);	
			   }
			}
	    }
		
		// clear CV data
		dm->payload_aperiodic_curr_size = 0;
		*dm->payload_aperiodic_buffer = 0;
		
		// clear timestamps
		dm->totalTimeStampCount = 0;
	    
		if(dm->device_id == GPMF_DEVICE_ID_PREFORMATTED)
	    {
		    int i;	
			for(i=0; i<GPMF_EXT_PERFORMATTED_STREAMS; i++)
			{
				if(ws->extrn_hndl[dm->channel][i])
					GPMFWriteStreamReset(ws->extrn_hndl[dm->channel][i]);
			}
		}

		Unlock(&dm->device_lock);
		
	}	
}




void AddSTRM(size_t hndl, uint32_t *payload)
{
	uint32_t lastGroupFlags = GPMF_FLAGS_STICKY;
	uint32_t STRMSizeLongs = GPMF_DATA_SIZE(*(payload + 1)) >> 2;
	uint32_t tag, tagSize, samples, sampleSize, type;

	payload += 2;
	tag = *payload;
	while (STRMSizeLongs > 1 && GPMF_VALID_FOURCC(tag))
	{
		tagSize = GPMF_DATA_SIZE(*(payload + 1)) >> 2;
		samples = GPMF_SAMPLES(*(payload + 1));
		sampleSize = GPMF_SAMPLE_SIZE(*(payload + 1));
		type = GPMF_SAMPLE_TYPE(*(payload + 1));

		if (tag == GPMF_KEY_TOTAL_SAMPLES) // ignore as the this will be freshly produced
		{
			payload += 2 + tagSize;
			STRMSizeLongs -= 2 + tagSize;
			lastGroupFlags = GPMF_FLAGS_NONE;  // if TSMP is present, then main data group is non-sticky
		}
		else if(tag == GPMF_KEY_EMPTY_PAYLOADS) // ignore as the this will be freshly produced, if needed
		{
			payload += 2 + tagSize;
			STRMSizeLongs -= 2 + tagSize;
		}
		else if (2 + tagSize == STRMSizeLongs) // last sample group
		{
			GPMFWriteStreamStore(hndl,
				tag,
				type,
				sampleSize, /* sample size */
				samples,  /* sample count */
				(void *)(payload + 2), /* pointer to data array */
				lastGroupFlags | GPMF_FLAGS_BIG_ENDIAN);
			STRMSizeLongs = 0;
		}
		else // all data preceeding the main data group is STICKY (metadata describing telemetry)
		{
			GPMFWriteStreamStore(hndl,
				tag,
				type,
				sampleSize, /* sample size */
				samples,  /* sample count */
				(void *)(payload + 2), /* pointer to data array */
				GPMF_FLAGS_STICKY | GPMF_FLAGS_BIG_ENDIAN);

			payload += 2 + tagSize;
			STRMSizeLongs -= 2 + tagSize;
		}

		tag = *payload;
	}
}


int GetPreformattedStrmNumber(size_t dm_handle, uint32_t deviceID, uint32_t *payload)
{
	int num = GPMF_EXT_PERFORMATTED_STREAMS;
	device_metadata *dm = (device_metadata *)dm_handle;

	if (dm)
	{
		GPMFWriterWorkspace *ws = (GPMFWriterWorkspace *)dm->ws_handle;
		uint32_t channel = dm->channel;
		uint32_t tagSize, tag, longs = 2 + (GPMF_DATA_SIZE(*(payload + 1)) >> 2);

		payload += 2; longs -= 2;
		tag = *payload;
		while (longs > 1 && GPMF_VALID_FOURCC(tag))
		{
			tagSize = GPMF_DATA_SIZE(*(payload + 1)) >> 2;

			if (2 + tagSize == longs) // last sample group
			{
				for (num = 0; num < GPMF_EXT_PERFORMATTED_STREAMS; num++)
				{
					if (ws->extrn_StrmFourCC[channel][num] == tag && ws->extrn_StrmDeviceID[channel][num] == deviceID)  // if device ID and stream FourCC match, reuse the existing stream number
						return num;

					if (ws->extrn_StrmFourCC[channel][num] == 0)
					{
						ws->extrn_StrmFourCC[channel][num] = tag;
						ws->extrn_StrmDeviceID[channel][num] = deviceID;
						return num;
					}
				}
				return num;
			}
			else // all data preceeding the main data group is STICKY (metadata describing telemetry)
			{
				payload += 2 + tagSize;
				longs -= 2 + tagSize;
			}

			tag = *payload;
		}
	}

	return num;
}

void SyncExternalGPMF(size_t dm_handle, uint32_t *payload, int size)
{
	device_metadata *dm = (device_metadata *)dm_handle;

	if (dm)
	{
		GPMFWriterWorkspace *ws = (GPMFWriterWorkspace *)dm->ws_handle;
		uint32_t channel = dm->channel;
		uint32_t *payloadPtr = payload;
		uint32_t offset, longs = size >> 2;
		uint32_t tag, deviceID = 0;
		char deviceName[80];
		//int deviceSize;
		int strmNum = 0;
		while (longs > 0 && *payloadPtr == GPMF_KEY_DEVICE)
		{
			payloadPtr++;
			longs--;
			//deviceSize = GPMF_DATA_SIZE(*payloadPtr); 
			payloadPtr++;  longs--;
			tag = *payloadPtr;
			while (longs > 0 && GPMF_VALID_FOURCC(tag))
			{
				if (tag == GPMF_KEY_STREAM)
				{
					offset = 2 + (GPMF_DATA_SIZE(*(payloadPtr + 1)) >> 2);
					strmNum = GetPreformattedStrmNumber(dm_handle, deviceID, payloadPtr);

					if (strmNum < GPMF_EXT_PERFORMATTED_STREAMS)
					{
						if (ws->extrn_hndl[channel][strmNum] == 0 && ws->extrn_buffer[channel][strmNum])
						{
							// send data to new stream
							ws->extrn_hndl[channel][strmNum] = GPMFWriteStreamOpen(dm->ws_handle,
								channel,
								deviceID | 0x01000000,
								deviceName,
								(char *)ws->extrn_buffer[channel][strmNum],
								ws->extrn_buffer_size[channel]);
						}

						if (offset >= 5 && ws->extrn_hndl[channel][strmNum]) // there is something in this stream
							AddSTRM(ws->extrn_hndl[channel][strmNum], payloadPtr);
					}
					payloadPtr += offset;
					longs -= offset;
				}
				else if (tag == GPMF_KEY_DEVICE_ID)
				{
					deviceID = BYTESWAP32(*(payloadPtr + 2)); payloadPtr += 3; longs -= 3;
				}
				else if (tag == GPMF_KEY_DEVICE_NAME)
				{
					int nameLength = GPMF_DATA_PACKEDSIZE(*(payloadPtr + 1));
					if (nameLength < sizeof(deviceName))
					{
						strncpy_s(deviceName, sizeof(deviceName), (char *)(payloadPtr + 2), nameLength);
						deviceName[nameLength] = 0;
					}
					offset = 2 + (((nameLength + 3)&~3) >> 2);
					payloadPtr += offset;
					longs -= offset;
				}
				else if (tag == GPMF_KEY_DEVICE)
				{
					break; // handled by the outter loop
				}
				else //any other tag
				{
					int length = GPMF_DATA_PACKEDSIZE(*(payloadPtr + 1));
					offset = 2 + (((length + 3)&~3) >> 2);
					payloadPtr += offset;
					longs -= offset;
				}

				tag = *payloadPtr;
			}
		}
	}
}




uint32_t GetCurrentStrmFourCC(uint32_t *payload, uint32_t **scale, uint32_t **units, uint32_t **type, uint32_t **tsmp, uint32_t **data)
{
	uint32_t tagSize, tag, longs = 2 + (GPMF_DATA_SIZE(*(payload + 1)) >> 2);

	payload += 2; longs -= 2;
	tag = *payload;

	*data = NULL;
	*scale = NULL;
	*units = NULL;
	*type = NULL;
	*tsmp = NULL;

	while (longs > 1 && GPMF_VALID_FOURCC(tag))
	{
		tagSize = GPMF_DATA_SIZE(*(payload + 1)) >> 2;

		if (2 + tagSize == longs) // last sample group
		{
			*data = payload;

			return tag;
		}
		else // all data preceeding the main data group is STICKY (metadata describing telemetry)
		{
			if (tag == GPMF_KEY_SCALE)
				*scale = payload;
			if (tag == GPMF_KEY_UNITS)
				*units = payload;
			if (tag == GPMF_KEY_SI_UNITS)
				*units = payload;
			if (tag == GPMF_KEY_TYPE)
				*type = payload;
			if (tag == GPMF_KEY_TOTAL_SAMPLES)
				*tsmp = payload;

			payload += 2 + tagSize;
			longs -= 2 + tagSize;
		}

		tag = *payload;
	}

	*data = NULL;
	*scale = NULL;
	*units = NULL;
	*type = NULL;
	*tsmp = NULL;

	return 0; // no tag match
}



static void ExpandComplexTYPE(char *src, char *dst, int maxsize)
{
	int i=0, k=0, count = 0;
	
	while(i<maxsize && k<maxsize)
	{
		if(src[i]=='[' && i>0) 
		{
			int j = 1;
			count = 0;
			while(src[i+j] >= '0' && src[i+j] <= '9')
			{
				count *= 10;
				count += src[i+j] - '0';
				j++;
			}			
			
			if(count > 1)
			{
				int l;
				for(l=1; l<count; l++)
				{
					dst[k] = src[i-1]; 
					k++;
				}
			}
			i+=j;	
			if (src[i] == ']') i++;
		}
		else
		{
			dst[k] = src[i]; 
			if (dst[k] == 0) break;
			i++, k++;
		}
	}	

	if (k >= maxsize)
		dst[0] = 0; // bad structure formed
		
}

uint32_t GPMFWriteStreamStoreStamped(    //Send RAW data to be formatted for the MP4 metadata track
  size_t dm_handle, 
  uint32_t tag, 
  uint32_t data_type,
  uint32_t sample_size, 
  uint32_t sample_count,
  void *data, 
  uint32_t flags,			// 1- metadata is sticky, sent only once, but stored in every payload
  uint64_t microSecondTimeStamp   // if zero, this is the same call as GPMFWriteStreamStore()
)
{
	uint32_t required_size = sample_count * sample_size + 12;
	uint32_t local_buf[128];
	uint32_t *scratch_buf = local_buf; 
	uint32_t tick = 0;

	device_metadata *dm = (device_metadata *)dm_handle;

	if (microSecondTimeStamp == 0 && flags & GPMF_FLAGS_ADD_TICK)
	{
		if (!(flags & GPMF_FLAGS_STICKY) && !(flags & GPMF_FLAGS_BIG_ENDIAN)) // non-sticky only -- first sample, and not sent from an external preformat (BigEndian) device
		{
			if (dm->device_id == GPMF_DEVICE_ID_CAMERA && dm->channel != GPMF_CHANNEL_SETTINGS)
			{
				if (dm->payload_tick == 0)
				{
					uint32_t buf[4];
					GetTick(&tick);
					dm->payload_tick = tick;

					buf[0] = GPMF_KEY_TICK;
					buf[1] = MAKEID('L', 4, 0, 1);
					buf[2] = BYTESWAP32(tick);
					buf[3] = GPMF_KEY_END;

					AppendFormattedMetadata(dm, buf, 12, GPMF_FLAGS_STICKY, 1, 0); // Timing is Sticky, only one value per data stream, it is simpy updated if sent more than once.	
				}
			}
		}
		else if (flags & GPMF_FLAGS_STICKY)
		{
			if (dm->device_id == GPMF_DEVICE_ID_CAMERA && dm->channel != GPMF_CHANNEL_SETTINGS)
			{
				if (dm->payload_sticky_curr_size == 0)
				{
					uint32_t buf[4];
					buf[0] = GPMF_KEY_TICK;
					buf[1] = MAKEID('L', 4, 0, 1);
					buf[2] = 0;
					buf[3] = GPMF_KEY_END;

					AppendFormattedMetadata(dm, buf, 12, GPMF_FLAGS_STICKY, 1, 0); // Desktop Quik 2.2 can't support TICK after SCAL, this ensure it is before.
				}
			}
		}
	}
	

    if(tag == GPMF_KEY_PREFORMATTED)
    {
    	SyncExternalGPMF(dm_handle, (uint32_t *)data, sample_size*sample_count);
    	return GPMF_ERROR_OK;
	}
        
	if(dm)
	{
		if(required_size > sizeof(local_buf))
			scratch_buf = GetScratchBuf(dm, required_size, flags); //DNEWMAN20160510 
		
		if (scratch_buf == NULL)
		{
			return GPMF_ERROR_MEMORY;
		}

		if(flags&GPMF_FLAGS_STICKY)
		{	
			if (tag == (uint32_t)STR2FOURCC("QUAN"))
			{
				dm->quantize = *((uint32_t *)data);
				return GPMF_ERROR_OK;
			}

			if(dm->payload_sticky_curr_size+required_size > dm->payload_sticky_alloc_size) 
				return GPMF_ERROR_MEMORY;
		}
		else if(dm->payload_curr_size+required_size > dm->payload_alloc_size) 
			return GPMF_ERROR_MEMORY;

		{
			uint32_t len = 0, blen = 0;
			uint32_t i,*valptr;

			if (tag == MAKEID('T', 'Y', 'P', 'E'))// DNEWMAN20160515 Added to support byte-swapping complex structures
			{
				int array = 0;
				char *c = (char *)data;
				for(i=0; i<strlen(c); i++)
					if(c[i] == '[') array = 1;
				
				if(array)
					ExpandComplexTYPE(c, dm->complex_type, sizeof(dm->complex_type));
				else
				{				
					if (strlen(c) < sizeof(dm->complex_type))
						strcpy_s(dm->complex_type, sizeof(dm->complex_type), c);
					else
						dm->complex_type[0] = 0;
				}
			}

			if ((data_type == 0) && (sample_size * sample_count) & 0x3) // keep nest sizes to four byte aligned
			{
				int totalbytes = sample_size * sample_count;
				totalbytes += 3;
				totalbytes >>= 2;

				scratch_buf[len++] = tag;
				scratch_buf[len++] = MAKEID(data_type, 4, totalbytes >> 8, totalbytes & 0xff);
				blen += 8;
			}
			else
			{
				scratch_buf[len++] = tag;
				scratch_buf[len++] = MAKEID(data_type, sample_size, sample_count >> 8, sample_count & 0xff);
				blen += 8;
			}

					
			valptr = (uint32_t *)data;

			if (flags & GPMF_FLAGS_BIG_ENDIAN)
				for (i = 0; i < (sample_count * sample_size + 3) / sizeof(uint32_t); i++)
					scratch_buf[len++] = valptr[i];
			else // Little-endian, needs to be swapped 
			{
				int32_t endianSize = GPMFWriteEndianSize(data_type);
				if (endianSize == 8) // 64-bit swap required
				{
					for (i = 0; i < (sample_count * sample_size + 3) / sizeof(uint32_t); i += 2)
					{
						scratch_buf[len++] = BYTESWAP32(valptr[i+1]);  // DNEWMAN20160515 Fix for 64-bit byte-swapping
						scratch_buf[len++] = BYTESWAP32(valptr[i]); 
					}
				}
				else if (endianSize >= 1)
				{
					for (i = 0; i < (sample_count * sample_size + 3) / sizeof(uint32_t); i ++)
					{
						switch (endianSize)
						{
						case 2:		scratch_buf[len++] = BYTESWAP2x16(valptr[i]); break;
						case 4:		scratch_buf[len++] = BYTESWAP32(valptr[i]); break;
						default:	scratch_buf[len++] = valptr[i]; break;
						}
					}
				}
				else // DNEWMAN20160515 Added to support byte-swapping complex structures
				{
					if (data_type == GPMF_TYPE_COMPLEX && dm->complex_type[0])
					{
						unsigned char *bvptr = (unsigned char *)valptr;
						unsigned char *bsptr = (unsigned char *)&scratch_buf[len];
						int type_position = 0, type_size = 0;
						int type_samplesleft = sample_size;
						int samples = sample_count;
						while (dm->complex_type[type_position] && type_samplesleft > 0 && samples > 0)
						{
							endianSize = GPMFWriteEndianSize((int)dm->complex_type[type_position]);
							type_size = GPMFWriteTypeSize((int)dm->complex_type[type_position]);
							type_samplesleft -= type_size;

							if (endianSize == 8) // 64-bit swap required
							{
								*((uint32_t*)bsptr) = BYTESWAP32(*((uint32_t*)(bvptr+4)));  bsptr += 4;
								*((uint32_t*)bsptr) = BYTESWAP32(*((uint32_t*)bvptr));  bsptr += 4;  bvptr += 8;
							}
							else
							{
								switch (endianSize)
								{
									case 2:
									{
										short val = *((uint16_t*)(bvptr)); bvptr += 2;
										val = ((val & 0xff) << 8) | ((val >> 8) & 0xff);
										*((uint16_t*)bsptr) = val; bsptr += 2;
										break;
									}
									case 4:
									{
										*((uint32_t*)bsptr) = BYTESWAP32(*((uint32_t*)(bvptr)));
										bsptr += 4, bvptr += 4;
										break;
									}
									default:
									{
										int ii;
										for (ii = 0; ii < type_size; ii++)
										{
											*bsptr++ = *bvptr++;
										}
										break;
									}
								}
							}

							type_position++;
							if (type_samplesleft == 0)
							{
								samples--;
								if (samples > 0)
								{
									type_samplesleft = sample_size;
									type_position = 0;
								}
							}
						}

						if (dm->complex_type[type_position] != 0 || type_samplesleft > 0)
						{
							return GPMF_ERROR_STRUCTURE;  // sample_size doesn't match the complex structure defined with typedef
						}
					}
				}
			}

			blen += sample_count * sample_size;
			AppendFormattedMetadata(dm, scratch_buf, blen, flags, sample_count, microSecondTimeStamp);
		
		}
		/*
		if (!(flags&GPMF_FLAGS_STICKY))
		{
			GPMFWriteStreamStore(    //Send RAW data to be formatted for the MP4 text track
				dm_handle,
				GPMF_KEY_TOTAL_SAMPLES,
				GPMF_TYPE_UNSIGNED_LONG,
				4,
				1,
				&sample_count,
				0.0,
				GPMF_FLAGS_STICKY_ACCUMULATE	
				);				
		}*/
	}
	else
		return GPMF_ERROR_DEVICE;
	

	return GPMF_ERROR_OK;
}

uint32_t GPMFWriteStreamStore(    //Send RAW data to be formatted for the MP4 metadata track
  size_t dm_handle, 
  uint32_t tag, 
  uint32_t data_type,
  uint32_t sample_size, 
  uint32_t sample_count,
  void *data, 
  uint32_t flags			// 1- metadata is sticky, sent only once, but stored in every payload
)
{
	return GPMFWriteStreamStoreStamped(dm_handle, tag, data_type, sample_size, sample_count, data, flags, 0);
}


//Beginning of computed data, e.g object detection.  Open an nested GPMF group so the multiple entries can represet one momemnt in time (e.g. n-objects in one frame.)
// ..CVOpen() should be called before time consuming computation has begun, as it will store at what time the detected objects/event occurs.
uint32_t GPMFWriteStreamAperiodicBegin(size_t dm_handle, uint32_t tag)
{
	uint32_t ret;
	uint32_t tick = 0;
	GetTick(&tick);

	ret = GPMFWriteStreamStore(
		dm_handle,
		MAKEID('T', 'I', 'C', 'K'),
		'L',
		4,
		1,
		&tick,
		GPMF_FLAGS_APERIODIC | GPMF_FLAGS_DONT_COUNT);

	if(ret != GPMF_ERROR_OK) return ret;
	
	ret = GPMFWriteStreamStore(
		dm_handle,
		MAKEID('T', 'O', 'C', 'K'),
		'L',
		4,
		1,
		&tick,
		GPMF_FLAGS_APERIODIC | GPMF_FLAGS_DONT_COUNT);
			
	return ret;

}


//Send objects/event one a time or several at once if not within Begin/End
uint32_t GPMFWriteStreamAperiodicStore(  
	size_t dm_handle, 
	uint32_t tag, 
	uint32_t data_type,
	uint32_t sample_size, 
	uint32_t sample_count,
	void *data
)
{	
	return GPMFWriteStreamStore(  
		dm_handle, 
		tag, 
		data_type, 
		sample_size,
		sample_count,
		data,
		GPMF_FLAGS_APERIODIC);
}


//Close objects/events once all processing is compute, after CVOpen, CVWrite (optional, only if something is detected == no faces found is a detection)
uint32_t GPMFWriteStreamAperiodicEnd(size_t dm_handle, uint32_t tag)
{
	uint32_t ret = GPMF_ERROR_DEVICE, tick = 0;
	uint32_t val;
	uint32_t *data;
	GetTick(&tick);
	val = BYTESWAP32(tick);
	
	device_metadata *dm = (device_metadata *)dm_handle;

	if (dm)
	{
		Lock(&dm->device_lock);

		data = dm->payload_aperiodic_buffer;

		//update TOCK, if this is slow aperiodic data
		if (data[3] == MAKEID('T', 'O', 'C', 'K'))
			data[5] = val;

		ret = GPMFWriteStreamStore(
			dm_handle,
			tag,
			GPMF_TYPE_NEST,
			1,
			dm->payload_aperiodic_curr_size,
			data,
			(uint32_t)(GPMF_FLAGS_LOCKED | GPMF_FLAGS_BIG_ENDIAN | GPMF_FLAGS_DONT_COUNT));

		// reset the CV Nest
		dm->payload_aperiodic_curr_size = 0;
		dm->payload_aperiodic_buffer[0] = 0;

		Unlock(&dm->device_lock);
	}

	return ret;
}





size_t GPMFWriteServiceInit()
{
	GPMFWriterWorkspace *ws = malloc(sizeof(GPMFWriterWorkspace));

	if (ws)
	{
		int i;

		memset(ws, 0, sizeof(GPMFWriterWorkspace));

		for (i = 0; i < GPMF_CHANNEL_MAX; i++)
			CreateLock(&ws->metadata_device_list[i]); // Insurance for single access the metadata device list

		return (size_t)ws;
	}

	return 0;
}

void GPMFWriteServiceClose(size_t ws_handle)
{
	GPMFWriterWorkspace *ws = (GPMFWriterWorkspace *)ws_handle;
	if (ws)
	{
		int i;
		for (i = 0; i < GPMF_CHANNEL_MAX; i++)
			DeleteLock(&ws->metadata_device_list[i]);

		free(ws);
	}
}



uint32_t GPMFWriteEstimateBufferSize(size_t ws_handle, uint32_t channel, uint32_t payloadscale) // how much data is currently needing to be readout. 
{
	GPMFWriterWorkspace *ws = (GPMFWriterWorkspace *)ws_handle;
	uint32_t estimatesize = 0;
	uint32_t totalsize = 0;
	device_metadata *dm, *dmnext;
	uint32_t session_scale = 0;
	uint32_t last_deviceID = 0;
	uint32_t devicesizebytes = 0;
	uint32_t session_scale_count;
	
	if (payloadscale)
		session_scale = payloadscale;

	if (ws)
	{

		Lock(&ws->metadata_device_list[channel]);	//Prevent device list changes while extracting data from the current device list

		dm = ws->metadata_devices[channel];

		// Copy in all the metadata, formatted into the new buffer
		while (dm)
		{
			Lock(&dm->device_lock); // Get data and return, minimal processing within the lock

			session_scale_count = dm->session_scale_count;

			//if(dm->payload_curr_size > 0) // Store information of all connected devices even if they have sent no data
			{
				size_t namelen;
				uint32_t namlen4byte;

				if (dm->device_id != last_deviceID) // Store device name and id and the begin of the data as a device
				{
					last_deviceID = dm->device_id;

					totalsize += devicesizebytes;
					devicesizebytes = 0;

					// New device for the new device
					totalsize += 8;

					//New Device ID -- GoPro source have a known ID
					devicesizebytes += 12;

					//String name for the device
					namelen = strlen(dm->device_name);
					namlen4byte = (namelen + 3) & ~3;
					devicesizebytes += 8 + namlen4byte;
				}


				// Wrap telemetry in a New Stream (or channel) if the payload has sticky metadata
				if (dm->payload_sticky_curr_size > 0)
				{
					devicesizebytes += 8;
					//Sticky Metadata for each stream
					devicesizebytes += ((dm->payload_sticky_curr_size + 3)&~3);
				}


				if (dm->payload_curr_size > 0)
				{
					uint32_t *src_lptr = (uint32_t *)dm->payload_buffer;

					//copy the preformatted device metadata into the output buffer
					if (session_scale == 0)
					{
#if SCAN_GPMF_FOR_STATE
						uint32_t payload_curr_size = SeekEndGPMF(src_lptr, dm->payload_alloc_size);
#else
						uint32_t payload_curr_size = dm->payload_curr_size;
#endif
						devicesizebytes += ((payload_curr_size + 3)&~3);
					}
					else
					{
						uint32_t last_tag = 0;
						uint32_t tag = src_lptr[0];
						do
						{
							uint32_t samples = GPMF_SAMPLES(src_lptr[1]);
							if ((samples >= (session_scale * 2) && session_scale) || GPMF_SAMPLE_TYPE(src_lptr[1]) == GPMF_TYPE_NEST || last_tag == tag) //DAN20160609 Scale data that is twice or more the the target sample rate.
							{
								if (GPMF_SAMPLE_TYPE(src_lptr[1]) != GPMF_TYPE_NEST && last_tag != tag)
								{
									uint32_t newscale = (samples + (session_scale / 2)) / session_scale;
									int samples_out = 0;
									int sample_size = GPMF_SAMPLE_SIZE(src_lptr[1]);
									src_lptr += (8 + GPMF_DATA_SIZE(src_lptr[1])) >> 2;

									if (newscale <= 1) newscale = 2;				//DAN20160609 support the new Session scaling

									while (samples--)
									{
										if (++session_scale_count >= newscale)  //DAN20160609 support the new Session scaling
										{
											session_scale_count = 0;
											samples_out++;
										}
									}

									devicesizebytes += (8 + sample_size * samples_out + 3) & ~3;
								}
								else
								{
									src_lptr += (8 + GPMF_DATA_SIZE(src_lptr[1])) >> 2;
								}
							}
							else
							{
								uint32_t datasize = GPMF_DATA_SIZE(src_lptr[1]);
								devicesizebytes += datasize + 8;
								src_lptr += (datasize + 8) >> 2;
							}

							last_tag = tag;
							tag = src_lptr[0];
						} while (GPMF_VALID_FOURCC(src_lptr[0]));
					}
				}
			}

			dmnext = dm->next;
			Unlock(&dm->device_lock);
			dm = dmnext;
		}

		Unlock(&ws->metadata_device_list[channel]);

		totalsize += devicesizebytes;
		estimatesize = totalsize;

		estimatesize *= 11;
		estimatesize /= 10; //Add 10% just in case extra samples arrive between calls
		estimatesize &= ~0x3;
	}
	return estimatesize;
}



static uint32_t IsValidGPMF(uint32_t *buffer, uint32_t size, uint32_t recurse, uint32_t level) // test if the data is a completed GPMF structure starting with DEVC
{
	uint32_t pos = 0;
	size >>= 2; // long size
	while (pos + 1 < size)
	{
		uint32_t tag = buffer[pos];
		uint32_t tsize = buffer[pos + 1];
		uint32_t datasize = GPMF_DATA_SIZE(tsize);
		if (level == 0 && tag != GPMF_KEY_DEVICE)
			return 0;
		else
		{
			if (0 == GPMF_VALID_FOURCC(tag))
				return 0;

			pos += 2;
			if (recurse && GPMF_SAMPLE_TYPE(tsize) == GPMF_TYPE_NEST)
			{
				int ret = IsValidGPMF(&buffer[pos], datasize, recurse, level + 1);
				if (ret == 0)
					return 0;
			}
			pos += (datasize >> 2);
		}
	}

	if (pos == size)
		return 1;
	else
		return 0;
}

uint32_t GPMFWriteIsValidGPMF(uint32_t *buffer, uint32_t size, uint32_t recurse) // test if the data is a completed GPMF structure starting with DEVC
{
	return IsValidGPMF(buffer, size, recurse, 0);
}



void GPMF_CompressedPutWord(BITSTREAM *stream, BITSTREAM_WORD_TYPE word)
{
	const int nWordsPerLong = sizeof(BITSTREAM_WORD_TYPE) / sizeof(uint8_t);
	BITSTREAM_WORD_TYPE *lpCurrentWord = (BITSTREAM_WORD_TYPE *)(stream->lpCurrentWord);
	int wordsUsed = stream->wordsUsed + nWordsPerLong;

	// Check that there is room in the block for the int32_t word
	assert(wordsUsed <= stream->dwBlockLength);
	if (wordsUsed <= stream->dwBlockLength)
	{
		*(lpCurrentWord++) = BYTESWAP16(word);

		stream->lpCurrentWord = (uint8_t *)lpCurrentWord;
		stream->wordsUsed = wordsUsed;
	}
	else
	{
		stream->error = BITSTREAM_ERROR_OVERFLOW;
	}
}

// Write bits to a compressed bitstream
void GPMF_CompressedPutBits(BITSTREAM *stream, int wBits, int nBits)
{
	BITSTREAM_WORD_TYPE  wBuffer;
	BITSTREAM_WORD_TYPE bitsFree;

	// Move the current word into a int32_t buffer
	wBuffer = stream->wBuffer;

	// Number of bits remaining in the current word
	bitsFree = stream->bitsFree;

	// Will the bits fit in the int32_t buffer?
	if (bitsFree == BITSTREAM_WORD_SIZE) {
		wBuffer = wBits & BITMASK(nBits);
		bitsFree -= (BITSTREAM_WORD_TYPE)nBits;
	}
	else if (nBits <= bitsFree) {
		wBuffer <<= nBits;
		wBuffer |= (wBits & BITMASK(nBits));
		bitsFree -= (BITSTREAM_WORD_TYPE)nBits;
	}
	else {
		// Fill the buffer with as many bits as will fit
		wBuffer <<= bitsFree;
		nBits -= bitsFree;

		// Should have some bits remaining
		assert(nBits > 0);

		// Insert as many bits as will fit into the buffer
		wBuffer |= (wBits >> nBits) & BITMASK(bitsFree);

		// Insert all of the bytes in the buffer into the bitstream
		GPMF_CompressedPutWord(stream, wBuffer);

		wBuffer = wBits & BITMASK(nBits);
		bitsFree = BITSTREAM_WORD_SIZE - (BITSTREAM_WORD_TYPE)nBits;
	}

	// Save the new current word and bit count in the stream
	stream->wBuffer = wBuffer;
	stream->bitsFree = bitsFree;
}




static void GPMF_CompressedFlushStream(BITSTREAM *stream)
{
	int bitsFree = stream->bitsFree;

	// Does the buffer contain any data?
	if (bitsFree < BITSTREAM_WORD_SIZE)
	{
		BITSTREAM_WORD_TYPE wBuffer = stream->wBuffer;

		// Fill the rest of the buffer with zeros
		wBuffer <<= bitsFree;

		// Write the buffer to the output
		GPMF_CompressedPutWord(stream, wBuffer);

		// Indicate that the bitstream buffer is empty
		stream->bitsFree = BITSTREAM_WORD_SIZE;
		stream->wBuffer = 0;
	}
}


BITSTREAM_WORD_TYPE GPMF_CompressedPutCode(BITSTREAM *stream, int code)
{
	BITSTREAM_WORD_TYPE numBits, bits;
	numBits = enccontrolcodestable.entries[code].size;
	bits = enccontrolcodestable.entries[code].bits;

	GPMF_CompressedPutBits(stream, bits, numBits);

	return numBits;
}


int GPMF_CompressedZeroRun(BITSTREAM *stream, int zeros)
{
	uint32_t totalBits = 0;
	BITSTREAM_WORD_TYPE numBits = 0, bits;
	int i = enczerorunstable.length - 1;

	while (i >= 0 && zeros > 0)
	{
		if (enczerorunstable.entries[i].count > zeros)
		{
			i--;
		}
		else
		{
			numBits = enczerorunstable.entries[i].size;
			bits = enczerorunstable.entries[i].bits;
			zeros -= enczerorunstable.entries[i].count;

			GPMF_CompressedPutBits(stream, bits, numBits);
			totalBits += numBits;
		}
	}

	// zeros have code '0'
	GPMF_CompressedPutBits(stream, 0, zeros);
	totalBits += zeros;

	return totalBits;
}

int GPMF_CompressedPutValue(BITSTREAM *stream, int delta)
{
	BITSTREAM_WORD_TYPE numBits = 0, bits;
	int mag = abs(delta);

	if (mag < enchuftable.length)
	{
		numBits = enchuftable.entries[mag].size;
		bits = enchuftable.entries[mag].bits;
		if (mag) {
			bits <<= 1;
			if (delta < 0) bits |= 1; //add sign bit
			numBits++; //sign bit.
		}

		GPMF_CompressedPutBits(stream, bits, numBits);
	}
	else
	{
		numBits += GPMF_CompressedPutCode(stream, HUFF_ESC_CODE_ENTRY);
		GPMF_CompressedPutBits(stream, delta, stream->bits_per_src_word); numBits += stream->bits_per_src_word;
	}

	return numBits;
}


// Initialize the bitstream
void GPMF_InitCompressedBitstream(BITSTREAM *stream, uint8_t *buffer, uint32_t bufferLength, uint32_t bits_per_src_word)
{
	// Initialize the block of words
	stream->dwBlockLength = bufferLength;
	stream->lpCurrentWord = buffer;
	stream->wordsUsed = 0;

	// Initialize the current bit buffer
	stream->bitsFree = BITSTREAM_WORD_SIZE;
	stream->wBuffer = 0;
	stream->error = 0;
	stream->bits_per_src_word = (uint16_t)bits_per_src_word;
}


#if HUFF_STATS
uint32_t sourcesize = 0;
uint32_t storedsize = 0;
#endif

uint32_t GPMFCompress(uint32_t* dst_gpmf, uint32_t *src_gpmf, uint32_t payloadAddition, uint32_t quantize)
{
	BITSTREAM bstream;
	uint32_t returnpayloadsize = 0;
	uint32_t typesizerepeat = src_gpmf[1];
	uint8_t *bptr = (uint8_t *)src_gpmf;
	uint8_t type = bptr[4];
	//uint8_t size = bptr[5];
	uint16_t repeat = (bptr[6] << 8) | bptr[7];
	uint32_t quantHi = quantize;
	uint32_t quantLo = quantize;

	//[] = 32-bit, {} - 16-bit, unisgned or unsigned depending on the uncompressed type.
	//[FOURCC]['#'sizerepeat][uncompressed typeSizeRepeat]{quantization}{first value}... delta encoded huffman ...

	dst_gpmf[0] = src_gpmf[0];         returnpayloadsize += 4;
	dst_gpmf[1] = 0;/*fill at end*/    returnpayloadsize += 4;
	dst_gpmf[2] = typesizerepeat;      returnpayloadsize += 4;

	int bytesize = GPMFWriteTypeSize(type);
	int channels = GPMF_SAMPLE_SIZE(typesizerepeat) / bytesize;

	switch (type)
	{
	case GPMF_TYPE_SIGNED_LONG:
	case GPMF_TYPE_UNSIGNED_LONG:
		channels *= 2, bytesize = 2; // treat LONGs as two channels of shorts
		quantHi = 1;
	case GPMF_TYPE_SIGNED_BYTE:
	case GPMF_TYPE_UNSIGNED_BYTE:
	case GPMF_TYPE_SIGNED_SHORT:
	case GPMF_TYPE_UNSIGNED_SHORT:
	{
		uint32_t i;
		int signed_type = 1; // not signed
		int chn;
		int8_t *dbyte = (int8_t *)&dst_gpmf[3];
		uint8_t *dByte = (uint8_t *)dbyte;
		//int16_t *dshort = (int16_t *)dbyte;
		uint16_t *dShort = (uint16_t *)dbyte;
		int8_t *sbyte = (int8_t *)&src_gpmf[2];
		uint8_t *sByte = (uint8_t *)sbyte;
		int16_t *sshort = (int16_t *)sbyte;
		uint16_t *sShort = (uint16_t *)sbyte;
		int pos = 0;

		channels = GPMF_SAMPLE_SIZE(typesizerepeat) / bytesize;

		if (type == GPMF_TYPE_SIGNED_SHORT || type == GPMF_TYPE_SIGNED_BYTE || type == GPMF_TYPE_SIGNED_LONG)
			signed_type = -1; //signed

		memcpy(dbyte, sbyte, bytesize*channels);  // store the first full sample as is.
		pos += channels;
		returnpayloadsize += bytesize*channels;

		for (chn = 0; chn < channels; chn++)
		{
			uint32_t quant = quantHi, bufsize = payloadAddition - returnpayloadsize;
			uint32_t totalbits = 0, zerorun = 0;

			if (chn & 1) quant = quantLo; // Hack for encoding quantized 32-bit data with 16-bits.

			if (bytesize == 2)
			{
				dShort[pos] = BYTESWAP16(quant); pos++,
					GPMF_InitCompressedBitstream(&bstream, (uint8_t*)&dShort[pos], bufsize, bytesize * 8);
			}
			else
			{
				dByte[pos] = (uint8_t)quant; pos++;
				pos = ((pos + 1) & ~1); //16-bit aligned compressed data
				GPMF_InitCompressedBitstream(&bstream, &dByte[pos], bufsize, bytesize * 8);
			}


			returnpayloadsize += 4;

			for (i = 1; i < repeat; i++)
			{
				int delta = 0;
				switch (bytesize*signed_type)
				{
				default:
				case -2: delta = ((int16_t)BYTESWAP16(sshort[i*channels + chn]) / (int16_t)quant) - ((int16_t)BYTESWAP16(sshort[(i - 1)*channels + chn]) / (int16_t)quant); break;
				case -1: delta = ((int)(sbyte[i*channels + chn]) / (int)quant) - ((int)(sbyte[(i - 1)*channels + chn]) / (int)quant); break;
				case 1: delta = ((int)(sByte[i*channels + chn]) / (int)quant) - ((int)(sByte[(i - 1)*channels + chn]) / (int)quant); break;
				case 2: delta = ((int)BYTESWAP16(sShort[i*channels + chn]) / (int)quant) - ((int)BYTESWAP16(sShort[(i - 1)*channels + chn]) / (int)quant); break;
				}

				if (delta == 0) {
					zerorun++;
					continue;
				}

				if (zerorun)
				{
					totalbits += GPMF_CompressedZeroRun(&bstream, zerorun);
					zerorun = 0;
				}

				totalbits += GPMF_CompressedPutValue(&bstream, delta);

				//make sure compressed is not larger than uncompressed.
				if (totalbits + 256 > bufsize * 8) // in bits
				{
					//too big, just store uncompresssed.
					memcpy(dst_gpmf, src_gpmf, payloadAddition);
#if HUFF_STATS
					sourcesize += payloadAddition;
					storedsize += payloadAddition;
#endif
					return payloadAddition;
				}
			}

			totalbits += GPMF_CompressedPutCode(&bstream, HUFF_END_CODE_ENTRY);

			GPMF_CompressedFlushStream(&bstream);

			int bytesadded = ((totalbits + 7) / 8);
			int lastsize = returnpayloadsize;
			returnpayloadsize += bytesadded;
			returnpayloadsize = ((returnpayloadsize + 1) & ~1); //16-bit aligned with a compressed channel
			pos += ((returnpayloadsize - lastsize) >> (bytesize - 1));
		}
		break;
	}
	default: // do not compress other types of data
		memcpy(dst_gpmf, src_gpmf, payloadAddition);
		return payloadAddition;
	}


	returnpayloadsize = ((returnpayloadsize + 3) >> 2) << 2; //32-bit aligned channel

	if (returnpayloadsize > payloadAddition) // double check
	{
		memcpy(dst_gpmf, src_gpmf, payloadAddition);
		returnpayloadsize = payloadAddition;
	}
	else
	{
		uint32_t typesizerepeat_compressed;

		typesizerepeat_compressed = GPMF_MAKE_TYPE_SIZE_COUNT('#', bytesize, (returnpayloadsize - 8) / bytesize);
		dst_gpmf[1] = typesizerepeat_compressed;
	}

#if HUFF_STATS
	sourcesize += payloadAddition;
	storedsize += returnpayloadsize;
#endif

	return returnpayloadsize;
}



uint32_t GPMFWriteGetPayloadAndSession(size_t ws_handle, uint32_t channel, uint32_t *buffer, uint32_t buffer_size,
								  uint32_t **payload, uint32_t *payloadsize,
								  uint32_t **session, uint32_t *sessionsize, int session_reduction)
{
	uint32_t *newpayload = NULL;
	uint32_t estimatesize = 0,j;

	device_metadata *dm, *dmnext;
	GPMFWriterWorkspace *ws = (GPMFWriterWorkspace *)ws_handle;

	if (ws == NULL) return GPMF_ERROR_MEMORY;

	if (payload)
		estimatesize = GPMFWriteEstimateBufferSize(ws_handle, channel, 0);
	if (session)
		estimatesize += GPMFWriteEstimateBufferSize(ws_handle, channel, session_reduction);

	if(buffer_size < estimatesize)
	{
		DBG_MSG("GPMFWriteGetPayloadAndSession: not enough buffer to work with\n");
		return GPMF_ERROR_MEMORY;
	}


	if (estimatesize == 0)
	{
		if (payloadsize)
			*payloadsize = 0;
		if (sessionsize)
			*sessionsize = 0;
		return GPMF_ERROR_EMPTY_DATA;
	}

	Lock(&ws->metadata_device_list[channel]);	//Prevent device list changes while extracting data from the current device list

	
	newpayload = (uint32_t *)buffer;

	for(j=0; j<2; j++)
	{
		uint32_t totalsize = 0;
		int freebuffers = 0;
		uint32_t session_scale = 0;

		switch (j)
		{
		case 0: // MP4 payload
			if(payload == NULL)
				continue;

			if(session == NULL)
				freebuffers = 1;
			break;
		case 1: // Session payload
			session_scale = session_reduction;
			if(session == NULL)
				continue;
			
			freebuffers = 1;
			break;
		}

		dm = ws->metadata_devices[channel];

		{
			uint32_t last_deviceID = 0;
			uint32_t *ptr = newpayload;
			uint32_t devicesizebytes = 0, *lastdevicesizeptr = NULL;

			dm = ws->metadata_devices[channel];
			// Copy in all the metadata, formatted into the new buffer
			while(dm && (payload || session))
			{
				Lock(&dm->device_lock); // Get data and return, minimal processing within the lock
				//if(dm->payload_curr_size > 0) // Store information of all connected devices even if they have sent no data
				{
					uint32_t streamsizebytes = 0, *laststreamsizeptr = NULL;
					size_t namelen;
					uint32_t namlen4byte;

					if(dm->device_id != last_deviceID && dm->device_id != GPMF_DEVICE_ID_PREFORMATTED) // Store device name and id and the begin of the data as a device
					{
					//	uint32_t *src_lptr = (uint32_t *)dm->payload_buffer;
						last_deviceID = dm->device_id;

						if(lastdevicesizeptr) //write the size field for the end of the last device
						{
							uint32_t chunksize = GetChunkSize(devicesizebytes);
							uint32_t devicechunks = (devicesizebytes + chunksize - 1) / chunksize;
							uint32_t extrapad = 0;

							*lastdevicesizeptr = MAKEID(0, chunksize, devicechunks >> 8, devicechunks & 0xff);
						
							extrapad = (devicechunks*chunksize - devicesizebytes) >> 2;

							//totalsize += devicesizebytes;
							totalsize += devicechunks * chunksize;
							devicesizebytes = 0;

							while (extrapad && extrapad < chunksize)
							{
								*ptr++ = GPMF_KEY_END;
								extrapad--;
							}
							
						}

						{

							// New device for the new device
							*ptr++ = GPMF_KEY_DEVICE; // nested device to speed the parsing of multiple devices in post 
							lastdevicesizeptr = ptr;  *ptr++ = 0;		// device size to be calculated and updates at the end. 
							totalsize += 8;

							//New Device ID -- GoPro source have a known ID
							*ptr++ = GPMF_KEY_DEVICE_ID;
							if (GPMF_VALID_FOURCC(dm->device_id))
							{
								*ptr++ = MAKEID('F', 4, 0, 1);
								*ptr++ = dm->device_id;
							}
							else
							{
								*ptr++ = MAKEID('L', 4, 0, 1);
								*ptr++ = BYTESWAP32(dm->device_id);

							}
							devicesizebytes += 12;

							//String name for the device
							namelen = strlen(dm->device_name);
							*ptr++ = GPMF_KEY_DEVICE_NAME;
							*ptr++ = MAKEID('c', 1, 0, namelen);
							namlen4byte = (namelen + 3) & ~3;
							memset((char *)ptr, 0, namlen4byte);
							strncpy_s((char *)ptr, sizeof(dm->device_name), dm->device_name, namelen);
							ptr += namlen4byte >> 2;
							devicesizebytes += 8 + namlen4byte;

							//Tick for the payload start (or higher precision MP4 timeing.)
							if (dm->device_id == GPMF_DEVICE_ID_CAMERA && dm->channel != GPMF_CHANNEL_SETTINGS)
							{
								uint32_t lowest_tick = 0;
								device_metadata *dmtick = ws->metadata_devices[channel];

								while (dmtick)
								{
									if (dmtick->payload_tick != 0)
										if (lowest_tick == 0 || lowest_tick > dmtick->payload_tick)
											lowest_tick = dmtick->payload_tick;
									dmtick = dmtick->next;
								}

								if (lowest_tick > 0)
								{
									*ptr++ = GPMF_KEY_TICK;
									*ptr++ = MAKEID('L', 4, 0, 1);
									*ptr++ = BYTESWAP32(lowest_tick);
									devicesizebytes += 12;
								}

							//	*ptr++ = GPMF_KEY_VERSION;
							//	*ptr++ = MAKEID('B', 1, 0, 3);
							//	*ptr++ = GPMF_VERS;
							//	devicesizebytes += 12;
							}
						}
					}
					
					if(dm->payload_curr_size == 0 && dm->device_id != GPMF_DEVICE_ID_PREFORMATTED) // no current payload, has the device stopped?
					{
						if (dm->last_nonsticky_fourcc != 0 && session_scale == 0)
						{
							dm->payload_buffer[0] = dm->last_nonsticky_fourcc;
							dm->payload_buffer[1] = dm->last_nonsticky_typesize & 0xffff;
							dm->payload_buffer[2] = GPMF_KEY_END;
							dm->payload_curr_size = 8;		
							
							{  // indicate the a device has disconnected
								uint32_t buf[4];
								buf[0] = GPMF_KEY_EMPTY_PAYLOADS; 
								buf[1] = MAKEID('L', 4, 0, 1);
								buf[2] = BYTESWAP32(1);
								buf[3] = GPMF_KEY_END;

								AppendFormattedMetadata(dm, buf, 12, (uint32_t)GPMF_FLAGS_STICKY_ACCUMULATE|GPMF_FLAGS_LOCKED, 1, 0); // Timing is Sticky, only one value per data stream, it is simpy updated if sent more than once.
							}
						}						
					} 
					
					

					// Wrap telemetry in a New Stream (or channel) if the payload has sticky metadata
					if (dm->payload_sticky_curr_size > 0)
					{
						*ptr++ = GPMF_KEY_STREAM; // nested device to speed the parsing of multiple devices in post 
						laststreamsizeptr = ptr;  *ptr++ = 0;		// device size to be calculated and updates at the end. 
						devicesizebytes += 8; 
						
						if(dm->totalTimeStampCount != 0)
						{
							uint32_t swap64timestamp[2];
							uint64_t *ptr64 = (uint64_t *)&swap64timestamp[0];

							if(dm->totalTimeStampCount > 5)  // remove jitter if we have enough timestamps
							{
								uint32_t sample;
								uint64_t timestamp;

								double slope, intercept, top = 0.0, bot = 0.0, meanX = 0, meanY = 0;

								for (sample = 0; sample < dm->totalTimeStampCount; sample++)
								{
									meanY += (double)dm->microSecondTimeStamp[sample];
									meanX += (double)sample;
								}
								meanY /= (double)dm->totalTimeStampCount;
								meanX /= (double)dm->totalTimeStampCount;

								for (sample = 0; sample < dm->totalTimeStampCount; sample++)
								{
									top += ((double)sample - meanX)*((double)dm->microSecondTimeStamp[sample] - meanY);
									bot += ((double)sample - meanX)*((double)sample - meanX);
								}
								slope = top / bot;
								intercept = meanY - slope*meanX;
								
								timestamp = (uint64_t)intercept;
								*ptr64 = BYTESWAP64(timestamp);
							}
							else
							{
								*ptr64 = BYTESWAP64(dm->microSecondTimeStamp[0]);
							}
							
							*ptr++ = GPMF_KEY_TIME_STAMP;
							*ptr++ = MAKEID('J', 8, 0, 1);
							*ptr++ = swap64timestamp[0];
							*ptr++ = swap64timestamp[1];
							
							devicesizebytes += 16;
							streamsizebytes += 16;
						}

						//Sticky Metadata for each stream
						if (session_scale == 0) // copy all Sticky data
						{
							memcpy(ptr, dm->payload_sticky_buffer, ((dm->payload_sticky_curr_size + 3)&~3));
							devicesizebytes += ((dm->payload_sticky_curr_size + 3)&~3);
							streamsizebytes += ((dm->payload_sticky_curr_size + 3)&~3);
							ptr += (dm->payload_sticky_curr_size + 3) >> 2;
						}
						else
						{
							uint32_t tag; 
							uint32_t *src_lptr = (uint32_t *)dm->payload_sticky_buffer;

							do
							{
								tag = src_lptr[0];
								if (tag == GPMF_KEY_EMPTY_PAYLOADS || tag == GPMF_KEY_TOTAL_SAMPLES) // meaningless in Session files
								{
									src_lptr += (8 + GPMF_DATA_SIZE(src_lptr[1])) >> 2;
								}
								else if(GPMF_VALID_FOURCC(tag))
								{
									int bytes = (8 + GPMF_DATA_SIZE(src_lptr[1]));
									memcpy(ptr, src_lptr, bytes);
									ptr += bytes >> 2;
									src_lptr += bytes >> 2;
									devicesizebytes += bytes;
									streamsizebytes += bytes;
								}

							} while (GPMF_VALID_FOURCC(tag));
						}
					}
					
					

					
					if (dm->payload_curr_size > 0)
					{
						uint32_t *src_lptr = (uint32_t *)dm->payload_buffer;
						uint8_t *src_bptr = (uint8_t *)dm->payload_buffer;

						//copy the preformatted device metadata into the output buffer
						if (session_scale == 0)
						{
#if SCAN_GPMF_FOR_STATE
							uint32_t payload_curr_size = SeekEndGPMF(src_lptr, dm->payload_alloc_size);
#else
							uint32_t payload_curr_size = dm->payload_curr_size;
#endif
							int payloadAddition = (payload_curr_size + 3)&~3;	
							
							if (dm->quantize && payloadAddition > 100)
								payloadAddition = GPMFCompress(ptr, dm->payload_buffer, payloadAddition, dm->quantize);
							else
								memcpy(ptr, dm->payload_buffer, payloadAddition);					
							devicesizebytes += payloadAddition;
							streamsizebytes += payloadAddition;
							ptr += (payloadAddition >> 2);
						}
						else
						{
							uint32_t last_tag = 0;
							uint32_t tag = src_lptr[0];
							do
							{
								uint32_t samples = GPMF_SAMPLES(src_lptr[1]);
								if ((samples >= (session_scale * 2) && session_scale) || GPMF_SAMPLE_TYPE(src_lptr[1]) == GPMF_TYPE_NEST || tag == last_tag) //DAN20160609 Scale data that is twice or more the the target sample rate.
								{
									int64_t averagebuf[10];
									double *d_averagebuf = (double *)averagebuf;
									int count = 0, average = 0;
									float		*favg = (float *)averagebuf;
									int32_t		*lavg = (int32_t *)averagebuf;
									uint32_t	*Lavg = (uint32_t *)averagebuf;
									int16_t		*savg = (int16_t *)averagebuf;
									uint16_t	*Savg = (uint16_t *)averagebuf;
									uint32_t newscale = (samples + (session_scale / 2)) / session_scale;
									uint8_t *bptr = (uint8_t *)ptr;
									int samples_out = 0;
									int sample_size = GPMF_SAMPLE_SIZE(src_lptr[1]);
									int sample_type = GPMF_SAMPLE_TYPE(src_lptr[1]);

									if (GPMF_SAMPLE_TYPE(src_lptr[1]) == GPMF_TYPE_NEST || tag == last_tag)
									{
										src_lptr += (8 + GPMF_DATA_SIZE(src_lptr[1])) >> 2;
									}
									else
									{
										memcpy(bptr, src_bptr, 8);
										bptr += 8;
										src_bptr += 8;
										src_lptr += (8 + GPMF_DATA_SIZE(src_lptr[1])) >> 2;


										if (sizeof(averagebuf) > sample_size &&
											(sample_type == GPMF_TYPE_FLOAT || sample_type == GPMF_TYPE_SIGNED_SHORT || sample_type == GPMF_TYPE_UNSIGNED_SHORT || sample_type == GPMF_TYPE_SIGNED_LONG || sample_type == GPMF_TYPE_UNSIGNED_LONG))
										{
											average = 1;

											memset(averagebuf, 0, sizeof(averagebuf)), count = 0;
										}

										if (newscale <= 1) newscale = 2;				//DAN20160609 support the new Session scaling

										while (samples--)
										{
											if (++dm->session_scale_count >= newscale)  //DAN20160609 support the new Session scaling
											{
												dm->session_scale_count = 0;
												if (average && count)
												{
													int looplen = 0, i;
													switch (sample_type)
													{
													case GPMF_TYPE_FLOAT:
														looplen = sample_size / sizeof(float);
														for (i = 0; i < looplen; i++)
														{
															d_averagebuf[i] /= (double)count;
															favg[i] = (float)d_averagebuf[i];
															Lavg[i] = BYTESWAP32(Lavg[i]);
														}
														break;
													case GPMF_TYPE_SIGNED_SHORT:
														looplen = sample_size / sizeof(short);
														for (i = 0; i < looplen; i++)
														{
															averagebuf[i] /= count;
															savg[i] = (int16_t)BYTESWAP16(averagebuf[i]);
														}
														break;
													case GPMF_TYPE_UNSIGNED_SHORT:
														looplen = sample_size / sizeof(short);
														for (i = 0; i < looplen; i++)
														{
															averagebuf[i] /= count;
															Savg[i] = (uint16_t)BYTESWAP16(averagebuf[i]);
														}
														break;
													case GPMF_TYPE_SIGNED_LONG:
														looplen = sample_size / sizeof(int32_t);
														for (i = 0; i < looplen; i++)
														{
															averagebuf[i] /= count;
															lavg[i] = (int32_t)BYTESWAP32(averagebuf[i]);
														}
														break;
													case GPMF_TYPE_UNSIGNED_LONG:
														looplen = sample_size / sizeof(int32_t);
														for (i = 0; i < looplen; i++)
														{
															averagebuf[i] /= count;
															Lavg[i] = (uint32_t)BYTESWAP32(averagebuf[i]);
														}
														break;
													}
													memcpy(bptr, averagebuf, sample_size);
													memset(averagebuf, 0, sizeof(averagebuf)), count = 0;
												}
												else
												{
													memcpy(bptr, src_bptr, sample_size);
												}

												bptr += sample_size;
												samples_out++;

											}

											if (average)
											{
												int looplen = 0, i;
												switch (sample_type)
												{
												case GPMF_TYPE_FLOAT:
												{
													float *src = (float *)src_bptr;
													uint32_t *Lsrc = (uint32_t *)src_bptr;
													looplen = sample_size / sizeof(float);
													for (i = 0; i < looplen; i++)
													{
														Lsrc[i] = BYTESWAP32(Lsrc[i]);
														d_averagebuf[i] += (double)src[i];
													}
												}
												break;
												case GPMF_TYPE_SIGNED_SHORT:
												{
													int16_t *src = (int16_t *)src_bptr;
													looplen = sample_size / sizeof(short);
													for (i = 0; i < looplen; i++)
													{
														short val = BYTESWAP16(src[i]);
														averagebuf[i] += (int64_t)val;
													}
												}
												break;
												case GPMF_TYPE_UNSIGNED_SHORT:
												{
													uint16_t *src = (uint16_t *)src_bptr;
													looplen = sample_size / sizeof(short);
													for (i = 0; i < looplen; i++)
													{
														unsigned short val = BYTESWAP16(src[i]);
														averagebuf[i] += (int64_t)val;
													}
												}
												break;
												case GPMF_TYPE_SIGNED_LONG:
												{
													int32_t *src = (int32_t *)src_bptr;
													looplen = sample_size / sizeof(int32_t);
													for (i = 0; i < looplen; i++)
													{
														int32_t val = BYTESWAP32(src[i]);
														averagebuf[i] += (int64_t)val;
													}
												}
												break;
												case GPMF_TYPE_UNSIGNED_LONG:
												{
													uint32_t *src = (uint32_t *)src_bptr;
													looplen = sample_size / sizeof(int32_t);
													for (i = 0; i < looplen; i++)
													{
														uint32_t val = BYTESWAP32(src[i]);
														averagebuf[i] += (int64_t)val;
													}
												}
												break;
												}
												count++;
											}
											src_bptr += sample_size;
										}
										ptr[1] = GPMF_MAKE_TYPE_SIZE_COUNT(sample_type, sample_size, samples_out);
										ptr += (8 + sample_size * samples_out + 3) >> 2;
										devicesizebytes += (8 + sample_size * samples_out + 3) & ~3;
										streamsizebytes += (8 + sample_size * samples_out + 3) & ~3;
									}

									src_bptr = (uint8_t *)src_lptr;
								}
								else
								{
									uint32_t datasize = GPMF_DATA_SIZE(src_lptr[1]);
									memcpy(ptr, src_lptr, datasize + 8);  
									devicesizebytes += datasize + 8;
									streamsizebytes += datasize + 8;
									src_lptr += (datasize + 8) >> 2;
									ptr += (datasize + 8) >> 2;
								}

								last_tag = tag;
								tag = src_lptr[0];

							} while (GPMF_VALID_FOURCC(src_lptr[0]));
						}
					}
					
					//write the size field for the end of each stream
					if (laststreamsizeptr)
					{
						if (streamsizebytes < 8) //Empty Stream
						{
							ptr -= 2;
							devicesizebytes -= 8;
						}
						else
						{
							uint32_t chunksize = GetChunkSize(streamsizebytes);
							uint32_t streamchunks = (streamsizebytes + chunksize - 1) / chunksize;
							uint32_t extrapad = 0;

							*laststreamsizeptr = MAKEID(0, chunksize, streamchunks >> 8, streamchunks & 0xff);

							extrapad = (streamchunks*chunksize - streamsizebytes) >> 2;
							chunksize >>= 2;
							while (extrapad && extrapad < chunksize)
							{
								*ptr++ = GPMF_KEY_END;
								devicesizebytes += 4;
								extrapad--;
							}
						}
					}

					if (freebuffers)
					{
						if(dm->payload_curr_size > 0 && dm->device_id != GPMF_DEVICE_ID_PREFORMATTED) // only clear is used for metadata, PREFORMATTED uses this buffer for nested payloads.
						{
							dm->payload_buffer[0] = GPMF_KEY_END;
							dm->payload_curr_size = 0;
							dm->payload_tick = 0;
							dm->microSecondTimeStamp[0] = 0;
							dm->totalTimeStampCount = 0;
						}
					}
				}
				
				dmnext = dm->next;
				Unlock(&dm->device_lock);
				dm = dmnext;
			}

			if(lastdevicesizeptr)//write the size field for the end of the last device
			{

				uint32_t chunksize = GetChunkSize(devicesizebytes);
				uint32_t devicechunks = (devicesizebytes + chunksize - 1) / chunksize;

				*lastdevicesizeptr = MAKEID(0,chunksize,devicechunks>>8,devicechunks&0xff);

				//totalsize += devicesizebytes;
				totalsize += devicechunks * chunksize;
				devicesizebytes = 0;
			}
		}

		
		switch (j)
		{
		case 0: // MP4 payload
			*payload = newpayload;
			*payloadsize = totalsize;

			newpayload += totalsize/sizeof(uint32_t);
			break;
		case 1: // session 
			*session = newpayload;
			*sessionsize = totalsize;
			break;
		}
	}
	Unlock(&ws->metadata_device_list[channel]);

	return GPMF_ERROR_OK;
}



uint32_t GPMFWriteGetPayload(size_t ws_handle, uint32_t channel, uint32_t *buffer, uint32_t buffer_size, uint32_t **payload, uint32_t *size)
{
	return GPMFWriteGetPayloadAndSession(ws_handle, channel, buffer, buffer_size, payload, size, NULL, NULL, 0);
}
			  
								