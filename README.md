# thread_pool_c
Thread pool written in C using the cross-platform threads.h library and
atomic functions to control thread synchronization. 

## Dependencies 
The project depends on gtest which is only needed when the `build type` is `DEBUG`.
The dependency is automatically cloned using CMAKE

## How to compile
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build
cmake --build build -j $(nproc)
ctest -j $(nproc) --test-dir build
```

## Example:
```c
void callback(void * data)
{
    atomic_uint_fast64_t * val = (atomic_uint_fast64_t *)data;
    printf("The atomic value is set to %ld\n", atomic_fetch_add(val, 1));
}

int main(void)
{
    atomic_uint_fast64_t * val = (atomic_uint_fast64_t *)calloc(1, sizeof(atomic_uint_fast64_t));
    
    thpool_t * thpool = thpool_init(4);

    for (int i = 0; i < 20; i++)
    {
        thpool_enqueue_job(thpool, callback, val);
    }

    thpool_wait(thpool);
    thpool_destroy(&thpool);
    free(val);
    return 0;
}
```




