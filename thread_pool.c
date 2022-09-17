#include <thread_pool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdio.h>


/*
* Macro creates a variable used to enable or disable print statements
* depending on if the application was compiled in either debug or released mode
*/
#ifdef NDEBUG
#define DEBUG_PRINT 0
#define DEBUG_STATIC static
#else
#define DEBUG_PRINT 1
#define DEBUG_STATIC
#endif // NDEBUG

/*
* Enable printing debug messages when in debug mode. To print a non text
* replacement you have to use debug_print("%s\n", "My text") otherwise
* it is used just like any other printf variant
* debug_print("My test %s\n", "more text");
*
* The debug_print_err adds the file and line number to the output for more
* information when wanting to debug.
  */
#define debug_print_err(fmt, ...) \
  do { if (DEBUG_PRINT) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
  __LINE__, __func__, __VA_ARGS__); } while (0)

#define debug_print(fmt, ...) \
do { if (DEBUG_PRINT) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

// Worker objects is a struct containing a pointer to the thpool and the
// thread itself
typedef struct worker_t
{
    int id;
    thrd_t thread;
    thpool_t * thpool;
} worker_t;

// A job is a queued up object containing the function that a woken thread
// should perform.
typedef struct job_t job_t;
struct job_t
{
    job_t * next_job;
    void (* job_function)(void * job_arg);
	void * job_arg;
};

// The work queue contains a list of first come, first served jobs that are
// consumed by the thread pool. When the queue is empty, the threads will
// block until a new job is enqueued.
typedef struct work_queue_t
{
    job_t * job_head;
    job_t * job_tail;
    atomic_uint_fast64_t job_count;
    mtx_t queue_access_mutex;
} work_queue_t;

// The main structure contains pointers to the mutexes and atomic variables
// that maintain synchronization between all the threads
struct thpool_t
{
    uint8_t thread_count;
    atomic_uint_fast8_t workers_alive;
    atomic_uint_fast8_t workers_working;
    atomic_uint_fast8_t thpool_active;

    worker_t ** workers;
    work_queue_t * work_queue;
    mtx_t run_mutex;
    cnd_t run_cond;

    mtx_t wait_mutex;
    cnd_t wait_cond;
};

typedef enum util_verify_t
{
    UV_VALID_ALLOC,
    UV_INVALID_ALLOC
} util_verify_t;


static void thread_pool(worker_t * worker);
static job_t * thpool_dequeue_job(thpool_t * thpool);
static util_verify_t verify_alloc(void * ptr);

/*!
 * @brief Create a threadpool with the `thread_count` number of threads in the
 * pool. This function will block until all threads have been initialized. The
 * initialized threads will execute their work function indefinitely until
 * the destroy function is called. The threads will remain blocking in their
 * work function until a job is enqueued. As soon as a job is enqueued, a thread
 * will be unblocked to execute the task in the queue.
 *
 * @param thread_count Number of threads to spawn. Amount must be greater than
 * 0.
 * @return Pointer to the threadpool object, or NULL if a failure occured.
 */
thpool_t * thpool_init(uint8_t thread_count)
{
    // Return NULL if 0 was passed in
    if (0 == thread_count)
    {
        debug_print("%s", "Attempted to allocate thpool with 0 threads\n");
        return NULL;
    }



    /*
     * Thread pool init
     */
    thpool_t * thpool = (thpool_t *)calloc(1, sizeof(thpool_t));
    if (UV_INVALID_ALLOC == verify_alloc(thpool))
    {
        return NULL;
    }



    /*
     * Workers init
     */
    thpool->workers = (worker_t **)calloc(thread_count, sizeof(worker_t *));
    if (UV_INVALID_ALLOC == verify_alloc(thpool->workers))
    {
        free(thpool);
        return NULL;
    }



    /*
     * Init work queue
     */
    work_queue_t * work_queue = (work_queue_t *)calloc(1, sizeof(work_queue_t));
    if (UV_INVALID_ALLOC == verify_alloc(work_queue))
    {
        thpool_destroy(&thpool);
        return NULL;
    }



    /*
     * Init Mutexes and conditional variables
     */
    int result = 0;
    result = mtx_init(&thpool->run_mutex, mtx_plain);
    if (thrd_success != result)
    {
        debug_print_err("%s", "Unable to init run_mutex\n");
        thpool_destroy(&thpool);
        return NULL;
    }
    result = cnd_init(&thpool->run_cond);
    if (thrd_success != result)
    {
        debug_print_err("%s", "Unable to init run_cond\n");
        thpool_destroy(&thpool);
        return NULL;
    }
    result = mtx_init(&work_queue->queue_access_mutex, mtx_plain);
    if (thrd_success != result)
    {
        debug_print_err("%s", "Unable to init queue_access\n");
        thpool_destroy(&thpool);
        return NULL;
    }
    result = mtx_init(&thpool->wait_mutex, mtx_plain);
    if (thrd_success != result)
    {
        debug_print_err("%s", "Unable to init wait_mutex\n");
        thpool_destroy(&thpool);
        return NULL;
    }
    result = cnd_init(&thpool->wait_cond);
    if (thrd_success != result)
    {
        debug_print_err("%s", "Unable to init wait_cond\n");
        thpool_destroy(&thpool);
        return NULL;
    }



    /*
     * Assign vars to thpool object
     */
    thpool->thpool_active = 1;
    thpool->workers_alive = 0;
    thpool->workers_working = 0;
    thpool->work_queue = work_queue;
    thpool->thread_count = thread_count;



    /*
     * Threads init
     */
    for (uint8_t i = 0; i < thread_count; i++)
    {
        thpool->workers[i] = (worker_t *)malloc(sizeof(worker_t));
        worker_t * worker = thpool->workers[i];
        if (UV_INVALID_ALLOC == verify_alloc(worker))
        {
            thpool_destroy(&thpool);
            return NULL;
        }

        worker->thpool = thpool;
        worker->id = i;
        result = thrd_create(&(worker->thread),
                             (thrd_start_t)thread_pool, worker);
        if (thrd_success != result)
        {
            debug_print_err("%s", "Unable to create a thread for the thread pool\n");
            thpool_destroy(&thpool);
            return NULL;

        }
	    result = thrd_detach(worker->thread);
        if (thrd_success != result)
        {
            debug_print_err("%s", "Unable to detach thread\n");
            thpool_destroy(&thpool);
            return NULL;
        }

    }



    /*
     * Block until all threads have been initialized
     */
    while (thread_count != atomic_load(&thpool->workers_alive))
    {
        debug_print("[THPOOL] Waiting for threads to init [%d/%d]\n",
                    atomic_load(&thpool->workers_alive), thread_count);
        usleep(200000);
    }
    debug_print("%s\n", "[THPOOL] Thread pool ready\n");
    return thpool;
}

/*!
 * @brief The function will block until all jobs have been consumed and all
 * threads return to their block position waiting for jobs to be enqueued.
 *
 * @param thpool Pointer to the thread pool object
 */
void thpool_wait(thpool_t * thpool)
{
    assert(thpool);

    mtx_lock(&thpool->wait_mutex);
    while ((0 != atomic_load(&thpool->workers_working)) || (0 != atomic_load(&thpool->work_queue->job_count)))
    {
        debug_print("\n[THPOOL] Waiting for threadpool to finish "
                    "[Workers working: %d] || [Jobs in queue: %ld]\n",
                    atomic_load(&thpool->workers_working),
                    atomic_load(&thpool->work_queue->job_count));

        cnd_wait(&thpool->wait_cond, &thpool->wait_mutex);
    }
    debug_print("%s\n", "[THPOOL] Finished waiting...");

    mtx_unlock(&thpool->wait_mutex);
}

/*!
 * @brief Function will signal the threads to exit the work function. The
 * threads already executing their task will be left to finish their task.
 * This will cause the function to block until all threads have exited.
 *
 * @param thpool Reference to the thread pool object **Note the double pointer**
 */
void thpool_destroy(thpool_t ** thpool_ptr)
{
    assert(thpool_ptr);
    thpool_t * thpool = * thpool_ptr;
    assert(thpool);

    // Set running bool to false and set the queue to have a value of 1
    // instructing the threads to look for work.
    atomic_fetch_sub(&thpool->thpool_active, 1);


    // update all the threads until they are done with their tasks
    while (0 != atomic_load(&thpool->workers_alive))
    {
        debug_print("\n[THPOOL] Broadcasting threads to exit...\n"
                    "Workers still alive: %d\n",
                    atomic_load(&thpool->workers_alive));
        cnd_broadcast(&thpool->run_cond);
        usleep(200000);
    }

    // Free any jobs left in the queue that have not been consumed
    job_t * job = thpool->work_queue->job_head;
    job_t * temp;
    while (NULL != job)
    {
        temp = job->next_job;
        job->next_job = NULL;
        free(job);
        job = temp;
    }

    // Free the threads
    for (uint8_t i = 0; i < thpool->thread_count; i++)
    {
        worker_t * worker = thpool->workers[i];
        worker->thpool = NULL;
        free(worker);
    }


    // Free the mutexes
    mtx_destroy(&thpool->run_mutex);
    cnd_destroy(&thpool->run_cond);
    mtx_destroy(&thpool->work_queue->queue_access_mutex);

    free(thpool->work_queue);
    thpool->work_queue = NULL;

    free(thpool->workers);
    thpool->workers = NULL;

    free(thpool);
    *thpool_ptr = NULL;
}



/*!
 * @brief Enqueue a job into the job queue. As soon as a job is enqueued, the
 * thread pool will signal the threads to start consuming tasks.
 *
 * @param thpool Pointer to the thread pool object
 * @param job_function Function the thread will execute
 * @param job_arg Void pointer passed to the function that the thread will
 * execute.
 * @return THP_SUCCESS for successful enqueue otherwise THP_FAILURE
 */
thpool_status thpool_enqueue_job(thpool_t * thpool, void (* job_function)(void *), void * job_arg)
{
    // No need to check args, args can be NULL
    assert(thpool);
    assert(job_function);

    job_t * job = (job_t *)malloc(sizeof(job_t));
    if (UV_INVALID_ALLOC == verify_alloc(job))
    {
        return THP_FAILURE;
    }

    job->job_arg = job_arg;
    job->job_function = job_function;
    job->next_job = NULL;
    work_queue_t * work_queue = thpool->work_queue;

    mtx_lock(&work_queue->queue_access_mutex);

    // If job queue is empty then assign the new job as the head and tail
    if (0 == atomic_load(&work_queue->job_count))
    {
        work_queue->job_head = job;
        work_queue->job_tail = job;
    }
    else // If work queue HAS jobs already
    {
        work_queue->job_tail->next_job = job;
        work_queue->job_tail = job;
    }
    atomic_fetch_add(&work_queue->job_count, 1);

    // Signal at least one thread that the run condition has changed
    // indicating that a new job has been added to the queue
    debug_print("[THPOOL] New job enqueued. Total jobs in queue: %ld\n",
                atomic_load(&work_queue->job_count));


    mtx_unlock(&work_queue->queue_access_mutex);
    cnd_signal(&thpool->run_cond);
    return THP_SUCCESS;
}


/*!
 * @brief Remove a job from the job queue
 * @param thpool Pointer to the thpool object
 * @return Pointer to the next job object
 */
static job_t * thpool_dequeue_job(thpool_t * thpool)
{
    mtx_lock(&thpool->work_queue->queue_access_mutex);
    work_queue_t * work_queue = thpool->work_queue;

    job_t * work = work_queue->job_head;

    // If there is only one job queued, then prep the queue to point to "none"
    if (1 == atomic_load(&work_queue->job_count))
    {
        work_queue->job_head = NULL;
        work_queue->job_tail = NULL;
        atomic_fetch_sub(&work_queue->job_count, 1);
    }

    // Else if the queue has more than on task left, then update head to
    // point to the next item
    else if (atomic_load(&work_queue->job_count) > 1)
    {
        work_queue->job_head = work->next_job;
        atomic_fetch_sub(&work_queue->job_count, 1);
    }

    mtx_unlock(&work_queue->queue_access_mutex);

    // Signal the threadpool that there are tasks in the queue
    cnd_signal(&thpool->run_cond);
    return work;
}

/*!
 * @brief Function where all threads in the thread pool live. All threads will
 * block while the work queue is empty and the thread pool is active. When
 * ever one of these conditions are no longer true, the thread will wake up.
 * @param worker Pointer to the worker object
 */
static void thread_pool(worker_t * worker)
{
    // Increment the number of threads alive. This is useful to indicate that
    // a thread has successfully init
    atomic_fetch_add(&worker->thpool->workers_alive, 1);
    thpool_t * thpool = worker->thpool;

    while (1 == atomic_load(&thpool->thpool_active))
    {
        mtx_lock(&thpool->run_mutex);

        // Block while the work queue is empty
        while ((0 == atomic_load(&thpool->work_queue->job_count)) && (1 == atomic_load(&thpool->thpool_active)))
        {
            debug_print("[THPOOL] Thread %d waiting for a job...[threads: %d || working: %d]\n",
                        worker->id, thpool->workers_alive, thpool->workers_working);
            cnd_wait(&thpool->run_cond, &thpool->run_mutex);
        }

        // As soon as the thread wakes up, unlock the run lock
        // We do not need it locked for operation
        mtx_unlock(&thpool->run_mutex);

        // Second check to make sure that the woken up thread should
        // execute work logic
        if (0 == atomic_load(&thpool->thpool_active))
        {
            // If there is no more work, signal the wait_cond about no work
            // being available incase it is waiting for the queue to be empty
            if (0 == atomic_load(&thpool->work_queue->job_count))
            {
                cnd_signal(&thpool->wait_cond);
            }
            break;
        }

        // Before beginning work, increment the working thread count
        atomic_fetch_add(&thpool->workers_working, 1);
        debug_print("[THPOOL] Thread %d activated, starting work..."
                    "[threads: %d || working: %d]\n",
                    worker->id,
                    atomic_load(&thpool->workers_alive),
                    atomic_load(&thpool->workers_working));

        /*
         * Fetch a job and execute it
         */
        job_t * job = thpool_dequeue_job(thpool);
        if (NULL != job)
        {
            job->job_function(job->job_arg);
            free(job);
        }

        // Decrement threads working before going back to blocking
        atomic_fetch_sub(&thpool->workers_working, 1);

        debug_print("[THPOOL] Thread %d finished work... [threads: %d || working: %d]\n",
                    worker->id,
                    atomic_load(&thpool->workers_alive),
                    atomic_load(&thpool->workers_working));

        // If there is no more work, signal the wait_cond about no work
        // being available incase it is waiting for the queue to be empty
        if (0 == atomic_load(&thpool->work_queue->job_count))
        {
            cnd_signal(&thpool->wait_cond);
        }
    }

    debug_print("[THPOOL] Thread %d is exiting...[threads: %d || working: %d]\n",
                worker->id,
                atomic_load(&thpool->workers_alive),
                atomic_load(&thpool->workers_working));
    atomic_fetch_sub(&thpool->workers_alive, 1);
    return;
}

static util_verify_t verify_alloc(void * ptr)
{
    if (NULL == ptr)
    {
        debug_print("%s", "[!] Unable to allocate memory");
        return UV_INVALID_ALLOC;
    }
    return UV_VALID_ALLOC;
}
