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
thpool_t * thpool_init(uint8_t thread_count);
void thpool_wait(thpool_t * thpool);
thpool_status thpool_enqueue_job(thpool_t * thpool, void (* job_function)(void *), void * job_arg);
void thpool_destroy(thpool_t ** thpool);


#ifdef __cplusplus
}
#endif //END __cplusplus
#endif //JG_NETCALC_SRC_THREAD_POOL_THREAD_POOL_H_
