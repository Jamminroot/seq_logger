#pragma once

#include <atomic>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "HTTPRequest.hpp"

namespace seq_logger {
    struct helpers {
        static inline std::string escape_json(const std::string &s) {
            std::ostringstream o;
            for (char c: s) {
                switch (c) {
                    case '"':
                        o << "\\\"";
                        break;
                    case '\\':
                        o << "\\\\";
                        break;
                    case '\b':
                        o << "\\b";
                        break;
                    case '\f':
                        o << "\\f";
                        break;
                    case '\n':
                        o << "\\n";
                        break;
                    case '\r':
                        o << "\\r";
                        break;
                    case '\t':
                        o << "\\t";
                        break;
                    default:
                        if ('\x00' <= c && c <= '\x1f') {
                            o << "\\u"
                              << std::hex << std::setw(4) << std::setfill('0') << (int) c;
                        } else {
                            o << c;
                        }
                }
            }
            return o.str();
        }
    };

    enum logging_level {
        verbose = 0,
        debug = 1,
        info = 2,
        warning = 3,
        error = 4,
        fatal = 5
    };

    inline logging_level& operator++(logging_level& other_)
    {
        other_ = static_cast<logging_level>(std::min((other_ + 1), 5));
        return other_;
    }

    inline logging_level operator++(logging_level& other_, int c)
    {
        logging_level rVal = other_;
        ++other_;
        return rVal;
    }

    inline logging_level& operator--(logging_level& other_)
    {
        other_ = static_cast<logging_level>(std::max((other_ - 1), 0));
        return other_;
    }

    inline logging_level operator--(logging_level& other_, int c)
    {
        logging_level rVal = other_;
        --other_;
        return rVal;
    }

    const char *const logging_level_strings[6] = {
            "Verbose",
            "Debug",
            "Information",
            "Warning",
            "Error",
            "Fatal"};

    const char *const logging_level_strings_short[6] = {
            "VRB",
            "DBG",
            "INF",
            "WRN",
            "ERR",
            "FTL"};

    struct stringified_value {
        stringified_value() = default;

        explicit stringified_value(const char *_value) : str_val(_value == nullptr ? "" : _value) {};

        explicit stringified_value(const std::string &value_) : str_val(value_) {};

        explicit stringified_value(std::string &&value_) : str_val(value_) {};

        explicit stringified_value(uint8_t &&value_) : str_val(std::to_string(value_)) {};

        explicit stringified_value(uint32_t &&value_) : str_val(std::to_string(value_)) {};

        explicit stringified_value(uint64_t &&value_) : str_val(std::to_string(value_)) {};

        explicit stringified_value(int8_t &&value_) : str_val(std::to_string(value_)) {};

        explicit stringified_value(int32_t &&value_) : str_val(std::to_string(value_)) {};

        explicit stringified_value(int64_t &&value_) : str_val(std::to_string(value_)) {};

        template<class T>
        stringified_value(T value_) {
            std::ostringstream ss;
            ss << value_;
            str_val = ss.str();
        }

        template<class T>
        explicit stringified_value(const T &value_) {
            std::ostringstream ss;
            ss << value_;
            str_val = ss.str();
        }

        std::string str_val;
    };

    typedef std::pair<std::string, stringified_value> seq_properties_pair_t;
    typedef std::vector<seq_properties_pair_t> seq_properties_vector_t;

    class seq_log_entry;

    struct seq_context {
    public:
        const seq_properties_pair_t &operator[](size_t index_) const {
            return _properties[index_];
        }

        ///\brief Add a property to the context
        void append(const seq_properties_vector_t &other_) {
            if (other_.empty()) return;
            _properties.insert(_properties.end(), other_.begin(), other_.end());
        }

        seq_properties_pair_t &operator[](size_t index_) {
            return _properties[index_];
        }

        seq_context() = default;

        [[nodiscard]] bool empty() const { return _properties.empty(); };

        [[nodiscard]] size_t size() const { return _properties.size(); };

        seq_context(logging_level level_, seq_properties_vector_t &&parameters_, const char *logger_name_) : level(
                level_), logger_name(logger_name_), _properties(std::move(parameters_)) {};

        seq_context(logging_level level_, const seq_properties_vector_t &parameters_, const char *logger_name_)
                : level(level_), logger_name(logger_name_), _properties(parameters_) {};

        ///\brief Add a property to the context
        void add(std::string key_, stringified_value value_) {
            _properties.emplace_back(std::move(key_), std::move(value_));
        }

        ///\brief Level of the context
        logging_level level;

        ///\brief Name of the logger
        const std::string logger_name;
    private:
        seq_properties_vector_t _properties;
    };


