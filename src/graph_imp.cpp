#include <stdarg.h>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "graph.h"

Graph::Graph(size_t thread_worker_num) : m_thread_worker_num(thread_worker_num) {
    graph_assert(
            thread_worker_num > 0, "thread worker number should be greater than 0");
    size_t max_threads = std::thread::hardware_concurrency();
    if (m_thread_worker_num > max_threads) {
        graph_log_warn(
                "Thread worker number %zu is greater than hardware concurrency %zu, "
                "using %zu threads",
                m_thread_worker_num, max_threads, max_threads);
        m_thread_worker_num = max_threads;
    }
};

void Graph::add_task(const std::string& id, Task task) {
    std::lock_guard<std::mutex> lock(mtx);
    graph_assert(!m_is_freezed, "Graph is freezed, cannot add more nodes!");
    graph_assert(
            m_nodes.find(id) == m_nodes.end(), "Node with id \"%s\" already exists!",
            id.c_str());
    Node* node = new Node(id, task);
    m_nodes[id] = node;
    m_dependency_count[node] = 0;  // Initialize dependency count
}

void Graph::dependency(
        const std::string& fromId, const std::initializer_list<std::string>& toIds) {
    Node* from = m_nodes.at(fromId);
    for (const std::string& toId : toIds) {
        dependency(fromId, toId);
    }
}

void Graph::dependency(const std::string& fromId, const std::string& toId) {
    std::lock_guard<std::mutex> lock(mtx);
    graph_assert(!m_is_freezed, "Graph is freezed, cannot add more dependencies!");
    Node* from = m_nodes.at(fromId);
    Node* to = m_nodes.at(toId);
    //! check double add same dependency
    for (const auto& dep : from->dependencies()) {
        graph_assert(
                dep != to, "Node %s already depends on %s", from->id().c_str(),
                to->id().c_str());
    }
    from->dependency(to);
    m_dependency_count[from]++;  // Increment dependency count for the dependent node
}

Graph::~Graph() {
    for (auto& pair : m_nodes) {
        delete pair.second;
    }
}

bool Graph::valid() {
    std::unordered_set<Node*> visited;
    std::unordered_set<Node*> recStack;
    auto ret = true;

    for (const auto& pair : m_nodes) {
        if (is_cyclic(pair.second, visited, recStack)) {
            graph_log_error(
                    "Graph is invalid: cycle detected at: %s",
                    pair.second->id().c_str());
            ret = false;
        }
    }

    if (!is_connected()) {
        graph_log_error("Graph is invalid: not all nodes are connected");
        ret = false;
    }
    return ret;
}

void Graph::freezed() {
    std::lock_guard<std::mutex> lock(mtx);
    graph_assert(valid(), "Graph is not a invalid!");
    graph_assert(!m_is_freezed, "Graph is already freezed!");
    graph_assert(m_nodes.size() > 0, "No nodes in the graph!");
    m_is_freezed = true;
}

void Graph::execute() {
    graph_assert(m_is_freezed, "Graph is not freezed! please call freezed() first!");

    std::condition_variable cv;
    std::queue<Node*> execution_queue;
    bool finished = false;
    //! copy the dependency count to support multi-execute
    //! because the dependency count will be changed during the execution
    std::unordered_map<Node*, size_t> restore_dependency_count;
    for (const auto& pair : m_dependency_count) {
        restore_dependency_count[pair.first] = pair.second;
    }

    // Initialize execution_queue with nodes that have no dependencies
    {
        std::lock_guard<std::mutex> lock(mtx);
        for (const auto& pair : m_nodes) {
            if (m_dependency_count[pair.second] == 0) {
                execution_queue.push(pair.second);
                graph_log_debug("Pushing %s to queue", pair.second->id().c_str());
            }
        }
    }
    graph_assert(!execution_queue.empty(), "No nodes to execute!");

    graph_log_info("Starting execution");
    //! show all nodes in the graph
    graph_log_debug("Nodes in the graph:");
    for (const auto& pair : m_nodes) {
        if (!pair.second->dependencies().empty()) {
            std::string dependencies_str;
            for (const auto& dep : pair.second->dependencies()) {
                dependencies_str += dep->id() + ", ";
            }
            graph_log_debug(
                    "Node %s, it`s depends: %s", pair.second->id().c_str(),
                    dependencies_str.c_str());
        }
    }

    auto worker = [&]() {
        while (true) {
            Node* node = nullptr;
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [&]() { return finished || !execution_queue.empty(); });
                if (finished && execution_queue.empty()) {
                    return;  // Exit the thread if finished and queue is empty
                }
                if (!execution_queue.empty()) {
                    node = execution_queue.front();
                    execution_queue.pop();
                }
            }
            if (node) {
                //! real work execute the task
                graph_log_debug("Executing %s", node->id().c_str());
                node->task()();
                graph_log_debug("Executed %s", node->id().c_str());
                node->mark_executed();
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    for (const auto& pair : m_nodes) {
                        Node* n = pair.second;
                        auto& dependencies = n->dependencies();
                        if (std::find(dependencies.begin(), dependencies.end(), node) !=
                            dependencies.end()) {
                            m_dependency_count[n]--;
                            if (m_dependency_count[n] == 0 && !n->is_executed()) {
                                execution_queue.push(n);
                                cv.notify_one();
                            }
                        }
                    }
                }
            }
        }
    };

    std::vector<std::thread> threads;
    graph_log_info("Starting %zu worker threads", m_thread_worker_num);
    for (size_t i = 0; i < m_thread_worker_num; ++i) {
        threads.emplace_back(worker);
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        finished = true;
    }
    cv.notify_all();

    for (std::thread& thread : threads) {
        thread.join();
    }

    execution_status();

    //! restore the dependency count and unmark_executed
    for (const auto& pair : restore_dependency_count) {
        m_dependency_count[pair.first] = pair.second;
        pair.first->unmark_executed();
    }
}

