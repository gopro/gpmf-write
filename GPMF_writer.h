/*! @file GPMF_writer.h
 *
 *	@brief metadata control library include
 *	
 *	@version 1.1.0
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
 */

#ifndef _GPMF_WRITER_H
#define _GPMF_WRITER_H


#define GPMF_VERS_MAJOR	(1)
#define GPMF_VERS_MINOR	(1)
#define GPMF_VERS_POINT	(0)

#define GPMF_VERS	(GPMF_VERS_MAJOR<<0)|(GPMF_VERS_MINOR<<8)|(GPMF_VERS_POINT<<16)

#include "threadlock.h"
#include "GPMF_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GPMF_EXT_PERFORMATTED_STREAMS	4	// maximum number of external performatted GPMF streams

#define GPMF_DEVICE_ID_CAMERA           1	// primary camera's should use this ID for all internal sensor data.
#define GPMF_DEVICE_ID_PREFORMATTED     0xFFFFFFFF	// use as a device_id if nesting payload data a output from MetadataGetPayload() -- RARE

typedef enum
{
	GPMF_CHANNEL_TIMED = 0, // primary payload gathering time changing metadata for MP4|MOV track storage
	GPMF_CHANNEL_SETTINGS,	// Any other independent GPMF storage, like global setting that aren't changing with time (like video mode.)
	GPMF_CHANNEL_MAX
} MetadataChannel;


#define MAX_TIMESTAMPS	50	//per payload, which is typically at 1Hz

typedef struct device_metadata
{
	struct device_metadata *next;
	struct device_metadata *prev;
	size_t ws_handle;
	uint32_t memory_allocated;
	LOCK device_lock; // Replace with sysyem native semphore
	uint32_t channel;
	uint32_t device_id;
	uint32_t payload_tick;
	char device_name[80];
	uint32_t *payload_buffer;
	uint32_t payload_alloc_size;
	uint32_t payload_curr_size;
	uint32_t *payload_sticky_buffer;
	uint32_t payload_sticky_alloc_size;
	uint32_t payload_sticky_curr_size;
	uint32_t *payload_aperiodic_buffer;
	uint32_t payload_aperiodic_alloc_size;
	uint32_t payload_aperiodic_curr_size;
	uint32_t auto_device_id;
	uint32_t session_scale_count;
	uint32_t last_nonsticky_fourcc;
	uint32_t last_nonsticky_typesize;
	char complex_type[256]; // Maximum structure size for a sample is 255 bytes.
	uint64_t microSecondTimeStamp[MAX_TIMESTAMPS+1];
	uint64_t totalTimeStampCount;
} device_metadata;

#define GPMF_STICKY_PAYLOAD_SIZE			256	// can be increased if need
#define GPMF_APERIODIC_PAYLOAD_SIZE			256 // temporary buffers
#define GPMF_OVERHEAD						(sizeof(device_metadata) + GPMF_STICKY_PAYLOAD_SIZE + GPMF_APERIODIC_PAYLOAD_SIZE)

#define GPMF_GLOBAL_STICKY_PAYLOAD_SIZE		1024 // global has more sticky data.
#define GPMF_GLOBAL_APERIODIC_PAYLOAD_SIZE	32   // not used much for global
#define GPMF_GLOBAL_OVERHEAD				(sizeof(device_metadata) + GPMF_GLOBAL_STICKY_PAYLOAD_SIZE + GPMF_GLOBAL_APERIODIC_PAYLOAD_SIZE)


#define GPMF_ERROR_OK				0
#define GPMF_ERROR_DEVICE			1
#define GPMF_ERROR_MEMORY			2
#define GPMF_ERROR_STICKY_MEMORY	3
#define GPMF_ERROR_EMPTY_DATA		4
#define GPMF_ERROR_STRUCTURE		5

