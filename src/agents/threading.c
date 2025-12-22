/**
 * @file threading.c
 * @brief Cross-platform threading implementation
 *
 * Implements thread, mutex, condition variable, and thread pool for
 * Windows (CreateThread/CriticalSection) and POSIX (pthreads).
 */

#include "cyxmake/threading.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>

#ifdef CYXMAKE_WINDOWS
    #include <process.h>
#else
    #include <unistd.h>
    #include <sys/time.h>
    #include <errno.h>
#endif

/* ============================================================================
 * Thread Operations
 * ============================================================================ */

bool thread_create(ThreadHandle* handle, ThreadFunc func, void* arg) {
    if (!handle || !func) {
        return false;
    }

#ifdef CYXMAKE_WINDOWS
    *handle = CreateThread(
        NULL,           /* Default security attributes */
        0,              /* Default stack size */
        func,           /* Thread function */
        arg,            /* Argument to thread function */
        0,              /* Creation flags (run immediately) */
        NULL            /* Thread ID (not needed) */
    );
    return (*handle != NULL);
#else
    int result = pthread_create(handle, NULL, func, arg);
    return (result == 0);
#endif
}

bool thread_join(ThreadHandle handle) {
#ifdef CYXMAKE_WINDOWS
    if (handle == NULL) {
        return false;
    }
    DWORD result = WaitForSingleObject(handle, INFINITE);
    if (result == WAIT_OBJECT_0) {
        CloseHandle(handle);
        return true;
    }
    return false;
#else
    int result = pthread_join(handle, NULL);
    return (result == 0);
#endif
}

bool thread_detach(ThreadHandle handle) {
#ifdef CYXMAKE_WINDOWS
    if (handle == NULL) {
        return false;
    }
    return CloseHandle(handle) != 0;
#else
    int result = pthread_detach(handle);
    return (result == 0);
#endif
}

unsigned long thread_current_id(void) {
#ifdef CYXMAKE_WINDOWS
    return (unsigned long)GetCurrentThreadId();
#else
    return (unsigned long)pthread_self();
#endif
}

