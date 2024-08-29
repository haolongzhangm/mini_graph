#include <gtest/gtest.h>
#include <thread>
#include "graph.h"

using namespace mini_graph;

namespace {
class Timer {
    using clock = ::std::chrono::high_resolution_clock;
    clock::time_point m_start;

public:
    Timer() { reset(); };

    void reset() { m_start = clock::now(); }

    double get_secs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return ::std::chrono::duration_cast<::std::chrono::nanoseconds>(now - m_start)
                       .count() *
               1e-9;
    }
    double get_msecs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return ::std::chrono::duration_cast<::std::chrono::nanoseconds>(now - m_start)
                       .count() *
               1e-6;
    }
    double get_secs_reset() {
        auto ret = get_secs();
        reset();
        return ret;
    }
    double get_msecs_reset() { return get_secs_reset() * 1e3; }
};

}  // namespace

TEST(Graph, init) {
    Graph g;
}

TEST(Graph, config_log_level) {
    Graph g;
    g.config_log_level(GraphLogLevel::INFO);
    ASSERT_EQ(g.log_level(), GraphLogLevel::INFO);
    g.config_log_level(GraphLogLevel::DEBUG);
    ASSERT_EQ(g.log_level(), GraphLogLevel::DEBUG);
}

TEST(Graph, add_task) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
}

TEST(graph, add_same_task) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    ASSERT_THROW(
            g.add_task("A", []() { std::cout << "Executing A" << std::endl; }),
            std::runtime_error);
}

TEST(Graph, dependency) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });

    g.dependency("A", "B");
}

TEST(Graph, dependency_invalid) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });

    ASSERT_THROW(g.dependency("A", "C"), std::out_of_range);
}

TEST(Graph, valid) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });
    g.add_task("C", []() { std::cout << "Executing C" << std::endl; });

    g.dependency("A", "B");
    g.dependency("B", "C");

    ASSERT_TRUE(g.valid());
}

TEST(Graph, is_not_dag_simple) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });

    g.dependency("A", "B");
    g.dependency("B", "A");

    ASSERT_FALSE(g.valid());
}

TEST(Graph, is_not_dag) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });
    g.add_task("C", []() { std::cout << "Executing C" << std::endl; });

    g.dependency("A", "B");
    g.dependency("B", "C");
    g.dependency("C", "A");

    ASSERT_FALSE(g.valid());
}

TEST(Graph, discrete_nodes) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });

    ASSERT_TRUE(g.valid());
}

TEST(Graph, discrete_nodes_2) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });
    g.add_task("C", []() { std::cout << "Executing c" << std::endl; });

    ASSERT_FALSE(g.valid());
}

TEST(Graph, discrete_nodes_3) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });
    g.add_task("C", []() { std::cout << "Executing c" << std::endl; });
    g.dependency("B", "C");

    ASSERT_FALSE(g.valid());
}

TEST(Graph, execute_empty) {
    Graph g;
    ASSERT_THROW(g.execute(), std::runtime_error);
}

TEST(Graph, execute_without_freeze) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    ASSERT_THROW(g.execute(), std::runtime_error);
}
TEST(Graph, execute_single) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.freezed();
    g.execute();
}

TEST(Graph, execute_single_with_dependency) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });
    g.dependency("A", "B");
    g.freezed();
    g.execute();
}

TEST(Graph, freeze_twice) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.freezed();
    ASSERT_THROW(g.freezed(), std::runtime_error);
}

TEST(Graph, test_without_freeze) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });

    ASSERT_THROW(g.execute(), std::runtime_error);
}

TEST(Graph, add_task_after_freeze) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.freezed();
    ASSERT_THROW(
            g.add_task("B", []() { std::cout << "Executing B" << std::endl; }),
            std::runtime_error);
}

TEST(Graph, add_dependency_after_freeze) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });
    g.add_task("C", []() { std::cout << "Executing C" << std::endl; });
    g.dependency("A", "B");
    g.dependency("A", "C");
    g.freezed();
    ASSERT_THROW(g.dependency("B", "C"), std::runtime_error);
}

