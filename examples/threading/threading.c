#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

// Macro for microseconds to milliseconds conversion
#define USEC_PER_MSEC 1000U

void* threadfunc(void* thread_param)
{
	int ret;

	struct thread_data* thread_func_args = (struct thread_data *) thread_param;

	DEBUG_LOG("Sleeping for %d milliseconds", thread_func_args->timeout_obtain);
	usleep(thread_func_args->timeout_obtain * USEC_PER_MSEC);

	ret = pthread_mutex_lock(thread_func_args->mutex);
	if (ret != 0) {
		ERROR_LOG("Failed to obtain mutex, with error: %d", ret);
		return thread_param;
	}
	DEBUG_LOG("Acquired mutex now, sleeping for %d seconds", thread_func_args->timeout_release);
	usleep(thread_func_args->timeout_release * USEC_PER_MSEC);

	ret = pthread_mutex_unlock(thread_func_args->mutex);
	if (ret != 0) {
		ERROR_LOG("Failed to release mutex, with error: %d", ret);
		return thread_param;
	}
	thread_func_args->thread_complete_success = true;

	return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
	int ret;
	struct thread_data* t_data = malloc(sizeof(struct thread_data));
	if (!t_data)
		return false;
	t_data->mutex = mutex;
	t_data->timeout_obtain = wait_to_obtain_ms;
	t_data->timeout_release = wait_to_release_ms;
	t_data->thread_complete_success = false;

	ret = pthread_create(thread, NULL, threadfunc, (void *)t_data); 
	if (ret != 0) {
		ERROR_LOG("Failed to spwan thread with error: %d", ret);
		free(t_data);
		return false;
	}
	return true; 
}

