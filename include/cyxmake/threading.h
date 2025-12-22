/**
 * @file threading.h
 * @brief Cross-platform threading primitives for multi-agent support
 *
 * Provides thread, mutex, condition variable, and thread pool abstractions
 * that work on Windows (CreateThread/CriticalSection) and POSIX (pthreads).
 */

#ifndef CYXMAKE_THREADING_H
#define CYXMAKE_THREADING_H

#include "cyxmake/compat.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef CYXMAKE_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Platform-Specific Type Definitions
 * ============================================================================ */

#ifdef CYXMAKE_WINDOWS

typedef HANDLE ThreadHandle;
typedef CRITICAL_SECTION MutexHandle;
typedef CONDITION_VARIABLE ConditionHandle;

/* Thread function signature: DWORD WINAPI func(LPVOID arg) */
typedef DWORD (WINAPI *ThreadFunc)(LPVOID);

#else /* POSIX */

typedef pthread_t ThreadHandle;
typedef pthread_mutex_t MutexHandle;
typedef pthread_cond_t ConditionHandle;

/* Thread function signature: void* func(void* arg) */
typedef void* (*ThreadFunc)(void*);

#endif

/* ============================================================================
 * Thread Operations
 * ============================================================================ */

/**
 * Create and start a new thread
 *
 * @param handle Pointer to store the thread handle
 * @param func Thread entry function
 * @param arg Argument to pass to the thread function
 * @return true on success, false on failure
 */
bool thread_create(ThreadHandle* handle, ThreadFunc func, void* arg);

/**
 * Wait for a thread to complete
 *
 * @param handle Thread handle to wait on
 * @return true on success, false on failure
 */
bool thread_join(ThreadHandle handle);

/**
 * Detach a thread (let it clean up automatically when done)
 *
 * @param handle Thread handle to detach
 * @return true on success, false on failure
 */
bool thread_detach(ThreadHandle handle);

/**
 * Get the current thread's ID as a unique value
 *
 * @return Platform-specific thread ID
 */
unsigned long thread_current_id(void);

/**
 * Sleep the current thread
 *
 * @param milliseconds Time to sleep in milliseconds
 */
void thread_sleep(unsigned int milliseconds);

/* ============================================================================
 * Mutex Operations
 * ============================================================================ */

/**
 * Initialize a mutex
 *
 * @param mutex Pointer to the mutex to initialize
 * @return true on success, false on failure
 */
bool mutex_init(MutexHandle* mutex);

/**
 * Destroy a mutex and release resources
 *
 * @param mutex Pointer to the mutex to destroy
 */
void mutex_destroy(MutexHandle* mutex);

/**
 * Lock a mutex (blocking)
 *
 * @param mutex Pointer to the mutex to lock
 */
void mutex_lock(MutexHandle* mutex);

/**
 * Unlock a mutex
 *
 * @param mutex Pointer to the mutex to unlock
 */
void mutex_unlock(MutexHandle* mutex);

/**
 * Try to lock a mutex without blocking
 *
 * @param mutex Pointer to the mutex
 * @return true if lock acquired, false if already locked
 */
bool mutex_trylock(MutexHandle* mutex);

/* ============================================================================
 * Condition Variable Operations
 * ============================================================================ */

/**
 * Initialize a condition variable
 *
 * @param cond Pointer to the condition variable
 * @return true on success, false on failure
 */
bool condition_init(ConditionHandle* cond);

/**
 * Destroy a condition variable
 *
 * @param cond Pointer to the condition variable
 */
void condition_destroy(ConditionHandle* cond);

/**
 * Wait on a condition variable (must hold mutex)
 *
 * @param cond Pointer to the condition variable
 * @param mutex Pointer to the associated mutex (must be locked)
 */
void condition_wait(ConditionHandle* cond, MutexHandle* mutex);

/**
 * Wait on a condition variable with timeout
 *
 * @param cond Pointer to the condition variable
 * @param mutex Pointer to the associated mutex (must be locked)
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return true if signaled, false if timeout
 */
bool condition_timedwait(ConditionHandle* cond, MutexHandle* mutex,
                         unsigned int timeout_ms);

/**
 * Signal one waiting thread
 *
 * @param cond Pointer to the condition variable
 */
void condition_signal(ConditionHandle* cond);

/**
 * Signal all waiting threads
 *
 * @param cond Pointer to the condition variable
 */
void condition_broadcast(ConditionHandle* cond);

/* ============================================================================
 * Thread Pool
 * ============================================================================ */

/**
 * Thread pool for executing tasks asynchronously
 */
typedef struct ThreadPool ThreadPool;

/**
 * Task function signature for thread pool
 */
typedef void (*TaskFunc)(void* arg);

/**
 * Task completion callback
 */
typedef void (*TaskCallback)(void* result, void* user_data);

/**
 * Create a thread pool
 *
 * @param num_threads Number of worker threads (0 = auto based on CPU cores)
 * @return New thread pool or NULL on failure
 */
ThreadPool* thread_pool_create(int num_threads);

/**
 * Destroy a thread pool and wait for pending tasks
 *
 * @param pool Thread pool to destroy
 */
void thread_pool_free(ThreadPool* pool);

/**
 * Submit a task to the thread pool
 *
 * @param pool Thread pool
 * @param func Task function to execute
 * @param arg Argument to pass to the task
 * @return true on success, false if pool is shutting down
 */
bool thread_pool_submit(ThreadPool* pool, TaskFunc func, void* arg);

/**
 * Submit a task with completion callback
 *
 * @param pool Thread pool
 * @param func Task function to execute
 * @param arg Argument to pass to the task
 * @param callback Function to call when task completes
 * @param user_data Data to pass to callback
 * @return true on success, false if pool is shutting down
 */
bool thread_pool_submit_with_callback(ThreadPool* pool, TaskFunc func, void* arg,
                                      TaskCallback callback, void* user_data);

/**
 * Wait for all submitted tasks to complete
 *
 * @param pool Thread pool
 */
void thread_pool_wait_all(ThreadPool* pool);

/**
 * Get number of pending tasks
 *
 * @param pool Thread pool
 * @return Number of tasks waiting to be executed
 */
size_t thread_pool_pending_count(ThreadPool* pool);

/**
 * Get number of worker threads
 *
 * @param pool Thread pool
 * @return Number of worker threads
 */
int thread_pool_thread_count(ThreadPool* pool);

/**
 * Get number of CPU cores (for auto-sizing thread pool)
 *
 * @return Number of logical CPU cores
 */
int thread_get_cpu_count(void);

/* ============================================================================
 * Atomic Operations (for lock-free counters)
 * ============================================================================ */

/**
 * Atomic counter type
 */
typedef struct {
#ifdef CYXMAKE_WINDOWS
    volatile LONG value;
#else
    volatile int value;
#endif
} AtomicInt;

/**
 * Initialize an atomic integer
 */
void atomic_init(AtomicInt* atomic, int value);

/**
 * Atomically increment and return new value
 */
int atomic_increment(AtomicInt* atomic);

/**
 * Atomically decrement and return new value
 */
int atomic_decrement(AtomicInt* atomic);

/**
 * Atomically load the current value
 */
int atomic_load(AtomicInt* atomic);

/**
 * Atomically store a value
 */
void atomic_store(AtomicInt* atomic, int value);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_THREADING_H */