TEST(Graph, invalid_freeze) {
    Graph g;
    ASSERT_THROW(g.freezed(), std::runtime_error);
}

TEST(Graph, add_dependency_cycle) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.dependency("A", "A");
    ASSERT_THROW(g.freezed(), std::runtime_error);
}

TEST(Graph, add_dependency_cycle_2) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });
    g.dependency("A", "B");
    g.dependency("B", "A");
    ASSERT_THROW(g.freezed(), std::runtime_error);
}

TEST(Graph, add_dependency_cycle_3) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });
    g.dependency("A", "B");

    ASSERT_THROW(g.dependency("A", "B"), std::runtime_error);
}

TEST(Graph, add_dependency_cycle_4) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });
    g.add_task("C", []() { std::cout << "Executing C" << std::endl; });
    g.add_task("D", []() { std::cout << "Executing D" << std::endl; });
    g.dependency("A", "B");
    g.dependency("B", {"C", "D"});
    g.dependency("C", "A");
    g.dependency("D", "A");

    ASSERT_THROW(g.freezed(), std::runtime_error);
}

TEST(Graph, connected) {
    Graph g;
    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });
    g.add_task("C", []() { std::cout << "Executing C" << std::endl; });
    g.add_task("D", []() { std::cout << "Executing D" << std::endl; });
    g.add_task("E", []() { std::cout << "Executing E" << std::endl; });

    g.dependency("A", {"B", "C"});  // A depends on B and C
    g.dependency("B", {"D"});       // B depends on D
    g.dependency("C", {"D"});       // C depends on D
    g.dependency("A", {"E"});       // A depends on E

    ASSERT_TRUE(g.valid());

    Graph g2;
    g2.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g2.add_task("B", []() { std::cout << "Executing B" << std::endl; });

    ASSERT_FALSE(g2.valid());
    ASSERT_THROW(g2.freezed(), std::runtime_error);
}

TEST(Graph, execute) {
    Graph g;

    g.add_task("A", []() { std::cout << "Executing A" << std::endl; });
    g.add_task("B", []() { std::cout << "Executing B" << std::endl; });
    g.add_task("C", []() { std::cout << "Executing C" << std::endl; });
    g.add_task("D", []() { std::cout << "Executing D" << std::endl; });
    g.add_task("E", []() { std::cout << "Executing E" << std::endl; });

    g.dependency("A", {"B", "C"});  // A depends on B and C
    g.dependency("B", {"D"});       // B depends on D
    g.dependency("C", {"D"});       // C depends on D
    g.dependency("A", {"E"});       // A depends on E

    g.freezed();
    g.execute();

    //! run again
    g.execute();
}

TEST(Graph, check_value_1) {
    Graph g;

    int a = 0;

    g.add_task("A", [&a]() { a += 1; });

    ASSERT_EQ(a, 0);
    g.freezed();
    g.execute();
    ASSERT_EQ(a, 1);

    g.execute();
    ASSERT_EQ(a, 2);
}

TEST(Graph, mult_thread_add_task) {
    Graph g;

    int a = 0;

    auto _ = std::thread([&]() {
        g.add_task("A", [&a]() {
            a += 1;
            std::cout << "Executing A" << std::endl;
        });
    });
    auto _2 = std::thread([&]() {
        g.add_task("B", [&a]() {
            a += 5;
            std::cout << "Executing B" << std::endl;
        });
    });
    _.join();
    _2.join();

    g.dependency("A", "B");

    ASSERT_EQ(a, 0);
    g.freezed();
    g.execute();
    ASSERT_EQ(a, 6);

    g.execute();
    ASSERT_EQ(a, 12);
}

