#include <stdarg.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#include <sys/system_properties.h>
#elif defined(__OHOS__)
#include <hilog/log.h>
#endif

#ifdef __linux__
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cstring>
#endif

#include "graph.h"

using namespace mini_graph;

double Gtimer::get_secs() const {
    auto now = std::chrono::high_resolution_clock::now();
    return ::std::chrono::duration_cast<::std::chrono::nanoseconds>(now - m_start)
                   .count() *
           1e-9;
}
double Gtimer::get_msecs() const {
    auto now = std::chrono::high_resolution_clock::now();
    return ::std::chrono::duration_cast<::std::chrono::nanoseconds>(now - m_start)
                   .count() *
           1e-6;
}
double Gtimer::get_secs_reset() {
    auto ret = get_secs();
    reset();
    return ret;
}
double Gtimer::get_msecs_reset() {
    return get_secs_reset() * 1e3;
}
void Gtimer::reset() {
    m_start = clock::now();
}
Gtimer::Gtimer() {
    reset();
}

void Node::exec(bool inplace_worker) {
    //! TODO: verify the sched delay on weak cpu if inplace_worker is false
    auto __ = [&]() {
        config();

#ifdef __linux__
        //! get the real cpu mask and priority when DEBUG mode
        if (GraphLogLevel::DEBUG == Graph::log_level()) {
            cpu_set_t cpu_mask;
            CPU_ZERO(&cpu_mask);
            auto ret = sched_getaffinity(0, sizeof(cpu_mask), &cpu_mask);
            if (ret) {
                graph_log_warn(
                        "Node: \"%s\" failed to getaffinity: err: %s (%d)",
                        id().c_str(), strerror(errno), errno);
            } else {
                std::string cpu_ids = "";
                for (int i = 0; i < CPU_SETSIZE; i++) {
                    if (CPU_ISSET(i, &cpu_mask)) {
                        cpu_ids += std::to_string(i) + ",";
                    }
                }
                graph_log_info(
                        "Node: \"%s\" getaffinity: cpu id: %s", id().c_str(),
                        cpu_ids.c_str());
            }

            int priority = getpriority(PRIO_PROCESS, 0);
            if (priority == -1) {
                graph_log_warn(
                        "Node: \"%s\" failed to getpriority: err: %s (%d)",
                        id().c_str(), strerror(errno), errno);
            } else {
                graph_log_info(
                        "Node: \"%s\" getpriority: priority=%d", id().c_str(),
                        priority);
            }
        }

#endif

        task()();
    };

    if (inplace_worker) {
        __();
    } else {
        //! create a new thread to execute the task
        std::thread _(__);
        _.join();
    }
}

void Node::config() {
#ifdef __linux__
    //! config cpu mask
    if (m_cpu_mask) {
        auto ret = syscall(
                __NR_sched_setaffinity, gettid(), sizeof(m_cpu_mask), &m_cpu_mask);
        if (ret) {
            graph_log_warn(
                    "Node: \"%s\" failed to setaffinity: mask=0x%x err: %s (%d)",
                    id().c_str(), m_cpu_mask, strerror(errno), errno);
        } else {
            graph_log_info(
                    "Node: \"%s\" setaffinity: mask=0x%x", id().c_str(), m_cpu_mask);
        }
    }

    //! config priority,
    if (m_priority >= -20 && m_priority <= 19) {
        auto ret = setpriority(PRIO_PROCESS, 0, m_priority);
        if (ret) {
            graph_log_warn(
                    "Node: \"%s\" failed to setpriority: priority=%d err: %s (%d)",
                    id().c_str(), m_priority, strerror(errno), errno);
        } else {
            graph_log_info(
                    "Node: \"%s\" setpriority: priority=%d", id().c_str(), m_priority);
        }
    } else if (m_priority != INT_MAX) {
        graph_log_warn(
                "Node: \"%s\" invalid priority: %d, should be in [-20, 19]",
                id().c_str(), m_priority);
        m_priority = INT_MAX;
    }
#endif
}

Graph::Graph(size_t thread_worker_num, std::string g_name)
        : m_thread_worker_num(thread_worker_num) {
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

    if (g_name.empty()) {
        //! get current time
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        m_name = std::string("graph_") + std::to_string(now_c);
    } else {
        m_name = g_name;
    }

    graph_log_info(
            "Create Graph \"%s\" with %zu thread workers", m_name.c_str(),
            m_thread_worker_num);
};