    class seq_log_entry {
    public:

        seq_log_entry(std::string message_,
                      seq_context &&context_)
                : context(std::move(context_)),
                  _message(std::move(message_)) {
            init_time();
        }

        [[nodiscard]] std::string to_raw_json_entry() const {
            std::stringstream sstream;
            sstream << R"({"@t": ")" << time << R"(", "@mt":")" << helpers::escape_json(_message) << R"(", "@l":")"
                    << logging_level_strings[context.level] << R"(","Logger":")" << context.logger_name << "\"";
            if (context.empty()) {
                sstream << "}";
                return sstream.str();
            }
            auto parameters_specified = context.size();
            for (size_t i = 0; i < parameters_specified; ++i) {
                sstream << ",\"" << helpers::escape_json(context[i].first) << "\":\"" << helpers::escape_json(context[i].second.str_val) << "\"";
            }
            sstream << "}";
            auto str = sstream.str();
            auto cc = str.c_str();
            return cc;
        }

        [[nodiscard]] const std::string &message() const {
            return _message;
        }

        const seq_context context;
        char time[24];
    private:
        void init_time() {
            auto str = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count() % 1000);
            time_t rawtime;
            struct tm *timeinfo;
            std::time(&rawtime);
            timeinfo = localtime(&rawtime);
            strftime(time, 23, "%FT%T.", timeinfo);
            time[19] = '.';
            std::copy(&str[0], &str[3], &time[20]);
            time[23] = '\0';
        }

        std::string _message;
    };

    class seq {
    public:
        ///\brief Base console logging level for all loggers - when other loggers are created, that level is used as a base
        inline static logging_level base_level_console;

        ///\brief Base seq logging level for all loggers - when other loggers are created, that level is used as a base
        inline static logging_level base_level_seq;

        ///\brief Console logging level for this logger
        logging_level level_console = logging_level::debug;

        ///\brief Seq logging level for this logger
        logging_level level_seq = logging_level::verbose;


        ///\brief Default constructor
        seq() {
            finish_initialization({});
        }

        ///\brief Constructor with properties
        ///\param properties_ Properties to be added to the logger
        seq(const char *name_, seq_properties_vector_t &&properties_) : _properties(std::move(properties_)) {
            finish_initialization(name_);
        }

        ///\brief Constructor with properties
        ///\param properties_ Properties to be added to the logger
        seq(const char *name_, seq_properties_pair_t &&property_pair_) : _properties({std::move(property_pair_)}) {
            finish_initialization(name_);
        }

        ///\brief Constructor with name
        ///\param name_ Name of the logger
        explicit seq(const char *name_) {
            finish_initialization(name_);
        }

        ///\brief Constructor with name and logging levels
        ///\param name_ Name of the logger
        /// \param console_verbosity_ Logging level for the console, e.g. logging_level::info would output info, but not debug messages
        /// \param seq_verbosity_ Logging level for SEQ, e.g. logging_level::info would output info, but not debug messages
        explicit seq(const char *name_, logging_level console_verbosity_, logging_level seq_verbosity_) {
            finish_initialization(name_);
            level_console = console_verbosity_;
            level_seq = seq_verbosity_;
        }


        ~seq() {
            _enrichers.clear();

            if (!_static_instance) {
                std::lock_guard<std::mutex> guard(_logs_mutex);
                unregister_logger(this);
                shared_instance().transfer_logs(_seq_dispatch_queue);
                return;
            }

            {
                std::unique_lock<std::mutex> lock{_s_thread_finished_mutex};
                _s_terminating = true;
            }

            std::unique_lock<std::mutex> lock{_s_thread_finished_mutex};
            _s_thread_finished.wait(lock);
            if (_s_thread.joinable()) {
                _s_thread.join();
            }
            http::Request request("http://" + _s_address + "/api/events/raw?clef");
            send_events_handler(request);
        }

        seq(seq const &) = delete;

        seq(seq &&) = delete;

        seq &operator=(seq const &) = delete;

        seq &operator=(seq &&) = delete;

        /// \brief Initialize the Seq logger
        /// \param address_ Address of the Seq server, e.g. 127.0.0.1:5341
        /// \param console_verbosity_ Logging level for the console, e.g. logging_level::info would output info, but not debug messages
        /// \param seq_verbosity_ Logging level for SEQ, e.g. logging_level::info would output info, but not debug messages
        /// \param dispatch_interval_ Interval at which to send logs to SEQ, in milliseconds
        /// \param api_key_ Seq API key
        /// \param seq_init_timeout Timeout for SEQ initialization, in milliseconds. If SEQ is not available after this time, the logger will start without SEQ if allow_without_seq is true
        /// \param allow_without_seq If SEQ is not available, allow the logger to start without SEQ
        static void init(std::string address_, logging_level console_verbosity_, logging_level seq_verbosity_,
                         size_t dispatch_interval_, const std::string &api_key_ = "", int seq_init_timeout = 1000, bool allow_without_seq = true) {
            if (_s_initialized) return;
            _s_address = std::move(address_);
            if (!api_key_.empty()) {
                _s_auth_header = api_key_;
            }
            _s_initialized = true;
            base_level_console = console_verbosity_;
            base_level_seq = seq_verbosity_;
            _s_dispatch_interval = std::chrono::milliseconds(dispatch_interval_);
            shared_instance().start_thread(seq_init_timeout, allow_without_seq);
        }

        /// \brief Add a property to all logs
        /// \param key_
        /// \param val_
        void add_property(std::string key_, stringified_value val_) {
            _properties.emplace_back(std::move(key_), std::move(val_));
        }

        /// \brief Add a property to all logs
        static void add_shared_property(std::string key_, stringified_value val_) {
            _s_shared_properties.emplace_back(std::move(key_), std::move(val_));
        }

        /// \brief Add a property to all logs with a value that is evaluated at runtime
        void add_enricher(std::function<void(seq_context &)> enricher_) {
            _enrichers.push_back(std::move(enricher_));
        }

        /// \brief Add a property to all logs with a value that is evaluated at runtime
        static void add_shared_enricher(std::function<void(seq_context &)> enricher_) {
            _s_enrichers.push_back(std::move(enricher_));
        }

//region instance logging method implementations
        void verbose(std::string message_, seq_properties_vector_t &&properties_) const {
            instance_log_generic<logging_level::verbose>(std::move(message_), std::move(properties_));
        }

        void debug(std::string message_, seq_properties_vector_t &&properties_) const {
            instance_log_generic<logging_level::debug>(std::move(message_), std::move(properties_));
        }

        void info(std::string message_, seq_properties_vector_t &&properties_) const {
            instance_log_generic<logging_level::info>(std::move(message_), std::move(properties_));
        }

        void warning(std::string message_, seq_properties_vector_t &&properties_) const {
            instance_log_generic<logging_level::warning>(std::move(message_), std::move(properties_));
        }

        void error(std::string message_, seq_properties_vector_t &&properties_) const {
            instance_log_generic<logging_level::error>(std::move(message_), std::move(properties_));
        }

        void fatal(std::string message_, seq_properties_vector_t &&properties_) const {
            instance_log_generic<logging_level::fatal>(std::move(message_), std::move(properties_));
        }

        void verbose(std::string message_) const {
            instance_log_generic<logging_level::verbose>(std::move(message_));
        }

        void debug(std::string message_) const {
            instance_log_generic<logging_level::debug>(std::move(message_));
        }

        void info(std::string message_) const {
            instance_log_generic<logging_level::info>(std::move(message_));
        }

        void warning(std::string message_) const {
            instance_log_generic<logging_level::warning>(std::move(message_));
        }

        void error(std::string message_) const {
            instance_log_generic<logging_level::error>(std::move(message_));
        }

        void fatal(std::string message_) const {
            instance_log_generic<logging_level::fatal>(std::move(message_));
        }

//endregion


//region static logging methods implementations
        static void log_verbose(std::string message_) {
            shared_instance().instance_log_generic<logging_level::verbose>(std::move(message_));
        }

        static void log_debug(std::string message_) {
            shared_instance().instance_log_generic<logging_level::debug>(std::move(message_));
        }

        static void log_info(std::string message_) {
            shared_instance().instance_log_generic<logging_level::info>(std::move(message_));
        }

        static void log_warning(std::string message_) {
            shared_instance().instance_log_generic<logging_level::warning>(std::move(message_));
        }

        static void log_error(std::string message_) {
            shared_instance().instance_log_generic<logging_level::error>(std::move(message_));
        }

        static void log_fatal(std::string message_) {
            shared_instance().instance_log_generic<logging_level::fatal>(std::move(message_));
        }

        static void log_verbose(std::string message_, seq_properties_vector_t &&properties_) {
            shared_instance().instance_log_generic<logging_level::verbose>(std::move(message_), std::move(properties_));
        }

        static void log_debug(std::string message_, seq_properties_vector_t &&properties_) {
            shared_instance().instance_log_generic<logging_level::debug>(std::move(message_), std::move(properties_));
        }

        static void log_info(std::string message_, seq_properties_vector_t &&properties_) {
            shared_instance().instance_log_generic<logging_level::info>(std::move(message_), std::move(properties_));
        }

        static void log_warning(std::string message_, seq_properties_vector_t &&properties_) {
            shared_instance().instance_log_generic<logging_level::warning>(std::move(message_), std::move(properties_));
        }

        static void log_error(std::string message_, seq_properties_vector_t &&properties_) {
            shared_instance().instance_log_generic<logging_level::error>(std::move(message_), std::move(properties_));
        }

        static void log_fatal(std::string message_, seq_properties_vector_t &&properties_) {
            shared_instance().instance_log_generic<logging_level::fatal>(std::move(message_), std::move(properties_));
        }
//endregion
    private:
        inline static bool _s_initialized;
        inline static bool _s_terminating;
        inline static std::string _s_address;
        inline static std::string _s_auth_header;
        inline static std::chrono::duration<long long, std::milli> _s_dispatch_interval;
        inline static std::mutex _s_thread_finished_mutex;
        inline static std::mutex _s_thread_started_mutex;
        inline static std::condition_variable _s_thread_finished;
        inline static std::condition_variable _s_thread_started;
        inline static std::mutex _s_loggers_mutex;
        inline static std::vector<seq *> _s_loggers;
        inline static std::atomic_int32_t _s_logger_id{0};
        inline static seq_properties_vector_t _s_shared_properties;
        inline static std::vector<std::function<void(seq_context &)>> _s_enrichers;

        mutable std::vector<seq_log_entry *> _seq_dispatch_queue;
        mutable std::mutex _logs_mutex;

        bool _static_instance{false};
        char _name[32]{"Default\0"};

        seq_properties_vector_t _properties;
        std::vector<std::function<void(seq_context &)>> _enrichers;
        const int32_t id = _s_logger_id++;
        std::thread _s_thread;
        seq(bool) {
            _s_initialized = false;
            _s_terminating = false;
            base_level_console = logging_level::verbose;
            base_level_seq = logging_level::verbose;
            _s_dispatch_interval = std::chrono::seconds(10);
            _static_instance = true;
            register_logger(this);
        }

        static void send_events_handler(http::Request &request_) {
            bool hasData(false);
            std::stringstream sstream;
            {
                std::lock_guard<std::mutex> static_guard(_s_loggers_mutex);
                if (_s_loggers.empty()) return;
                int32_t index = _s_loggers.size() - 1;
                while (index >= 0) {
                    auto &logger = _s_loggers[index];
                    std::lock_guard<std::mutex> guard(logger->_logs_mutex);

                    while (!logger->_seq_dispatch_queue.empty()) {
                        hasData = true;
                        sstream << logger->_seq_dispatch_queue.back()->to_raw_json_entry() << "\n";
                        delete logger->_seq_dispatch_queue.back();
                        logger->_seq_dispatch_queue.pop_back();
                    }

                    --index;
                }
            }
            if (hasData) {
                try {
                    http::Response resp;
                    if (_s_auth_header.empty()) {
                        resp = request_.send("POST", sstream.str(), {{
                                                                             "Content-type", "application/json"
                                                                     }});
                    } else {
                        resp = request_.send("POST", sstream.str(), {
                                {"Content-type", "application/json" },
                                {"X-Seq-ApiKey", _s_auth_header}
                        });
                    }
                    if (resp.status.code > 300) {
                        std::string body(resp.body.begin(), resp.body.end());
                        std::cout << "Error while sending batch " << resp.status.code << ":" << resp.status.reason << "\n" << body << std::endl;
                    }
                } catch (const std::exception &e) {
                    log_error("Error while trying to ingest logs:", {{"What", e.what()}});
                }
            }
        }

        static void send_events_loop_handler(int timeout, bool allow_without_seq) {
            bool seq_ready(false);
            try {
                http::Request health_check_request("http://" + _s_address + "/health");

                auto response = health_check_request.send("GET", "", {}, std::chrono::milliseconds(timeout));
                if (response.status.code == 200 ||
                    std::string{response.body.begin(), response.body.end()}.find("The Seq node is in service.") !=
                    std::string::npos) {
                    seq_ready = true;
                } else {
                    log_warning("Seq ingestion not ready");
                }
            } catch (const std::exception &e) {
                log_error("Error while checking Seq status", {{"What", e.what()}});
            }

            if (!seq_ready) {
                if (allow_without_seq){
                    log_info("Seq failed to initialize, but working without it is allowed. Only using console output.");
                } else {
                    std::unique_lock<std::mutex> lock{_s_thread_finished_mutex};
                    _s_terminating = true;
                    _s_thread_finished.notify_all();
                    return;
                }
            }

            http::Request request("http://" + _s_address + "/api/events/raw?clef");

            {
                std::unique_lock<std::mutex> lock_start{_s_thread_started_mutex};
                _s_thread_started.notify_all();
            }

            while (!_s_terminating) {
                std::this_thread::sleep_for(_s_dispatch_interval);
                send_events_handler(request);
            }

            std::unique_lock<std::mutex> lock{_s_thread_finished_mutex};
            _s_thread_finished.notify_all();
        }

        template<logging_level L>
        void instance_log_generic(std::string message_, seq_properties_vector_t &&properties_) const {
            if (L < level_console && L < level_seq) return;
            enqueue(std::move(message_), make_context(L, properties_));
        }

        template<logging_level L>
        void instance_log_generic(std::string message_) const {
            if (L < level_console && L < level_seq) return;
            enqueue(std::move(message_), make_context(L));
        }

        void start_thread(int timeout, bool allow_without_seq) {
            if (!_static_instance) return;
            _s_thread = std::thread(&seq::send_events_loop_handler, timeout, allow_without_seq);
            _s_thread.detach();
            {
                std::unique_lock<std::mutex> lock_start{_s_thread_started_mutex};
                _s_thread_started.wait(lock_start);
            }
        }

        [[nodiscard]] static seq &shared_instance() {
            static seq instance(true);
            return instance;
        }

        seq_context make_context(logging_level level_, seq_properties_vector_t properties_) const {
            auto ctx = seq_context(level_, std::move(properties_), _name);
            ctx.append(_properties);
            ctx.append(_s_shared_properties);
            if (!_enrichers.empty()) {
                for (auto &enricher: _enrichers) {
                    enricher(ctx);
                }
            }
            if (!_s_enrichers.empty()) {
                for (auto &enricher: _s_enrichers) {
                    enricher(ctx);
                }
            }
            return ctx;
        }

        seq_context make_context(logging_level level_) const {
            auto ctx = seq_context(level_, _properties, _name);
            ctx.append(_s_shared_properties);
            if (!_enrichers.empty()) {
                for (auto &enricher: _enrichers) {
                    enricher(ctx);
                }
            }
            if (!_s_enrichers.empty()) {
                for (auto &enricher: _s_enrichers) {
                    enricher(ctx);
                }
            }
            return ctx;
        }

        void enqueue(std::string message_, seq_context &&context_) const {
            auto *entry = new seq_log_entry(std::move(message_), std::move(context_));
            static const char esc_char = 27;

            if (entry->context.level >= level_seq) {
                std::lock_guard<std::mutex> guard(_logs_mutex);
                _seq_dispatch_queue.push_back(entry);
            }

            if (entry->context.level >= level_console) {
                std::stringstream ss;
                ss << entry->time << "\t" << entry->context.logger_name << "\t["
                   << logging_level_strings_short[entry->context.level] << "]\t" << esc_char << "[1m"
                   << entry->message() << esc_char
                   << "[0m\t\t";
                if (!entry->context.empty()) {
                    for (size_t i = 0; i < entry->context.size(); ++i) {
                        ss << entry->context[i].first << "=" << entry->context[i].second.str_val << " ";
                    }
                }
                ss << std::endl;
                if (entry->context.level > logging_level::warning) {
                    std::cerr << ss.str();
                    std::cerr.flush();
                } else {
                    std::cout << ss.str();
                    std::cout.flush();
                }
            }
        }

        void transfer_logs(std::vector<seq_log_entry *> &queue_) {
            std::lock_guard<std::mutex> guard(_logs_mutex);
            _seq_dispatch_queue.reserve(_seq_dispatch_queue.size() + queue_.size());
            _seq_dispatch_queue.insert(_seq_dispatch_queue.end(), queue_.begin(), queue_.end());
        }

        static void register_logger(seq *logger_) {
            std::lock_guard<std::mutex> guard(_s_loggers_mutex);
            _s_loggers.push_back(logger_);
        }

        static void unregister_logger(seq *logger_) {
            std::lock_guard<std::mutex> guard(_s_loggers_mutex);
            auto pos = std::find_if(_s_loggers.begin(), _s_loggers.end(), [&](auto &logger) {
                return logger == logger_;
            });
            if (pos != _s_loggers.end()) {
                _s_loggers.erase(pos);
            }
        }

        void finish_initialization(const char *name_) {
            level_console = base_level_console;
            level_seq = base_level_seq;
            std::strcpy(_name, name_);
            register_logger(this);
        }
    };
}