TEST(Graph, check_value_2) {
    Graph g;

    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    int e = 0;

    g.add_task("A", [&]() { a = b + c + d + e; });
    g.add_task("B", [&b]() { b += 1; });
    g.add_task("C", [&c]() { c += 1; });
    g.add_task("D", [&d]() { d += 1; });
    g.add_task("E", [&e]() { e += 1; });

    g.dependency("A", {"B", "C"});  // A depends on B and C
    g.dependency("B", {"D"});       // B depends on D
    g.dependency("C", {"D"});       // C depends on D
    g.dependency("A", {"D"});       // A depends on D
    g.dependency("A", {"E"});       // A depends on E

    g.freezed();
    g.execute();

    ASSERT_EQ(a, 4);
    ASSERT_EQ(b, 1);
    ASSERT_EQ(c, 1);
    ASSERT_EQ(d, 1);
    ASSERT_EQ(e, 1);

    g.execute();
    ASSERT_EQ(a, 8);
    ASSERT_EQ(b, 2);
    ASSERT_EQ(c, 2);
    ASSERT_EQ(d, 2);
    ASSERT_EQ(e, 2);
}

TEST(Graph, task_with_thead_with_sleep) {
    Graph g;

    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    int e = 0;

    g.add_task("A", [&]() {
        printf("A start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        printf("b: %d, c: %d, d: %d, e: %d\n", b, c, d, e);
        a = b + c + d + e;
        printf("A end\n");
    });
    g.add_task("B", [&b]() {
        printf("B start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        b += 1;
        printf("B end\n");
    });
    g.add_task("C", [&c]() {
        printf("C start\n");
        c += 1;
        printf("C end\n");
    });
    g.add_task("D", [&d]() {
        printf("D start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(90));
        d += 1;
        printf("D end\n");
    });
    g.add_task("E", [&e]() {
        printf("E start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        e += 1;
        printf("E end\n");
    });

    g.dependency("A", {"B", "C"});  // A depends on B and C
    g.dependency("B", {"D"});       // B depends on D
    g.dependency("C", {"D"});       // C depends on D
    g.dependency("A", {"D"});       // A depends on D
    g.dependency("A", {"E"});       // A depends on E

    g.freezed();
    g.execute();

    ASSERT_EQ(a, 4);
    ASSERT_EQ(b, 1);
    ASSERT_EQ(c, 1);
    ASSERT_EQ(d, 1);
    ASSERT_EQ(e, 1);

    g.execute();
    ASSERT_EQ(a, 8);
    ASSERT_EQ(b, 2);
    ASSERT_EQ(c, 2);
    ASSERT_EQ(d, 2);
    ASSERT_EQ(e, 2);
}

TEST(Graph, thread_number) {
    ASSERT_THROW(Graph g(0), std::runtime_error);

    size_t max = std::thread::hardware_concurrency();

    {
        Graph g(1);
        ASSERT_EQ(g.thread_worker_num(), 1);
    }

    {
        auto m = std::min(4, (int)max);
        Graph g(m);
        ASSERT_EQ(g.thread_worker_num(), m);
    }
    {
        Graph g(max);
        ASSERT_EQ(g.thread_worker_num(), max);
    }

    {
        Graph g(max + 8);
        ASSERT_EQ(g.thread_worker_num(), max);
    }
}

TEST(Graph, one_thread) {
    Graph g(1);

    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    int e = 0;

    g.add_task("A", [&]() {
        printf("A start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        printf("b: %d, c: %d, d: %d, e: %d\n", b, c, d, e);
        a = b + c + d + e;
        printf("A end\n");
    });
    g.add_task("B", [&b]() {
        printf("B start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        b += 1;
        printf("B end\n");
    });
    g.add_task("C", [&c]() {
        printf("C start\n");
        c += 1;
        printf("C end\n");
    });
    g.add_task("D", [&d]() {
        printf("D start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(90));
        d += 1;
        printf("D end\n");
    });
    g.add_task("E", [&e]() {
        printf("E start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        e += 1;
        printf("E end\n");
    });

    g.dependency("A", {"B", "C"});  // A depends on B and C
    g.dependency("B", {"D"});       // B depends on D
    g.dependency("C", {"D"});       // C depends on D
    g.dependency("A", {"D"});       // A depends on D
    g.dependency("A", {"E"});       // A depends on E

    g.freezed();
    g.execute();

    ASSERT_EQ(a, 4);
    ASSERT_EQ(b, 1);
    ASSERT_EQ(c, 1);
    ASSERT_EQ(d, 1);
    ASSERT_EQ(e, 1);

    g.execute();
    ASSERT_EQ(a, 8);
    ASSERT_EQ(b, 2);
    ASSERT_EQ(c, 2);
    ASSERT_EQ(d, 2);
    ASSERT_EQ(e, 2);
}

TEST(Graph, two_graph) {
    Graph g1(4);
    Graph g2(2);

    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    int e = 0;

    g1.add_task("A", [&]() {
        printf("A start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        printf("b: %d, c: %d, d: %d, e: %d\n", b, c, d, e);
        a = b + c + d + e;
        printf("A end\n");
    });
    g1.add_task("B", [&b]() {
        printf("B start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        b += 1;
        printf("B end\n");
    });
    g1.add_task("C", [&c]() {
        printf("C start\n");
        c += 1;
        printf("C end\n");
    });
    g1.add_task("D", [&d]() {
        printf("D start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(90));
        d += 1;
        printf("D end\n");
    });
    g1.add_task("E", [&e]() {
        printf("E start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        e += 1;
        printf("E end\n");
    });

    g1.dependency("A", {"B", "C"});  // A depends on B and C
    g1.dependency("B", {"D"});       // B depends on D
    g1.dependency("C", {"D"});       // C depends on D
    g1.dependency("A", {"D"});       // A depends on D
    g1.dependency("A", {"E"});       // A depends on E

    g1.freezed();
    g1.execute();

    ASSERT_EQ(a, 4);
    ASSERT_EQ(b, 1);
    ASSERT_EQ(c, 1);
    ASSERT_EQ(d, 1);
    ASSERT_EQ(e, 1);

    g1.execute();
    ASSERT_EQ(a, 8);
    ASSERT_EQ(b, 2);
    ASSERT_EQ(c, 2);
}

TEST(Graph, five_stage_pipeline) {
    //! we define five function to simulate a five stage pipeline to process data
    //! the data will be processed by each stage in order
    //! every stage cal 1024 elements, total 1024 * 5 elements

    std::vector<float> data(1024 * 5, 0);
    std::vector<float> result(1024 * 5, 0);
    std::vector<float> expected(1024 * 5, 0);

    //! init data
    for (size_t i = 0; i < 1024 * 5; i++) {
        data[i] = i;
    }

    //! define four post processing to handle + -> / -> - , * operation
    auto stage_m = [&data, &result](size_t offset, size_t total) {
        for (size_t i = 0; i < total; i++) {
            result[offset + i] = result[offset + i] * 6.0;
        }
    };

    auto stage_s = [&data, &result](size_t offset, size_t total) {
        for (size_t i = 0; i < total; i++) {
            result[offset + i] = result[offset + i] - 5.0;
        }
    };

    auto stage_d = [&data, &result](size_t offset, size_t total) {
        //! sleep 101ms
        std::this_thread::sleep_for(std::chrono::milliseconds(101));
        for (size_t i = 0; i < total; i++) {
            result[offset + i] = result[offset + i] / 4.0;
        }
    };

    auto stage_a = [&data, &result](size_t offset, size_t total) {
        for (size_t i = 0; i < total; i++) {
            result[offset + i] = data[offset + i] + 3.0;
        }
    };

    //! cal naive result
    for (size_t i = 0; i < 1024 * 5; i++) {
        expected[i] = (((data[i] + 3.0) / 4.0) - 5.0) * 6.0;
    }

    Graph g(8);
    g.add_task("M0", [&]() { stage_m(0, 1024); });
    g.add_task("M1", [&]() { stage_m(1024, 1024); });
    g.add_task("M2", [&]() { stage_m(1024 * 2, 1024); });
    g.add_task("M3", [&]() { stage_m(1024 * 3, 1024); });
    g.add_task("M4", [&]() { stage_m(1024 * 4, 1024); });
    g.add_task("S0", [&]() { stage_s(0, 1024); });
    g.add_task("S1", [&]() { stage_s(1024, 1024); });
    g.add_task("S2", [&]() { stage_s(1024 * 2, 1024); });
    g.add_task("S3", [&]() { stage_s(1024 * 3, 1024); });
    g.add_task("S4", [&]() { stage_s(1024 * 4, 1024); });
    g.add_task("D0", [&]() { stage_d(0, 1024); });
    g.add_task("D1", [&]() { stage_d(1024, 1024); });
    g.add_task("D2", [&]() { stage_d(1024 * 2, 1024); });
    g.add_task("D3", [&]() { stage_d(1024 * 3, 1024); });
    g.add_task("D4", [&]() { stage_d(1024 * 4, 1024); });
    g.add_task("A0", [&]() { stage_a(0, 1024); });
    g.add_task("A1", [&]() { stage_a(1024, 1024); });
    g.add_task("A2", [&]() { stage_a(1024 * 2, 1024); });
    g.add_task("A3", [&]() { stage_a(1024 * 3, 1024); });
    g.add_task("A4", [&]() { stage_a(1024 * 4, 1024); });

    //! define dependency
#define DEPENDENCY(stage)                   \
    g.dependency("M" #stage, {"S" #stage}); \
    g.dependency("S" #stage, {"D" #stage}); \
    g.dependency("D" #stage, {"A" #stage});

    DEPENDENCY(0);
    DEPENDENCY(1);
    DEPENDENCY(2);
    DEPENDENCY(3);
    DEPENDENCY(4);

    //! define a virtual node to trigger the pipeline
    g.add_task("end", []() { printf("five_stage_pipeline end\n"); });
    g.virtual_dependency("end", {"M0", "M1", "M2", "M3", "M4"});

    //! simulation nn with single thread by add dependency
    g.virtual_dependency("A0", {"A1"});
    g.virtual_dependency("A1", {"A2"});
    g.virtual_dependency("A2", {"A3"});
    g.virtual_dependency("A3", {"A4"});

    g.freezed();
    g.execute();

    for (size_t i = 0; i < 1024 * 5; i++) {
        // printf("result[%zu]: %f, expected[%zu]: %f\n", i, result[i], i, expected[i]);
        //! assert with 1e-6
        ASSERT_NEAR(result[i], expected[i], 1e-6);
    }

    //! rerun
    g.execute();
}

TEST(Graph, worker_parallel) {
    size_t max = std::thread::hardware_concurrency();
    if (max < 4) {
        return;
    }

    Graph g(4);

    //! create 4 task, each task will sleep 100ms
    g.add_task("A", []() {
        printf("A start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        printf("A end\n");
    });

    g.add_task("B", []() {
        printf("B start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        printf("B end\n");
    });

    g.add_task("C", []() {
        printf("C start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        printf("C end\n");
    });

    g.add_task("D", []() {
        printf("D start\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        printf("D end\n");
    });

    //! create a virtual node to trigger the pipeline
    g.add_task("end", []() { printf("worker_parallel end\n"); });
    g.dependency("end", {"A", "B", "C", "D"});

    g.freezed();
    Timer t;
    double ret_time0 = g.execute();

    //! check time
    double time = t.get_msecs_reset();
    ASSERT_NEAR(time, 100.0, 20.0);

    //! rerun
    double ret_time1 = g.execute();
    time = t.get_msecs_reset();
    ASSERT_NEAR(time, 100.0, 20.0);

    //! check return time
    ASSERT_NEAR(ret_time0, 100.0, 20.0);
    ASSERT_NEAR(ret_time1, 100.0, 20.0);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
