#ifndef JG_NETCALC_SRC_THREAD_POOL_THREAD_POOL_H_
#define JG_NETCALC_SRC_THREAD_POOL_THREAD_POOL_H_

#ifdef __cplusplus
extern "C" {
#endif //END __cplusplus
#include <stdint.h>
#include <threads.h>

typedef enum
{
    THP_SUCCESS,
    THP_FAILURE
} thpool_status;

typedef struct thpool_t thpool_t;

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
thpool_t * thpool_init(uint8_t thread_count);

/*!
 * @brief Function will signal the threads to exit the work function. The
 * threads already executing their task will be left to finish their task.
 * This will cause the function to block until all threads have exited.
 *
 * @param thpool Reference to the thread pool object **Note the double pointer**
 */
void thpool_destroy(thpool_t ** thpool);

/*!
 * @brief The function will block until all jobs have been consumed and all
 * threads return to their block position waiting for jobs to be enqueued.
 *
 * @param thpool Pointer to the thread pool object
 */
void thpool_wait(thpool_t * thpool);

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
thpool_status thpool_enqueue_job(thpool_t * thpool, void (* job_function)(void *), void * job_arg);


#ifdef __cplusplus
}
#endif //END __cplusplus
#endif //JG_NETCALC_SRC_THREAD_POOL_THREAD_POOL_H_
