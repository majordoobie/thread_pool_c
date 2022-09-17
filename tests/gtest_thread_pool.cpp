#include <gtest/gtest.h>
#include <thread_pool.h>

void work_func_one(void * arg)
{
    std::atomic_int64_t * val = (std::atomic_int64_t *)arg;
    std::atomic_fetch_add(val, 10);
    // Sleep for a fifth of a second to encourage other threads to start
    // work; otherwise the same thread would get the jobs since it will go
    // by too quickly
    usleep(200000);
}

class ThreadPoolTextFixture : public ::testing::Test
{
 public:
    thpool_t * thpool;
 protected:
    void SetUp() override
    {
        this->thpool = thpool_init(8);
    }
    void TearDown() override
    {
        thpool_destroy(&(this->thpool));
    }
};

TEST_F(ThreadPoolTextFixture, TestInit)
{
    EXPECT_NE(this->thpool, nullptr);
}

TEST_F(ThreadPoolTextFixture, TestVarUpdating)
{
    std::atomic_int64_t * val = (std::atomic_int64_t *)calloc(1, sizeof(std::atomic_int64_t));
    thpool_enqueue_job(this->thpool, work_func_one, val);
    thpool_enqueue_job(this->thpool, work_func_one, val);
    thpool_wait(this->thpool);
    EXPECT_EQ(std::atomic_load(val), 20);
    free(val);
}

TEST_F(ThreadPoolTextFixture, TestMultiUpdating)
{
    std::atomic_int64_t * val = (std::atomic_int64_t *)calloc(1, sizeof(std::atomic_int64_t));

    int i = 0;
    while (i < 1000)
    {
        thpool_enqueue_job(this->thpool, work_func_one, val);
        i = i + 10;
    }

    thpool_wait(this->thpool);
    EXPECT_EQ(std::atomic_load(val), i);
    free(val);
}

void callback(void * data)
{
    std::atomic_uint_fast64_t * val = (std::atomic_uint_fast64_t *)data;
    printf("The atomic value is set to %ld\n", std::atomic_fetch_add(val, 1));
}

TEST(README_DEMO, README_DEMO_TEST)
{
    std::atomic_uint_fast64_t * val = (std::atomic_uint_fast64_t *)calloc(1, sizeof(std::atomic_uint_fast64_t));
    thpool_t * thpool = thpool_init(4);

    for (int i = 0; i < 20; i++)
    {
        thpool_enqueue_job(thpool, callback, val);
    }

    thpool_wait(thpool);
    thpool_destroy(&thpool);
    free(val);
}