#define GPMF_FLAGS_NONE					0
#define GPMF_FLAGS_STICKY				1  // This metadata should be presented in every payload, will only initializing once, like UNIT, SCAL, etc. 
#define GPMF_FLAGS_BIG_ENDIAN			2  // Indiciate whether source data is pre-formatted as big endian
#define GPMF_FLAGS_GROUPED				4  // CV data like face coordinates, could have multiple entries per frame. Use GROUPED when samples are a the some time and are sent together
#define GPMF_FLAGS_ACCUMULATE			8  // Special case for Sticky data 
#define GPMF_FLAGS_STICKY_ACCUMULATE	9  // Same as sticky, but adding to the previous value
#define GPMF_FLAGS_APERIODIC			16 // Internal : Some CV extracted metadata may take computation time, this is an internal flag used by MetadataStreamAperiodic...().
#define GPMF_FLAGS_DONT_COUNT			32 // Internal : Some CV extracted metadata may take computation time, this is an internal flag used by MetadataStreamAperiodic...().
#define GPMF_FLAGS_SORTED				64 // Special case for non-sticky global data, data is presort by the quanity of the first field. 
												// e.g. a machine vision confident value could autopriority the stored data (and trucate if storage is limited low priority data.) 
#define GPMF_FLAGS_STORE_ALL_TIMESTAMPS	128	// Generally don't use this. This would be if you sensor is has no peroidic times, yet precision is required, or for debugging.  
#define GPMF_FLAGS_ADD_TICK				256	// Generally don't use this. This is for emulating old style GoPro metadata that used a Millisecond tick from the OS timer.							

#define GPMF_FLAGS_LOCKED 				(1<<31) //Metadata Internal use only



/* GPMFWriteServiceInit
*
* Called once to initialize and return the handle for managing the list of metadata streams
*
* @retval handle to the GPMF Service
*/
size_t GPMFWriteServiceInit();


/* GPMFWriteServiceClose
*
* Free resources used for managing the metadata streams. 
* Called only after all individual streams have been closed.
*
* @param[in] handle returned by GPMFWriteServiceInit()
*
* @retval none
*/
void GPMFWriteServiceClose(size_t ws_handle);


/* GPMFWriteSetScratchBuffer
*
* Optional:  An additional pool for memory to handle any sensor overflowing it pre-allocated buffer.
*
* @param[in] ws_handle returned by GPMFWriteServiceInit()
* @param[in] buffer externally allocated buffer where data will be copied to.*
* @param[in] buffer_size the size of the buffer.*
*
* @retval error code
*/
uint32_t GPMFWriteSetScratchBuffer(size_t ws_handle, uint32_t *buffer, uint32_t buffer_size);


/* GPMFWriteStreamOpen
*
* Open a new stream for a particular device, a device may have mulitple streams/sensors 
* (e.g. the GoPro IMU can have ACCL, GRYO and MAGN). 
* Open with device ID and name, used for internal telemetry and third party contented devices. 
* The estimated buffer size in bytes per second. 
*
* @param[in] ws_handle returned by GPMFWriteServiceInit()
* @param[in] channel to indicate the type of metadata
* @param[in] device_id, user provided device ID, or NULL for auto-assigned
* @param[in] buffer pointer to the external buffer to use, or NULL for internally allocated memory
* @param[in] buffer_size Size of buffer passed, or minimum size needed for estimated sensor data.
*
* @retval handle to the new stream
*/
size_t GPMFWriteStreamOpen(
	size_t ws_handle,		//handle returned by GPMFWriteServiceInit()
	uint32_t channel,		//GPMF_CHANNEL_TIMED or GPMF_CHANNEL_SETTINGS
	uint32_t device_id,		//NULL for auto-assigned, or pass in your own number, like IDs share a DEVC 
	char *device_name,		//ASCII name for the device to be stored within DVNM
	char *buffer,			//external buffer to use, or NULL for internally allocated memory
	uint32_t buffer_size	//Size of buffer passed, or minimum size needed for estimated sensor data.
);


/* GPMFWriteStreamReset
*
* Reset stream for a particular device, clear any stale data from an earlier capture. 
*
* @param[in] dm_handle returned by GPMFWriteStreamOpen()
*
* @retval none
*/
void GPMFWriteStreamReset(
	size_t dm_handle
);

/* GPMFWriteStreamStore
*
* Send RAW sensor data to be formatted for storing within the MP4 text track 
*
* @param[in] dm_handle returned by GPMFWriteStreamOpen()
* @param[in] FourCC Tag/Key of the new data
* @param[in] data_type of GPMF_SampleType of the new data
* @param[in] sample_size is the number of bytes in each sample (gryo data "x,y,z" is one sample might be 6 bytes.)
* @param[in] sample_count is the number of samples to store.
* @param[in] data is a pointer to the array of samples
* @param[in] flags set the type of storage e.g. GPMF_FLAGS_STICKY
*
* @retval error code
*/
uint32_t GPMFWriteStreamStore(  
	size_t dm_handle,		//handle returned by GPMFWriteStreamOpen()
	uint32_t tag,			//FOURCC Tag/Key
	uint32_t data_type,
	uint32_t sample_size, 
	uint32_t sample_count,
	void *data, 
	uint32_t flags					// e.g. GPMF_FLAGS_STICKY
);