bool Graph::is_cyclic(
        Node* node, std::unordered_set<Node*>& visited,
        std::unordered_set<Node*>& recStack) {
    if (recStack.count(node))
        return true;
    if (visited.count(node))
        return false;

    visited.insert(node);
    recStack.insert(node);

    for (Node* neighbor : node->dependencies()) {
        if (is_cyclic(neighbor, visited, recStack)) {
            return true;
        }
    }

    recStack.erase(node);
    return false;
}
bool Graph::is_connected() {
    if (m_nodes.empty()) {
        return true;
    }

    // Create adjacency list for undirected graph representation
    std::unordered_map<Node*, std::unordered_set<Node*>> undirected_graph;
    for (const auto& pair : m_nodes) {
        Node* node = pair.second;
        undirected_graph[node] = std::unordered_set<Node*>();
    }

    // Build the undirected graph
    for (const auto& pair : m_nodes) {
        Node* node = pair.second;
        for (Node* neighbor : node->dependencies()) {
            undirected_graph[node].insert(neighbor);
            undirected_graph[neighbor].insert(
                    node);  // Add reverse edge for undirected graph
        }
    }

    // Perform BFS to check connectivity
    std::unordered_set<Node*> visited;
    std::queue<Node*> q;
    auto start_node = m_nodes.begin()->second;
    q.push(start_node);
    visited.insert(start_node);

    while (!q.empty()) {
        Node* node = q.front();
        q.pop();

        for (Node* neighbor : undirected_graph[node]) {
            if (visited.find(neighbor) == visited.end()) {
                visited.insert(neighbor);
                q.push(neighbor);
            }
        }
    }

    auto ret = visited.size() == m_nodes.size();
    if (!ret) {
        graph_log_error("Graph is not connected, details:");

        //! print all nodes that are not connected
        for (const auto& pair : m_nodes) {
            if (visited.find(pair.second) == visited.end()) {
                graph_log_error("Node %s is not connected", pair.second->id().c_str());
            }
        }
    }
    return ret;
}

void Graph::execution_status() {
    size_t executed_count = 0;
    for (const auto& pair : m_nodes) {
        if (pair.second->is_executed()) {
            ++executed_count;
        }
    }
    graph_log_debug(
            "Execution status: %zu/%zu nodes executed", executed_count, m_nodes.size());
    if (executed_count == m_nodes.size()) {
        graph_log_info("Execution completed successfully");
    } else {
        graph_log_error("some nodes are not executed, details:");
        for (const auto& pair : m_nodes) {
            if (!pair.second->is_executed()) {
                graph_log_error("Node %s is not executed", pair.second->id().c_str());
            }
        }
        graph_throw("Execution failed");
    }
}

namespace {
std::string svsprintf(const char* fmt, va_list ap_orig) {
    int size = 100; /* Guess we need no more than 100 bytes */
    char* p;

    if ((p = (char*)malloc(size)) == nullptr)
        return "svsprintf: malloc failed";

    for (;;) {
        va_list ap;
        va_copy(ap, ap_orig);
        int n = vsnprintf(p, size, fmt, ap);
        va_end(ap);

        if (n < 0)
            return "svsprintf: vsnprintf failed";

        if (n < size) {
            std::string rst(p);
            free(p);
            return rst;
        }

        size = n + 1;

        char* np = (char*)realloc(p, size);
        if (!np) {
            free(p);
            return "svsprintf: realloc failed";
        } else
            p = np;
    }
}
std::string __ssprintf__(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto rst = svsprintf(fmt, ap);
    va_end(ap);
    return rst;
}
GraphLogLevel g_log_level = GraphLogLevel::DEBUG;

//! define a default log handler by printf
void default_log_handler(
        GraphLogLevel level, const char* file, const char* func, int line,
        const char* fmt, va_list ap) {
    if (level < g_log_level)
        return;

    const char* level_str = nullptr;
    switch (level) {
        case GraphLogLevel::DEBUG:
            level_str = "DEBUG";
            break;
        case GraphLogLevel::INFO:
            level_str = "INFO";
            break;
        case GraphLogLevel::WARN:
            level_str = "WARN";
            break;
        case GraphLogLevel::ERROR:
            level_str = "ERROR";
            break;
        default:
            level_str = "UNKNOWN";
            break;
    }
    printf("[%s] %s:%d %s: ", level_str, file, line, func);
    vprintf(fmt, ap);
    printf("\n");
}

GraphLogHandler g_log_handler = default_log_handler;
}  // anonymous namespace

void Graph::__assert_fail__(
        const char* file, int line, const char* func, const char* expr,
        const char* msg_fmt, ...) {
    std::string msg;
    if (msg_fmt) {
        va_list ap;
        va_start(ap, msg_fmt);
        msg = "\nextra message: ";
        msg.append(svsprintf(msg_fmt, ap));
        va_end(ap);
    }
    msg = __ssprintf__(
            "assertion `%s' failed at %s:%d: %s%s", expr, file, line, func,
            msg.c_str());
    graph_throw(msg.c_str());
}

void Graph::__log__(
        GraphLogLevel level, const char* file, const char* func, int line,
        const char* fmt, ...) {
    if (!g_log_handler)
        return;
    va_list ap;
    va_start(ap, fmt);
    g_log_handler(level, file, func, line, fmt, ap);
    va_end(ap);
}

GraphLogHandler Graph::config_logger(GraphLogHandler handler) {
    GraphLogHandler old = g_log_handler;
    g_log_handler = handler;
    return old;
}

void Graph::config_log_level(GraphLogLevel level) {
    g_log_level = level;
}
