/*--------------------------------------------------------------------
This source distribution is placed in the public domain by its author,
Jason Papadopoulos. You may use it for any purpose, free of charge,
without having to notify anyone. I disclaim any responsibility for any
errors.

Optionally, please be nice and tell me if you find this source to be
useful. Again optionally, if you add to the functionality present here
please consider making those additions public too, so that others may 
benefit from your work.	

$Id$
--------------------------------------------------------------------*/

#ifndef _THREAD_H_
#define _THREAD_H_

#include <util.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* mutexes ---------------------------------------------------------*/

#if defined(WIN32) || defined(_WIN64)
typedef HANDLE mutex_t;
#else
typedef pthread_mutex_t mutex_t;
#endif

static INLINE void mutex_init(mutex_t *m)
{
#if defined(WIN32) || defined(_WIN64)
	*m = CreateMutex(NULL, FALSE, NULL);
#else
	pthread_mutex_init(m, NULL);
#endif
}

static INLINE void mutex_free(mutex_t *m)
{
#if defined(WIN32) || defined(_WIN64)
	CloseHandle(*m);
#else
	pthread_mutex_destroy(m);
#endif
}

static INLINE void mutex_lock(mutex_t *m)
{
#if defined(WIN32) || defined(_WIN64)
	WaitForSingleObject(*m, INFINITE);
#else
	pthread_mutex_lock(m);
#endif
}

static INLINE void mutex_unlock(mutex_t *m)
{
#if defined(WIN32) || defined(_WIN64)
	ReleaseMutex(*m);
#else
	pthread_mutex_unlock(m);
#endif
}

/* a thread pool --------------------------------------------------*/

typedef void (*threadpool_func)(void *data, int thread_num);

struct threadpool* threadpool_init(int num_threads);

int threadpool_add_task(struct threadpool *pool, 
			threadpool_func func, void *data,
			int blocking);

void threadpool_free(struct threadpool *pool);


#ifdef __cplusplus
}
#endif

#endif /* !_THREAD_H_ */