//Send RAW data to be formatted for storing within the MP4 text track with a µs timestamp of the first sample within the write.


/* GPMFWriteStreamStoreStamped  (avoid)
* 
* Send RAW sensor data to be formatted for storing within the MP4 text track, but requiring 
* additional precision in timing (the need is very rare.) 
*
* @param[in] dm_handle returned by GPMFWriteStreamOpen()
* @param[in] FourCC Tag/Key of the new data
* @param[in] data_type of GPMF_SampleType of the new data
* @param[in] sample_size is the number of bytes in each sample (gryo data "x,y,z" is one sample might be 6 bytes.)
* @param[in] sample_count is the number of samples to store.
* @param[in] data is a pointer to the array of samples
* @param[in] flags set the type of storage e.g. GPMF_FLAGS_STICKY
* @param[in] microSecondTimeStamp a microsecond time stamp from a single clock/soucre for all sensor using this call.
*
* @retval error code
*/
uint32_t GPMFWriteStreamStoreStamped(
	size_t dm_handle,
	uint32_t tag, 
	uint32_t data_type,
	uint32_t sample_size, 
	uint32_t sample_count,
	void *data, 
	uint32_t flags,					// e.g. GPMF_FLAGS_STICKY
	uint64_t microSecondTimeStamp	// if zero, this is the same call as GPMFWriteStreamStore()
);


/* GPMFWriteStreamAperiodicBegin
*
* Mark the beginning of computed data, or slow data, where the sample time 
* is long as compared with the payload time.
* This opens a nested GPMF group so the multiple entries can represet one moment in time 
* (e.g. n-objects in one frame.)
*
* @param[in] dm_handle returned by GPMFWriteStreamOpen()
* @param[in] FourCC Tag/Key of the new data
*
* @retval error code
*/
uint32_t GPMFWriteStreamAperiodicBegin(  
	size_t dm_handle,
	uint32_t tag
);

//Send objects/event one a time or at once after calling ...CVOpen
/* GPMFWriteStreamAperiodicStore
*
* Send objects/events one a time or at once after calling GPMFWriteStreamAperiodicBegin()
*
* @param[in] dm_handle returned by GPMFWriteStreamOpen()
* @param[in] FourCC Tag/Key of the new data
* @param[in] data_type of GPMF_SampleType of the new data
* @param[in] sample_size is the number of bytes in each sample (gryo data "x,y,z" is one sample might be 6 bytes.)
* @param[in] sample_count is the number of samples to store.
* @param[in] data is a pointer to the array of samples
*
* @retval error code
*/
uint32_t GPMFWriteStreamAperiodicStore(  
	size_t dm_handle,
	uint32_t tag, 
	uint32_t data_type,
	uint32_t sample_size, 
	uint32_t sample_count,
	void *data
);

/* GPMFWriteStreamAperiodicBegin
*
* Mark the end of computed data, or slow data, where the sample time
* is long as compared with the payload time. Used after GPMFWriteStreamAperiodicBegin()
* and GPMFWriteStreamAperiodicStore()
*
* @param[in] dm_handle returned by GPMFWriteStreamOpen()
* @param[in] FourCC Tag/Key of the new data
*
* @retval error code
*/
uint32_t GPMFWriteStreamAperiodicEnd(
	size_t dm_handle,
	uint32_t tag
);



/* GPMFWriteStreamClose
*
* Close if a device is disconnected by the user (e.g. user turns the GPS off, 
* or disconnects an external Blutooth device.)
*
* @param[in] dm_handle returned by GPMFWriteStreamOpen()
*
* @retval pointer to any memory not freed
*/
void *GPMFWriteStreamClose(size_t dm_handle); 


/* GPMFWriteTypeSize
*
* Returned the byte size of any GPMF type
*
* @param[in] type of GPMF_SampleType
*
* @retval pointer to any memory not freed
*/
int32_t GPMFWriteTypeSize(int type);


