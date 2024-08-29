#include <cstdarg>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mini_graph {
using Task = std::function<void()>;
enum class GraphLogLevel { DEBUG, INFO, WARN, ERROR, NO_LOG };
typedef void (*GraphLogHandler)(
        GraphLogLevel level, const char* file, const char* func, int line,
        const char* fmt, va_list ap);

namespace {
class Node {
public:
    /*
     * Node constructor
     * @param id: Node id
     * @param task: Task to be executed
     */
    Node(const std::string& id, Task task) : m_id(id), m_task(task) {}

    /*
     * Get node id
     * @return Node id
     */
    const std::string& id() const { return m_id; }

    /*
     * Add dependency to the node
     * @param node: Node to be added as dependency
     */
    void dependency(Node* node) { m_dependencies.push_back(node); }

    /*
     * Get node dependencies
     * @return Node dependencies
     */
    const std::vector<Node*>& dependencies() const { return m_dependencies; }

    /*
     * Get node task
     * @return Node task
     */
    Task task() const { return m_task; }

    /*
     * Check if the node is executed
     * @return True if the node is executed, false otherwise
     */
    bool is_executed() const { return m_status == Status::FINISHED; }

    enum class Status { WAITING, RUNNING, FINISHED };
    /*
     * set status
     * @param s: status
     */
    void status(Status s) { m_status = s; }
    Status status() const { return m_status; }
    std::string status_str() const {
        switch (m_status) {
            case Status::WAITING:
                return "WAITING";
            case Status::RUNNING:
                return "RUNNING";
            case Status::FINISHED:
                return "FINISHED";
            default:
                __builtin_trap();
                return "UNKNOWN";
        }
    }

    /*
     * set duration
     * @param d: duration
     */
    void duration(double d) { m_duration = d; }
    double duration() const { return m_duration; }

    /*
     * set start time
     * @param t: start time
     */
    void start_time(double t) { m_start_time = t; }
    double start_time() const { return m_start_time; }

    /*
     * restore the node status
     */
    void restore() {
        m_status = Status::WAITING;
        m_duration = 0.0;
        m_start_time = 0.0;
    }

private:
    std::string m_id;
    Task m_task;
    std::vector<Node*> m_dependencies;
    double m_duration = 0.0;
    double m_start_time = 0.0;
    Status m_status = Status::WAITING;
};

class Gtimer {
    using clock = ::std::chrono::high_resolution_clock;
    clock::time_point m_start;

public:
    Gtimer();

    void reset();

    [[nodiscard]] double get_secs() const;
    [[nodiscard]] double get_msecs() const;

    double get_secs_reset();
    double get_msecs_reset();
};

}  // namespace

class Graph {
public:
    /*
     * Graph constructor
     * @param thread_worker_num: thread worker number for the graph
     */
    Graph(size_t thread_worker_num = 8);

    //! disable any other constructor
    Graph(const Graph&) = delete;
    Graph(Graph&&) = delete;
    Graph& operator=(const Graph&) = delete;
    Graph& operator=(Graph&&) = delete;

    /*
     * add a task to the graph
     * @param id: Task id
     * @param task: Task to be executed
     */
    void add_task(const std::string& id, Task task);

    /*
     * Add a dependency between two tasks
     * @param who: Task id of the task that depends on the other task
     * @param depend_whos: Task ids of the tasks that the task depends on
     */
    void dependency(
            const std::string& who,
            const std::initializer_list<std::string>& depend_whos);
    void dependency(const std::string& who, const std::string& depend_who);
    void virtual_dependency(
            const std::string& who,
            const std::initializer_list<std::string>& depend_whos) {
        dependency(who, depend_whos);
    };
    void virtual_dependency(const std::string& who, const std::string& depend_who) {
        dependency(who, depend_who);
    };

    /*
     * Check if the graph is a valid, eg is dag etc.
     * @return True if the graph is valid, false otherwise
     */
    bool valid();

    /*
     * Execute the tasks in the graph
     * @return Execution time in ms
     */
    double execute();

    /*
     * get thread worker number for the graph
     */
    size_t thread_worker_num() const { return m_thread_worker_num; }

