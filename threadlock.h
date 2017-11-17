/*! @file threadlock.h
 *
 *  @brief threading library include
 *
 *  @version 1.1.0
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

#ifndef _THREADLOCK_H
#define _THREADLOCK_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


typedef enum
{
	THREAD_ERROR_OKAY = 0,				// No error occurred
	THREAD_ERROR_CREATE_FAILED,			// Failed to create object
	THREAD_ERROR_JOIN_FAILED,			// Wait for thread failed
	THREAD_ERROR_INVALID_ARGUMENT,		// Bad argument passed to a thread routine
	THREAD_ERROR_WAIT_FAILED,			// Wait was abandoned or timed out
	THREAD_ERROR_BAD_STATE,				// Undefined event state
	THREAD_ERROR_DETACH_FAILED,			// Unable to detach a POSIX thread
	THREAD_ERROR_NOWORK,				// No more units of work available
	THREAD_ERROR_NOWORKYET,				// No units cuurently available
	THREAD_ERROR_WORKCOMPLETE,			// All units for all job are complete

} THREAD_ERROR;

#if _WINDOWS

#include <windows.h>

// Macro for declaring routines in the threads API
#define THREAD_API(proc) \
	static __inline THREAD_ERROR proc

typedef struct
{
	CRITICAL_SECTION mutex;
} LOCK;


THREAD_API(CreateLock)(LOCK *lock)
{
	InitializeCriticalSection(&lock->mutex);
	return THREAD_ERROR_OKAY;
}

THREAD_API(DeleteLock)(LOCK *lock)
{
	DeleteCriticalSection(&lock->mutex);
	return THREAD_ERROR_OKAY;
}

THREAD_API(Lock)(LOCK *lock)
{
	EnterCriticalSection(&lock->mutex);
	return THREAD_ERROR_OKAY;
}

THREAD_API(Unlock)(LOCK *lock)
{
	LeaveCriticalSection(&lock->mutex);
	return THREAD_ERROR_OKAY;
}

THREAD_API(GetTick)(uint32_t *tick)
{
	uint32_t systick = (uint32_t) GetTickCount();
	*tick = systick;
	return THREAD_ERROR_OKAY;
}

#elif BUILD_CAMERA_RTOS

#include "rtos_mutex.h"
#include "rtos_time.h"

// Macro for declaring routines in the threads API
#define THREAD_API(proc) \
	static inline THREAD_ERROR proc

typedef struct
{
	rtos_mutex_t mutex;
} LOCK;

THREAD_API(CreateLock)(LOCK *lock)
{
	static int usecount = 0;
	char lockname[15] = "dev_metadata00";
	lockname[12] = (usecount/10) + '0';
	lockname[13] = (usecount%10) + '0';
	lockname[14] = 0;
	usecount++; // keep the name name unque
	rtos_mutex_create(&lock->mutex, lockname);
	return THREAD_ERROR_OKAY;
}

THREAD_API(DeleteLock)(LOCK *lock)
{
	rtos_mutex_delete(&lock->mutex);
	return THREAD_ERROR_OKAY;
}

THREAD_API(Lock)(LOCK *lock)
{
	rtos_mutex_acquire(&lock->mutex, RTOS_WAIT_FOREVER);
	return THREAD_ERROR_OKAY;
}

THREAD_API(Unlock)(LOCK *lock)
{
	rtos_mutex_release(&lock->mutex);
	return THREAD_ERROR_OKAY;
}

THREAD_API(GetTick)(uint32_t *tick)
{
	uint32_t systick = rtos_time_get();
	*tick = systick;
	return THREAD_ERROR_OKAY;
}


#else

#include "pthread.h"

#if __APPLE__
#include "macdefs.h"
#endif

// Macro for declaring routines in the threads API
#define THREAD_API(proc) \
	static inline THREAD_ERROR proc

typedef struct
{
	pthread_mutex_t mutex;

} LOCK;

THREAD_API(CreateLock)(LOCK *lock)
{
	// Initialize a mutex with default attributes
	int result = pthread_mutex_init(&lock->mutex, NULL);
	if (result != 0) {
		return THREAD_ERROR_CREATE_FAILED;
	}

	return THREAD_ERROR_OKAY;
}

THREAD_API(DeleteLock)(LOCK *lock)
{
	pthread_mutex_destroy(&lock->mutex);
	return THREAD_ERROR_OKAY;
}

THREAD_API(Lock)(LOCK *lock)
{
	pthread_mutex_lock(&lock->mutex);
	return THREAD_ERROR_OKAY;
}

THREAD_API(Unlock)(LOCK *lock)
{
	pthread_mutex_unlock(&lock->mutex);
	return THREAD_ERROR_OKAY;
}

THREAD_API(GetTick)(uint32_t *tick)
{
	*tick = 0;
	return THREAD_ERROR_OKAY;
}


#endif



#endif