void thread_sleep(unsigned int milliseconds) {
#ifdef CYXMAKE_WINDOWS
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

/* ============================================================================
 * Mutex Operations
 * ============================================================================ */

bool mutex_init(MutexHandle* mutex) {
    if (!mutex) {
        return false;
    }

#ifdef CYXMAKE_WINDOWS
    InitializeCriticalSection(mutex);
    return true;
#else
    int result = pthread_mutex_init(mutex, NULL);
    return (result == 0);
#endif
}

void mutex_destroy(MutexHandle* mutex) {
    if (!mutex) {
        return;
    }

#ifdef CYXMAKE_WINDOWS
    DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}

void mutex_lock(MutexHandle* mutex) {
    if (!mutex) {
        return;
    }

#ifdef CYXMAKE_WINDOWS
    EnterCriticalSection(mutex);
#else
    pthread_mutex_lock(mutex);
#endif
}

void mutex_unlock(MutexHandle* mutex) {
    if (!mutex) {
        return;
    }

#ifdef CYXMAKE_WINDOWS
    LeaveCriticalSection(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif
}

bool mutex_trylock(MutexHandle* mutex) {
    if (!mutex) {
        return false;
    }

#ifdef CYXMAKE_WINDOWS
    return TryEnterCriticalSection(mutex) != 0;
#else
    int result = pthread_mutex_trylock(mutex);
    return (result == 0);
#endif
}

/* ============================================================================
 * Condition Variable Operations
 * ============================================================================ */

bool condition_init(ConditionHandle* cond) {
    if (!cond) {
        return false;
    }

#ifdef CYXMAKE_WINDOWS
    InitializeConditionVariable(cond);
    return true;
#else
    int result = pthread_cond_init(cond, NULL);
    return (result == 0);
#endif
}

void condition_destroy(ConditionHandle* cond) {
    if (!cond) {
        return;
    }

#ifdef CYXMAKE_WINDOWS
    /* Windows condition variables don't need explicit destruction */
#else
    pthread_cond_destroy(cond);
#endif
}

void condition_wait(ConditionHandle* cond, MutexHandle* mutex) {
    if (!cond || !mutex) {
        return;
    }

#ifdef CYXMAKE_WINDOWS
    SleepConditionVariableCS(cond, mutex, INFINITE);
#else
    pthread_cond_wait(cond, mutex);
#endif
}

bool condition_timedwait(ConditionHandle* cond, MutexHandle* mutex,
                         unsigned int timeout_ms) {
    if (!cond || !mutex) {
        return false;
    }

#ifdef CYXMAKE_WINDOWS
    BOOL result = SleepConditionVariableCS(cond, mutex, timeout_ms);
    return (result != 0);
#else
    struct timespec ts;
    struct timeval tv;
    gettimeofday(&tv, NULL);

    ts.tv_sec = tv.tv_sec + (timeout_ms / 1000);
    ts.tv_nsec = (tv.tv_usec * 1000) + ((timeout_ms % 1000) * 1000000);

    /* Handle nanosecond overflow */
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += ts.tv_nsec / 1000000000;
        ts.tv_nsec = ts.tv_nsec % 1000000000;
    }

    int result = pthread_cond_timedwait(cond, mutex, &ts);
    return (result == 0);
#endif
}

void condition_signal(ConditionHandle* cond) {
    if (!cond) {
        return;
    }

#ifdef CYXMAKE_WINDOWS
    WakeConditionVariable(cond);
#else
    pthread_cond_signal(cond);
#endif
}

void condition_broadcast(ConditionHandle* cond) {
    if (!cond) {
        return;
    }

#ifdef CYXMAKE_WINDOWS
    WakeAllConditionVariable(cond);
#else
    pthread_cond_broadcast(cond);
#endif
}

/* ============================================================================
 * Atomic Operations
 * ============================================================================ */

void atomic_init(AtomicInt* atomic, int value) {
    if (!atomic) {
        return;
    }
    atomic->value = value;
}

int atomic_increment(AtomicInt* atomic) {
    if (!atomic) {
        return 0;
    }

#ifdef CYXMAKE_WINDOWS
    return InterlockedIncrement(&atomic->value);
#else
    return __sync_add_and_fetch(&atomic->value, 1);
#endif
}

int atomic_decrement(AtomicInt* atomic) {
    if (!atomic) {
        return 0;
    }

#ifdef CYXMAKE_WINDOWS
    return InterlockedDecrement(&atomic->value);
#else
    return __sync_sub_and_fetch(&atomic->value, 1);
#endif
}

int atomic_load(AtomicInt* atomic) {
    if (!atomic) {
        return 0;
    }

#ifdef CYXMAKE_WINDOWS
    return InterlockedCompareExchange(&atomic->value, 0, 0);
#else
    return __sync_fetch_and_add(&atomic->value, 0);
#endif
}

void atomic_store(AtomicInt* atomic, int value) {
    if (!atomic) {
        return;
    }

#ifdef CYXMAKE_WINDOWS
    InterlockedExchange(&atomic->value, value);
#else
    __sync_lock_test_and_set(&atomic->value, value);
#endif
}

/* ============================================================================
 * CPU Count Detection
 * ============================================================================ */

int thread_get_cpu_count(void) {
#ifdef CYXMAKE_WINDOWS
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;
#else
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    return (nprocs > 0) ? (int)nprocs : 1;
#endif
}

/* ============================================================================
 * Thread Pool Implementation
 * ============================================================================ */

/* Task node for the work queue */
typedef struct PoolTask {
    TaskFunc func;
    void* arg;
    TaskCallback callback;
    void* user_data;
    struct PoolTask* next;
} PoolTask;

/* Thread pool structure */
struct ThreadPool {
    ThreadHandle* threads;       /* Array of worker threads */
    int thread_count;            /* Number of worker threads */

    PoolTask* task_head;         /* Head of task queue */
    PoolTask* task_tail;         /* Tail of task queue */
    size_t pending_count;        /* Number of pending tasks */

    MutexHandle queue_mutex;     /* Protects task queue */
    ConditionHandle work_cond;   /* Signal when work available */
    ConditionHandle done_cond;   /* Signal when work completed */

    AtomicInt active_count;      /* Number of threads currently working */
    volatile bool shutdown;      /* Shutdown flag */
    volatile bool started;       /* Pool has started */
};

/* Worker thread entry point */
#ifdef CYXMAKE_WINDOWS
static DWORD WINAPI thread_pool_worker(LPVOID arg) {
#else
static void* thread_pool_worker(void* arg) {
#endif
    ThreadPool* pool = (ThreadPool*)arg;

    while (1) {
        PoolTask* task = NULL;

        /* Wait for work or shutdown */
        mutex_lock(&pool->queue_mutex);

        while (pool->task_head == NULL && !pool->shutdown) {
            condition_wait(&pool->work_cond, &pool->queue_mutex);
        }

        if (pool->shutdown && pool->task_head == NULL) {
            mutex_unlock(&pool->queue_mutex);
            break;
        }

        /* Dequeue task */
        task = pool->task_head;
        if (task) {
            pool->task_head = task->next;
            if (pool->task_head == NULL) {
                pool->task_tail = NULL;
            }
            pool->pending_count--;
        }

        mutex_unlock(&pool->queue_mutex);

        if (task) {
            atomic_increment(&pool->active_count);

            /* Execute the task */
            task->func(task->arg);

            /* Call completion callback if provided */
            if (task->callback) {
                task->callback(NULL, task->user_data);
            }

            free(task);

            atomic_decrement(&pool->active_count);

            /* Signal if all work is done */
            mutex_lock(&pool->queue_mutex);
            if (pool->task_head == NULL && atomic_load(&pool->active_count) == 0) {
                condition_broadcast(&pool->done_cond);
            }
            mutex_unlock(&pool->queue_mutex);
        }
    }

#ifdef CYXMAKE_WINDOWS
    return 0;
#else
    return NULL;
#endif
}

ThreadPool* thread_pool_create(int num_threads) {
    if (num_threads <= 0) {
        num_threads = thread_get_cpu_count();
        if (num_threads <= 0) {
            num_threads = 2;
        }
    }

    ThreadPool* pool = (ThreadPool*)calloc(1, sizeof(ThreadPool));
    if (!pool) {
        log_error("Failed to allocate thread pool");
        return NULL;
    }

    pool->thread_count = num_threads;
    pool->task_head = NULL;
    pool->task_tail = NULL;
    pool->pending_count = 0;
    pool->shutdown = false;
    pool->started = false;
    atomic_init(&pool->active_count, 0);

    /* Initialize synchronization primitives */
    if (!mutex_init(&pool->queue_mutex)) {
        log_error("Failed to initialize thread pool mutex");
        free(pool);
        return NULL;
    }

    if (!condition_init(&pool->work_cond)) {
        log_error("Failed to initialize work condition variable");
        mutex_destroy(&pool->queue_mutex);
        free(pool);
        return NULL;
    }

    if (!condition_init(&pool->done_cond)) {
        log_error("Failed to initialize done condition variable");
        condition_destroy(&pool->work_cond);
        mutex_destroy(&pool->queue_mutex);
        free(pool);
        return NULL;
    }

    /* Allocate thread handles */
    pool->threads = (ThreadHandle*)calloc(num_threads, sizeof(ThreadHandle));
    if (!pool->threads) {
        log_error("Failed to allocate thread handles");
        condition_destroy(&pool->done_cond);
        condition_destroy(&pool->work_cond);
        mutex_destroy(&pool->queue_mutex);
        free(pool);
        return NULL;
    }

    /* Create worker threads */
    for (int i = 0; i < num_threads; i++) {
        if (!thread_create(&pool->threads[i], thread_pool_worker, pool)) {
            log_error("Failed to create worker thread %d", i);
            /* Shutdown already created threads */
            pool->shutdown = true;
            condition_broadcast(&pool->work_cond);
            for (int j = 0; j < i; j++) {
                thread_join(pool->threads[j]);
            }
            free(pool->threads);
            condition_destroy(&pool->done_cond);
            condition_destroy(&pool->work_cond);
            mutex_destroy(&pool->queue_mutex);
            free(pool);
            return NULL;
        }
    }

    pool->started = true;
    log_debug("Thread pool created with %d workers", num_threads);
    return pool;
}

void thread_pool_free(ThreadPool* pool) {
    if (!pool) {
        return;
    }

    /* Signal shutdown */
    mutex_lock(&pool->queue_mutex);
    pool->shutdown = true;
    condition_broadcast(&pool->work_cond);
    mutex_unlock(&pool->queue_mutex);

    /* Wait for all threads to finish */
    for (int i = 0; i < pool->thread_count; i++) {
        thread_join(pool->threads[i]);
    }

    /* Free remaining tasks */
    PoolTask* task = pool->task_head;
    while (task) {
        PoolTask* next = task->next;
        free(task);
        task = next;
    }

    /* Clean up */
    free(pool->threads);
    condition_destroy(&pool->done_cond);
    condition_destroy(&pool->work_cond);
    mutex_destroy(&pool->queue_mutex);
    free(pool);

    log_debug("Thread pool destroyed");
}

bool thread_pool_submit(ThreadPool* pool, TaskFunc func, void* arg) {
    return thread_pool_submit_with_callback(pool, func, arg, NULL, NULL);
}

bool thread_pool_submit_with_callback(ThreadPool* pool, TaskFunc func, void* arg,
                                      TaskCallback callback, void* user_data) {
    if (!pool || !func) {
        return false;
    }

    /* Allocate task */
    PoolTask* task = (PoolTask*)malloc(sizeof(PoolTask));
    if (!task) {
        log_error("Failed to allocate pool task");
        return false;
    }

    task->func = func;
    task->arg = arg;
    task->callback = callback;
    task->user_data = user_data;
    task->next = NULL;

    mutex_lock(&pool->queue_mutex);

    /* Check for shutdown */
    if (pool->shutdown) {
        mutex_unlock(&pool->queue_mutex);
        free(task);
        return false;
    }

    /* Enqueue task */
    if (pool->task_tail) {
        pool->task_tail->next = task;
    } else {
        pool->task_head = task;
    }
    pool->task_tail = task;
    pool->pending_count++;

    /* Signal a worker */
    condition_signal(&pool->work_cond);

    mutex_unlock(&pool->queue_mutex);
    return true;
}

void thread_pool_wait_all(ThreadPool* pool) {
    if (!pool) {
        return;
    }

    mutex_lock(&pool->queue_mutex);

    while (pool->task_head != NULL || atomic_load(&pool->active_count) > 0) {
        condition_wait(&pool->done_cond, &pool->queue_mutex);
    }

    mutex_unlock(&pool->queue_mutex);
}

size_t thread_pool_pending_count(ThreadPool* pool) {
    if (!pool) {
        return 0;
    }

    mutex_lock(&pool->queue_mutex);
    size_t count = pool->pending_count;
    mutex_unlock(&pool->queue_mutex);
    return count;
}

int thread_pool_thread_count(ThreadPool* pool) {
    return pool ? pool->thread_count : 0;
}
