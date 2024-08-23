#include <gtest/gtest.h>
#include <thread>
#include "graph.h"

TEST(Graph, init) {
    Graph g;
}

TEST(Graph, config_log_level) {
    Graph g;
    g.config_log_level(GraphLogLevel::INFO);
    g.config_log_level(GraphLogLevel::DEBUG);
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

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