    /*
     * Freeze the graph, now only support static graph, so we can't add more tasks
     * after freeze, also, we can't add more dependencies after freeze, also, needed
     * call this function before execute
     */
    void freezed();

    /*
     * dump the node status of graph
     * @param force: do not care about the log level, just dump
     */
    void dump_node_status(bool force = true);

    ~Graph() {
        for (auto& pair : m_nodes) {
            delete pair.second;
        }
    }

    /* helper */
    static void __assert_fail__(
            const char* file, int line, const char* func, const char* expr,
            const char* msg_fmt, ...);
    static void __log__(
            GraphLogLevel level, const char* file, const char* func, int line,
            const char* fmt, ...);
    static GraphLogHandler config_logger(GraphLogHandler handler);
    static void config_log_level(GraphLogLevel level);
    static GraphLogLevel log_level();

private:
    /* graph all nodes */
    std::unordered_map<std::string, Node*> m_nodes;

    /* depends on the number of nodes */
    std::unordered_map<Node*, size_t> m_dependency_count;

    /*
     * Mutex to protect the execution of the tasks
     */
    std::mutex mtx;

    /*
     * Queue to store the tasks wait to be executed
     */
    std::queue<Node*> m_execution_queue;

    /*
     * Prepare the execution of the tasks
     */
    void prepare_exe();

    /*
     * restore the node status
     */
    inline void restore() {
        for (auto& [_, node] : m_nodes) {
            node->restore();
        }
        m_executed_node_count = 0;
    }

    /*
     * flag to represent if the graph is freezed
     */
    bool m_is_freezed = false;

    /*
     * Gtimer for the graph
     */
    Gtimer m_timer;

    /*
     * Check if the graph is cyclic
     * @param node: Node to be checked
     * @param visited: Set of visited nodes
     * @param rec_stack: Set of nodes in the recursion stack
     * @return True if the graph is cyclic, false otherwise
     */
    bool is_cyclic(
            Node* node, std::unordered_set<Node*>& visited,
            std::unordered_set<Node*>& rec_stack);

    /*
     * Check if the graph is connected
     * @return True if the graph is connected, false otherwise
     */
    bool is_connected();

    /*
     * thread worker number for the graph
     * this just for the execution of the tasks, not the task itself thread worker
     */
    size_t m_thread_worker_num;

    /*
     * verify the execute
     */
    void verify();

    /*
     * count for execute
     */
    size_t m_execute_count = 0;

    /*
     * already executed node number
     */
    size_t m_executed_node_count = 0;
    std::mutex m_executed_node_count_mtx;
};

/************* helper ************/
#define graph_log_debug(fmt...) \
    Graph::__log__(GraphLogLevel::DEBUG, __FILE__, __func__, __LINE__, fmt)
#define graph_log_info(fmt...) \
    Graph::__log__(GraphLogLevel::INFO, __FILE__, __func__, __LINE__, fmt)
#define graph_log_warn(fmt...) \
    Graph::__log__(GraphLogLevel::WARN, __FILE__, __func__, __LINE__, fmt)
#define graph_log_error(fmt...) \
    Graph::__log__(GraphLogLevel::ERROR, __FILE__, __func__, __LINE__, fmt)

#define graph_likely(v)   __builtin_expect(static_cast<bool>(v), 1)
#define graph_unlikely(v) __builtin_expect(static_cast<bool>(v), 0)

#define graph_assert(expr, ...)                                                     \
    do {                                                                            \
        if (graph_unlikely(!(expr))) {                                              \
            Graph::__assert_fail__(                                                 \
                    __FILE__, __LINE__, __PRETTY_FUNCTION__, #expr, ##__VA_ARGS__); \
        }                                                                           \
    } while (0)

#define graph_trap() __buitin_trap()

#ifdef __EXCEPTIONS
#define graph_throw(msg)  \
    graph_log_error(msg); \
    throw std::runtime_error(msg);
#else
#define graph_throw(msg)  \
    graph_log_error(msg); \
    graph_trap();
#endif
}  // namespace mini_graph