/* GPMFWriteGetPayload
*
* Called for each payload to be sent to the MP4.
*
* @param[in] ws_handle returned by GPMFWriteServiceInit()
* @param[in] channel to indicate the type of metadata
* @param[in] buffer externally allocated buffer where data will be copied to.*
* @param[in] buffer_size the size of the buffer.*
* @param[out] payload pointer to the payload (in this function, it will always point to the buffer passed in)
* @param[out] size the size of returned payload
*
* @retval error code
*/
uint32_t GPMFWriteGetPayload(size_t ws_handle, uint32_t channel, uint32_t *buffer, uint32_t buffer_size, uint32_t **payload, uint32_t *size);


/* GPMFWriteGetPayloadAndSession
*
* Called for each payload to be sent to the MP4 and/or Session File (with optional sampling reduction), returns pointers to pre-alloc'd memory and its sizes.
*
* @param[in] ws_handle returned by GPMFWriteServiceInit()
* @param[in] channel to indicate the type of metadata
* @param[in] buffer externally allocated buffer where data will be copied to.*
* @param[in] buffer_size the size of the buffer.*
* @param[out] payload pointer to the payload (in this function, it will always point to the buffer passed in)
* @param[out] size the size of returned payload*
* @param[out] session pointer to the session payload*
* @param[out] size the size of returned session payload*
* @param[in] reduction reduction scaler of the session data.
*
* @retval error code
*/
uint32_t GPMFWriteGetPayloadAndSession(size_t ws_handle, uint32_t channel, uint32_t *buffer, uint32_t buffer_size,
	uint32_t **payload, uint32_t *size,
	uint32_t **session, uint32_t *sessionsize, int session_reduction); // reduction is a target sample rate, anything at least twice this is reduced to this rate  


/* GPMFWriteIsValidGPMF
*
* Test if the data is a completed GPMF structure starting with DEVC
*
* @param[in] buffer pointer to GPMF formatted data
* @param[in] byte_size the size of the data in the buffer.
* @param[in] recurse set non-zero is recurve through GPMF levels.
*
* @retval error code
*/
uint32_t GPMFWriteIsValidGPMF(uint32_t *buffer, uint32_t byte_size, uint32_t recurse);


/* GPMFWriteEstimateBufferSize
*
* Test if the data is a completed GPMF structure starting with DEVC
*
* @param[in] ws_handle  returned by GPMFWriteServiceInit()
* @param[in] channel to indicate the type of metadata
* @param[in] payloadscale if you using session_reduction, otherwise set to zero.
*
* @retval number of bytes estimated for storing the GPMF payload
*/
uint32_t GPMFWriteEstimateBufferSize(size_t ws_handle, uint32_t channel, uint32_t payloadscale);




//GPMF Writer service calling proceedure
/*
{
	size_t gpmfhandle = 0;
	if (gpmfhandle = GPMFWriteServiceInit())
	{
 
		//flush any stale sensor data
		GPMFWriteGetPayload(..);

		handleA = GPMFWriteStreamOpen(gpmfhandle, ...);
		handleB = GPMFWriteStreamOpen(gpmfhandle, ...);
		...

		//Write sticky data
		GPMFWriteStreamStore(handleA, ..., GPMF_FLAGS_STICKY);
		GPMFWriteStreamStore(handleA, ..., GPMF_FLAGS_STICKY);
		GPMFWriteStreamStore(handleB, ..., GPMF_FLAGS_STICKY);

		// In a device thread(s)
		while(SensorA)			
			GPMFWriteStreamStore(handleA, ...);

		while(SensorB)
			GPMFWriteStreamStore(handleB, ...);

		// In another thread
		while(recording)
		{
			GPMFWriteGetPayload(..);// to flush any stale sensor data
			StoreInMP4TextTrack(..);// your file muxing code.
		}

		//Stop recording and store any remaining sensor data
		GPMFWriteGetPayload(..);// to flush any stale sensor data
		StoreInMP4TextTrack(..);// your file muxing code.
		//Close the MP4

		GPMFWriteServiceClose();
	}
}
*/

//Each metadata source use this calling proceedure, likely in a separate thread
/*
{
	//While the sensor is connected
	for(each packet from the sensor)
		GPMFWriteStreamStore(handleA, ...)

	//when the device is finally disconnected
	GPMFWriteStreamClose(handle);
}
*/

#ifdef __cplusplus
}
#endif

#endif
