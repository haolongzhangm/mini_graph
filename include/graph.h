#include <climits>
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
     * Add virtual dependency to the node
     * @param node: Node to be added as virtual dependency
     */
    void virtual_dependency(Node* node) { m_virtual_dependencies.push_back(node); }

    /*
     * Get node dependencies
     * @return Node dependencies
     */
    const std::vector<Node*>& dependencies() const { return m_dependencies; }

    /*
     * Get node virtual dependencies
     * @return Node virtual dependencies
     */
    const std::vector<Node*>& virtual_dependencies() const {
        return m_virtual_dependencies;
    }

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

    /*
     * set cpu mask
     * @param mask: cpu mask
     */
    void cpu_mask(size_t mask) { m_cpu_mask = mask; }

    /*
     * get cpu mask
     * @return cpu mask
     */
    size_t cpu_mask() const { return m_cpu_mask; }

    /*
     * set priority
     * @param priority: priority
     */
    void priority(int priority) { m_priority = priority; }

    /*
     * get priority
     * @return priority
     */
    int priority() const { return m_priority; }

    /*
     * execute the node
     * as graph use the thread pool worker and node will config cpu mask and priority
     * so we need use a new thread to execute the task, which will not affect the graph
     * thread pool worker
     * @param inplace_worker: if use the graph thread pool worker
     */
    void exec(bool inplace_worker = false);

private:
    std::string m_id;
    Task m_task;
    std::vector<Node*> m_dependencies;
    std::vector<Node*> m_virtual_dependencies;
    double m_duration = 0.0;
    double m_start_time = 0.0;
    Status m_status = Status::WAITING;
    unsigned int m_cpu_mask = 0;
    /* at Linux priority range is -20 to 19 so we use INT_MAX to means do not config */
    int m_priority = INT_MAX;
    /*
     * config mask and priority
     */
    void config();

    /*
     * Get node task
     * @return Node task
     */
    Task task() const { return m_task; }
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
     * @param name: graph name, if not set, will be gen by the time of hash
     */
    Graph(size_t thread_worker_num = 8, std::string g_name = "");

    //! disable any other constructor
    Graph(const Graph&) = delete;
    Graph(Graph&&) = delete;
    Graph& operator=(const Graph&) = delete;
    Graph& operator=(Graph&&) = delete;

    /*
     * add a task to the graph
     * @param id: Task id
     * @param task: Task to be executed
     * @param cpu_mask: cpu mask for the task
     * @param priority: priority for the task
     */
    void add_task(
            const std::string& id, Task task, unsigned int cpu_mask = 0,
            int priority = INT_MAX);

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
        {
            std::lock_guard<std::mutex> lock(mtx);
            Node* from = m_nodes.at(who);
            for (const auto& depend_who : depend_whos) {
                Node* to = m_nodes.at(depend_who);
                from->virtual_dependency(to);
            }
        }
    };
    void virtual_dependency(const std::string& who, const std::string& depend_who) {
        dependency(who, depend_who);
        //! no need do check as check will do int the function: dependency
        {
            std::lock_guard<std::mutex> lock(mtx);
            Node* from = m_nodes.at(who);
            Node* to = m_nodes.at(depend_who);
            from->virtual_dependency(to);
        }
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

    /*
     * dump the graph to dot
     * @param path: path to save the dot file
     * @return True if the dump is successful, false otherwise
     */
    bool dump_dot(const std::string& path);

    /*
     * config the task exec inplace graph worker
     * @param is_inplace: if exec inplace
     */
    void inplace_worker(bool is_inplace);

    /*
     * get the name of the graph
     */
    const std::string& name() const { return m_name; }

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
        m_cost_time = -1.0;
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

    /*
     * name of the graph
     */
    std::string m_name;

    /*
     * flag for task exec inplace graph worker
     */
    bool m_is_inplace_worker = false;

    /*
     * Get node run line, show time postion, eg, WWWWWRRRRRRFFFFFFF
     * only used at Node status is FINISHED
     * @return Node run line
     * @param zoom_to: zoom to the node
     */
    std::string run_line(Node* node, size_t zoom_to) const;

    /*
     * every node have duration time, so we can get the longest path by it
     * @return long path node vector
     */
    std::vector<Node*> longest_path();

    /* graph execute cost time */
    double m_cost_time = 0.0;
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