void Graph::add_task(
        const std::string& id, Task task, unsigned int cpu_mask, int priority) {
    std::lock_guard<std::mutex> lock(mtx);
    graph_assert(!m_is_freezed, "Graph is freezed, cannot add more nodes!");
    graph_assert(
            m_nodes.find(id) == m_nodes.end(), "Node with id \"%s\" already exists!",
            id.c_str());
    Node* node = new Node(id, task);
    node->cpu_mask(cpu_mask);
    node->priority(priority);
    m_nodes[id] = node;
    m_dependency_count[node] = 0;
}

void Graph::dependency(
        const std::string& who, const std::initializer_list<std::string>& depend_whos) {
    for (const std::string& depend_who : depend_whos) {
        dependency(who, depend_who);
    }
}

void Graph::dependency(const std::string& who, const std::string& depend_who) {
    std::lock_guard<std::mutex> lock(mtx);
    graph_assert(!m_is_freezed, "Graph is freezed, cannot add more dependencies!");
    Node* from = m_nodes.at(who);
    Node* to = m_nodes.at(depend_who);
    for (const auto& dep : from->dependencies()) {
        graph_assert(
                dep != to, "Node %s already depends on %s", from->id().c_str(),
                to->id().c_str());
    }
    from->dependency(to);
    m_dependency_count[from]++;
}

