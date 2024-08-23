#include <cstdarg>
#include <functional>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using Task = std::function<void()>;
enum class GraphLogLevel { DEBUG, INFO, WARN, ERROR, NO_LOG };
typedef void (*GraphLogHandler)(
        GraphLogLevel level, const char* file, const char* func, int line,
        const char* fmt, va_list ap);

class Node {
public:
    /*
     * Node constructor
     * @param id: Node id
     * @param task: Task to be executed
     */
    Node(const std::string& id, Task task)
            : m_id(id), m_task(task), m_is_executed(false) {}

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
    bool is_executed() const { return m_is_executed; }

    /*
     * unmark/Mark the node as executed
     */
    void mark_executed() { m_is_executed = true; }
    void unmark_executed() { m_is_executed = false; }

private:
    std::string m_id;
    Task m_task;
    std::vector<Node*> m_dependencies;
    bool m_is_executed;
};

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
     * @param fromId: Task id of the task that depends on the other task
     * @param toIds: Task ids of the tasks that the task depends on
     */
    void dependency(
            const std::string& fromId, const std::initializer_list<std::string>& toIds);
    void dependency(const std::string& fromId, const std::string& toIds);

    /*
     * Check if the graph is a valid, eg is dag etc.
     * @return True if the graph is valid, false otherwise
     */
    bool valid();

    /*
     * Execute the tasks in the graph
     */
    void execute();

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

    ~Graph();

    /* helper */
    static void __assert_fail__(
            const char* file, int line, const char* func, const char* expr,
            const char* msg_fmt, ...);
    static void __log__(
            GraphLogLevel level, const char* file, const char* func, int line,
            const char* fmt, ...);
    static GraphLogHandler config_logger(GraphLogHandler handler);
    static void config_log_level(GraphLogLevel level);

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
     * flag to represent if the graph is freezed
     */
    bool m_is_freezed = false;

    /*
     * Check if the graph is cyclic
     * @param node: Node to be checked
     * @param visited: Set of visited nodes
     * @param recStack: Set of nodes in the recursion stack
     * @return True if the graph is cyclic, false otherwise
     */
    bool is_cyclic(
            Node* node, std::unordered_set<Node*>& visited,
            std::unordered_set<Node*>& recStack);

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
     * Execute the tasks in the graph
     */
    void execution_status();
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