bool Graph::valid() {
    std::unordered_set<Node*> visited;
    std::unordered_set<Node*> rec_stack;
    auto ret = true;

    for (const auto& pair : m_nodes) {
        if (is_cyclic(pair.second, visited, rec_stack)) {
            graph_log_error(
                    "Graph is invalid: cycle detected at: %s details:",
                    pair.second->id().c_str());
            Node* node = pair.second;
            std::string dependencies_str;
            while (rec_stack.size() > 0) {
                dependencies_str.clear();
                for (const auto& dep : node->dependencies()) {
                    dependencies_str += dep->id() + ",";
                }
                graph_log_error(
                        "Node %s depends %s", node->id().c_str(),
                        dependencies_str.c_str());
                rec_stack.erase(node);
                for (Node* neighbor : node->dependencies()) {
                    if (rec_stack.find(neighbor) != rec_stack.end()) {
                        node = neighbor;
                        break;
                    }
                }
            }
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
    graph_assert(valid(), "Graph is invalid!");
    graph_assert(!m_is_freezed, "Graph is already freezed!");
    graph_assert(m_nodes.size() > 0, "No nodes in the graph!");
    m_is_freezed = true;

    prepare_exe();

    //! show all nodes in the graph when debug
    if (log_level() == GraphLogLevel::DEBUG) {
        graph_log_debug(
                "++++++++++++++++++++++++++++Nodes in the "
                "graph:+++++++++++++++++++++++++++++");
        for (const auto& pair : m_nodes) {
            if (!pair.second->dependencies().empty()) {
                std::string dependencies_str;
                for (const auto& dep : pair.second->dependencies()) {
                    dependencies_str += dep->id() + ",";
                }
                graph_log_debug(
                        "Node \"%s\" depends: \"%s\"", pair.second->id().c_str(),
                        dependencies_str.c_str());
            } else {
                graph_log_debug(
                        "Node \"%s\" has no dependencies", pair.second->id().c_str());
            }
        }
        graph_log_debug(
                "++++++++++++++++++++++++++++Nodes in the "
                "graph:+++++++++++++++++++++++++++++\n");
    }

    graph_log_info(
            "User Build Graph use time %.3f ms with %zu nodes",
            m_timer.get_msecs_reset(), m_nodes.size());
}

std::string Graph::run_line(Node* node, size_t zoom_to) const {
    graph_assert(
            node->status() == Node::Status::FINISHED,
            "code issue happened!!, Node: %s is not finished!", node->id().c_str());
    graph_assert(zoom_to > 10, "zoom_to should be greater than 10");

    std::string time_pos;
    auto start_time = node->start_time();
    auto duration = node->duration();
    //! Divide used_time into zoom_to ratio
    start_time = start_time * zoom_to / m_timer.get_msecs();
    duration = duration * zoom_to / m_timer.get_msecs();
    bool have_r = false;
    int w_pos = -1;
    for (int i = 0; i < zoom_to; i++) {
        if (i >= start_time && i < start_time + duration) {
            time_pos += "R";
            have_r = true;
        } else if (i < start_time) {
            time_pos += "W";
            w_pos++;
        } else {
            time_pos += "F";
        }
    }
    //! mark last is F, as zoom may cause last is not F
    time_pos[zoom_to - 1] = 'F';
    if (!have_r && w_pos >= 0) {
        //! if no R, then mark the last W as R, caused by running time is too short
        time_pos[w_pos] = 'R';
    }
    return time_pos;
}

std::vector<Node*> Graph::longest_path() {
    graph_assert(m_is_freezed, "Graph is not freezed! please call freezed() first!");
    graph_assert(m_executed_node_count == m_nodes.size(), "Graph is not executed!");

    std::vector<Node*> path;
    std::vector<Node*> longest_path;
    double total_cost = 0;
    //! every node have duration time, so we can get the longest path by it
    //! we can use topological sort to get the longest path by DFS
    std::function<void(Node*, std::vector<Node*>&, std::vector<Node*>&, double&)> dfs =
            [&](Node* node, std::vector<Node*>& visited, std::vector<Node*>& path,
                double& cost) {
                visited.push_back(node);
                path.push_back(node);
                cost += node->duration();
                if (cost > total_cost) {
                    total_cost = cost;
                    longest_path = path;
                }
                for (Node* neighbor : node->dependencies()) {
                    if (std::find(visited.begin(), visited.end(), neighbor) ==
                        visited.end()) {
                        dfs(neighbor, visited, path, cost);
                    }
                }
                visited.pop_back();
                path.pop_back();
                cost -= node->duration();
            };

    for (const auto& pair : m_nodes) {
        std::vector<Node*> visited;
        std::vector<Node*> path;
        double cost = 0;
        dfs(pair.second, visited, path, cost);
    }

    if (0) {
        graph_log_debug(
                "++++++++++++++++++++++++++++Longest path in the "
                "graph:+++++++++++++++++++++++++++++: %.3f ms",
                total_cost);
        for (const auto& node : longest_path) {
            graph_log_debug(
                    "Node \"%s\" duration: %.3f ms", node->id().c_str(),
                    node->duration());
        }
        graph_log_debug(
                "++++++++++++++++++++++++++++Longest path in the "
                "graph:+++++++++++++++++++++++++++++\n");
    }

    return longest_path;
}

bool Graph::dump_dot(const std::string& path) {
    Gtimer timer;
    std::lock_guard<std::mutex> lock(mtx);
    if (!m_is_freezed) {
        graph_log_error(
                "Graph is not freezed! please call freezed() first before dump_dot!");
        return false;
    }
    std::ofstream ofs(path);
    if (ofs.is_open()) {
        //! get long_path info
        std::vector<Node*> lpath;
        if (m_executed_node_count == m_nodes.size()) {
            lpath = longest_path();
        }

        ofs << "digraph " << m_name << " {\n";
        ofs << "  rankdir=LR;\n";
        for (const auto& pair : m_nodes) {
            for (const auto& dep : pair.second->dependencies()) {
                //! check dependency is virtual or not, if virtual will use dashed line
                bool is_virtual = false;
                for (const auto& v_dep : pair.second->virtual_dependencies()) {
                    if (v_dep == dep) {
                        is_virtual = true;
                        break;
                    }
                }

                bool is_longest_path = false;
                //! check if the dependency is in the longest path, if in the longest
                //! path, then use red color
                //! dep->id() index_a in lpath and pair->second->id() index_b in lpath,
                //! and index_b - index_a = 1
                for (int i = 0; i < static_cast<int>(lpath.size()) - 1; i++) {
                    if (lpath[i] == pair.second && lpath[i + 1] == dep) {
                        is_longest_path = true;
                        break;
                    }
                }
                std::string style = "";
                if (is_virtual || is_longest_path) {
                    if (is_virtual && is_longest_path) {
                        style = " [style=dashed, color=red]";
                    } else if (is_virtual) {
                        style = " [style=dashed]";
                    } else if (is_longest_path) {
                        style = " [color=red]";
                    } else {
                        graph_throw("code issue happened!!");
                    }
                }
                ofs << "  \"" << dep->id() << "\" -> \"" << pair.second->id() << "\""
                    << style << ";\n";
            }

            //! write time,status,cpu mask and priority to node
            //! write cpu mask and priority to node
            ofs << "  \"" << pair.second->id() << "\" [label=\"" << pair.second->id()
                << "\\n"
                << "status: " << pair.second->status_str() << "\\n";
            auto c_mask = pair.second->cpu_mask();
            if (c_mask) {
                ofs << "cpumask: " << std::hex << std::showbase << c_mask
                    << std::noshowbase << std::dec << "\\n";
            }
            auto priority = pair.second->priority();
            if (priority != INT_MAX) {
                ofs << "priority: " << priority << "\\n";
            }
            ofs << "exec time: " << pair.second->duration() << "ms";
            ofs << "\"];\n";
        }
        //! write timeing diagram if some node is executed
        if (m_executed_node_count > 0) {
            bool verify = false;
            ofs << "isolated_node [shape=box, style=filled, color=lightblue, "
                   "label=\"Timing diagram\n";
            for (const auto& pair : m_nodes) {
                if (pair.second->status() == Node::Status::FINISHED) {
                    verify = true;
                    ofs << "\n"
                        << run_line(pair.second, 50) << " : " << pair.second->id();
                }
            }
            if (m_executed_node_count == m_nodes.size()) {
                //! write graph cost time
                ofs << "\n\nTotal cost time: " << m_cost_time << "ms";
            }
            ofs << "\"];\n";
            graph_assert(
                    verify,
                    "code issue happened!!, no node is executed but "
                    "m_executed_node_count is not 0");
        }

        ofs << "}\n";
        ofs.close();
        graph_log_info(
                "Graph is dumped to %s(Use time: %fms) You can use `dot -Tpng %s -o "
                "%s.png` to generate png file",
                path.c_str(), timer.get_msecs(), path.c_str(), path.c_str());
        return true;
    } else {
        graph_log_error(
                "Failed to dump graph to \"%s\" by "
                "%s",
                path.c_str(), strerror(errno));
        return false;
    }
}

void Graph::prepare_exe() {
    // Initialize execution_queue with nodes that have no dependencies
    for (const auto& pair : m_nodes) {
        if (m_dependency_count[pair.second] == 0) {
            m_execution_queue.push(pair.second);
            graph_log_info("Pushing \"%s\" to start queue", pair.second->id().c_str());
        }
    }
}

void Graph::inplace_worker(bool is_inplace) {
    graph_log_debug(
            "node exec will be executed in %s",
            is_inplace ? "inplace worker" : "new thread");
    if (is_inplace) {
        graph_log_warn(
                "node exec will be executed in inplace worker, all node exec will be "
                "executed in the same thread as worker, if node do not config cpu_mask "
                "and priority, then it will use the same cpu_mask and priority as "
                "worker "
                "even use pre task config");
    }
    m_is_inplace_worker = is_inplace;
}

double Graph::execute() {
    restore();
    graph_assert(m_is_freezed, "Graph is not freezed! please call freezed() first!");

    std::condition_variable cv;
    std::queue<Node*> execution_queue;
    std::unordered_map<Node*, size_t> dependency_count;
    std::atomic<bool> finished(false);

    //! copy the execution queue caused by loop execute
    execution_queue = m_execution_queue;
    dependency_count = m_dependency_count;
    graph_assert(!execution_queue.empty(), "No nodes to execute!");

    auto time_after_freeze = m_timer.get_msecs_reset();
    graph_log_info(
            "Starting execution(%zu) after freezed %.3f ms", ++m_execute_count,
            time_after_freeze);

    //! worker id
    size_t worker_id = 0;
    std::mutex mtx_id;
    auto worker = [&]() {
        size_t id = 0;
        {
            std::lock_guard<std::mutex> lock(mtx_id);
            id = worker_id++;
        }
        while (true) {
            //! check if finished do not depends on cv, as sometime some worker may into
            //! wait after finished
            {
                std::unique_lock<std::mutex> lock(mtx);
                if (finished.load(std::memory_order_seq_cst)) {
                    graph_log_debug("worker: %zu exit", id);
                    return;
                }
            }

            Node* node = nullptr;
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [&]() {
                    return finished.load(std::memory_order_seq_cst) ||
                           !execution_queue.empty();
                });
                if (finished.load(std::memory_order_seq_cst) &&
                    execution_queue.empty()) {
                    graph_log_debug("worker: %zu exit", id);
                    return;  // Exit the thread if finished and queue is empty
                }
                if (!execution_queue.empty()) {
                    node = execution_queue.front();
                    execution_queue.pop();
                }
            }
            if (node) {
                //! real work execute the task, infact graph worker just call Node exec,
                //! which will create a new thread to execute the real task
                Gtimer t;

                graph_log_info("Executing %s", node->id().c_str());
                node->status(Node::Status::RUNNING);
                node->start_time(m_timer.get_msecs());
                node->exec(m_is_inplace_worker);
                node->status(Node::Status::FINISHED);
                node->duration(t.get_msecs());
                {
                    std::unique_lock<std::mutex> _(m_executed_node_count_mtx);
                    m_executed_node_count++;
                    graph_log_info(
                            "Executed %s (%zu/%zu)", node->id().c_str(),
                            m_executed_node_count, m_nodes.size());
                    if (m_executed_node_count == m_nodes.size()) {
                        std::unique_lock<std::mutex> _(mtx);
                        finished.store(true, std::memory_order_seq_cst);
                        graph_log_info(
                                "All nodes executed notify all threads to exit, %zu "
                                "exit",
                                id);
                        cv.notify_all();
                        return;
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(mtx);
                    for (const auto& pair : m_nodes) {
                        Node* n = pair.second;
                        auto& dependencies = n->dependencies();
                        if (std::find(dependencies.begin(), dependencies.end(), node) !=
                            dependencies.end()) {
                            dependency_count[n]--;
                            if (dependency_count[n] == 0 && !n->is_executed()) {
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
    auto create_thread_number = std::min(m_thread_worker_num, m_nodes.size());
    graph_log_info("Starting %zu worker threads", create_thread_number);
    for (size_t i = 0; i < create_thread_number; ++i) {
        threads.emplace_back(worker);
    }

    cv.notify_all();

    for (std::thread& thread : threads) {
        thread.join();
    }

    verify();

    m_cost_time = m_timer.get_msecs();
    graph_log_info("Execution completed in %.3f ms", m_cost_time);

    if (log_level() == GraphLogLevel::DEBUG) {
        graph_log_debug(
                "++++++++++++++++++++++Execution time "
                "details:+++++++++++++++++++");
        for (const auto& pair : m_nodes) {
            Node* node = pair.second;
            auto time_pos = run_line(node, 50);
            graph_log_debug("%s : \"%s\"", time_pos.c_str(), node->id().c_str());
        }
        graph_log_debug(
                "++++++++++++++++++++++Execution time "
                "details:+++++++++++++++++++");
    }

    return m_cost_time;
}

bool Graph::is_cyclic(
        Node* node, std::unordered_set<Node*>& visited,
        std::unordered_set<Node*>& rec_stack) {
    if (rec_stack.count(node))
        return true;
    if (visited.count(node))
        return false;

    visited.insert(node);
    rec_stack.insert(node);

    for (Node* neighbor : node->dependencies()) {
        if (is_cyclic(neighbor, visited, rec_stack)) {
            return true;
        }
    }

    rec_stack.erase(node);
    return false;
}
bool Graph::is_connected() {
    graph_assert(m_nodes.size() > 0, "No nodes in the graph!");

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

void Graph::verify() {
    graph_log_info(
            "Execution status: %zu/%zu nodes executed", m_executed_node_count,
            m_nodes.size());
    if (m_executed_node_count == m_nodes.size()) {
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

    dump_node_status(false);
}

void Graph::dump_node_status(bool force_dump) {
    if (force_dump || log_level() == GraphLogLevel::DEBUG) {
        auto old_level = log_level();
        config_log_level(GraphLogLevel::DEBUG);
        graph_log_debug("++++++++++++++++dump node status start+++++++++++++++");
        for (const auto& pair : m_nodes) {
            Node* node = pair.second;
            auto may_duration = node->duration();
            auto may_start_time = node->start_time();
            if (node->status() == Node::Status::FINISHED) {
                may_duration = node->duration();
                may_start_time = node->start_time();
            } else if (node->status() == Node::Status::RUNNING) {
                may_duration = m_timer.get_msecs() - node->start_time();
            } else if (node->status() == Node::Status::WAITING) {
                may_duration = 0;
                may_start_time = 0;
            } else {
                graph_throw("Node status is invalid");
            }
            graph_log_debug(
                    "Node \"%s\" status: %s, exec duration: %.3f ms, wait sched: %.3f "
                    "ms",
                    node->id().c_str(), node->status_str().c_str(), may_duration,
                    may_start_time);
        }
        graph_log_debug("++++++++++++++++dump node status end+++++++++++++++\n");
        config_log_level(old_level);
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

auto config_dlf_log_level() -> std::pair<bool, GraphLogLevel> {
    bool is_use_env = false;
    auto dlf_level = GraphLogLevel::INFO;
    if (auto* env = ::std::getenv("MINI_GRAPH_OVERRIDE_LOG_LEVEL")) {
        dlf_level = static_cast<GraphLogLevel>(::std::stoi(env));
        is_use_env = true;
    }

#ifdef __ANDROID__
    char buf[PROP_VALUE_MAX];
    if (__system_property_get("MINI_GRAPH_OVERRIDE_LOG_LEVEL", buf) > 0) {
        dlf_level = static_cast<GraphLogLevel>(atoi(buf));
        is_use_env = true;
    }
#endif

    return {is_use_env, dlf_level};
}

std::pair<bool, GraphLogLevel> g_log_level = config_dlf_log_level();
//! define a default log handler
void default_log_handler(
        GraphLogLevel level, const char* file, const char* func, int line,
        const char* fmt, va_list ap) {
    if (level < g_log_level.second)
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

    //! now call the android/ohos log
#ifdef __ANDROID__
    int android_level = ANDROID_LOG_UNKNOWN;
    switch (level) {
        case GraphLogLevel::DEBUG:
            android_level = ANDROID_LOG_DEBUG;
            break;
        case GraphLogLevel::INFO:
            android_level = ANDROID_LOG_INFO;
            break;
        case GraphLogLevel::WARN:
            android_level = ANDROID_LOG_WARN;
            break;
        case GraphLogLevel::ERROR:
            android_level = ANDROID_LOG_ERROR;
            break;
        default:
            android_level = ANDROID_LOG_UNKNOWN;
            break;
    }
    __android_log_vprint(android_level, "mini_graph", fmt, ap);
#elif defined(__OHOS__)
    //! refer to sysroot/usr/include/hilog/log.h @LogLevel
    //! ohos use same name `LogLevel` as some other library
    //! so we use `_OhOsLogLevel` to avoid conflict
    enum class _OhOsLogLevel : unsigned char {
        LOG_DEBUG = 3,
        LOG_INFO = 4,
        LOG_WARN = 5,
        LOG_ERROR = 6,
        LOG_FATAL = 7,
    };

    _OhOsLogLevel ohos_level = _OhOsLogLevel::LOG_INFO;
    switch (level) {
        case GraphLogLevel::DEBUG:
            ohos_level = _OhOsLogLevel::LOG_DEBUG;
            break;
        case GraphLogLevel::INFO:
            ohos_level = _OhOsLogLevel::LOG_INFO;
            break;
        case GraphLogLevel::WARN:
            ohos_level = _OhOsLogLevel::LOG_WARN;
            break;
        case GraphLogLevel::ERROR:
            ohos_level = _OhOsLogLevel::LOG_ERROR;
            break;
        default:
            ohos_level = _OhOsLogLevel::LOG_INFO;
            break;
    }
    OH_LOG_Print(
            LOG_APP, static_cast<::LogLevel>(ohos_level), LOG_DOMAIN, "mini_graph", fmt,
            ap);
#endif
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
    if (g_log_level.first && g_log_level.second != level) {
        printf("prioritize the use of env log: %d, config level: %d do not "
               "take effect!!",
               static_cast<int>(g_log_level.second), static_cast<int>(level));
    } else {
        g_log_level.second = level;
    }
}

GraphLogLevel Graph::log_level() {
    return g_log_level.second;
}
