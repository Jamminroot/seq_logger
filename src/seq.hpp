#pragma once

#include <atomic>
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

    enum logging_level {
        verbose,
        debug,
        info,
        warning,
        error,
        fatal
    };

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

        explicit stringified_value(const char *_value) : str_val(_value) {};

        explicit stringified_value(const std::string &_value) : str_val(_value) {};

        explicit stringified_value(std::string &&_value) : str_val(_value) {};

        explicit stringified_value(uint8_t &&_value) : str_val(std::to_string(_value)) {};

        explicit stringified_value(uint32_t &&_value) : str_val(std::to_string(_value)) {};

        explicit stringified_value(uint64_t &&_value) : str_val(std::to_string(_value)) {};

        explicit stringified_value(int8_t &&_value) : str_val(std::to_string(_value)) {};

        explicit stringified_value(int32_t &&_value) : str_val(std::to_string(_value)) {};

        explicit stringified_value(int64_t &&_value) : str_val(std::to_string(_value)) {};

        template<class T>
        stringified_value(T _value) {
            std::ostringstream ss;
            ss << _value;
            str_val = ss.str();
        }

        template<class T>
        explicit stringified_value(const T &_value) {
            std::ostringstream ss;
            ss << _value;
            str_val = ss.str();
        }

        std::string str_val;
    };

    typedef std::pair<std::string, stringified_value> seq_extras_pair_t;
    typedef std::vector<seq_extras_pair_t> seq_extras_vector_t;

    class seq_log_entry;

    struct seq_context {
    public:
        const seq_extras_pair_t &operator[](size_t index) const {
            return _extras[index];
        }

        void append(const seq_extras_vector_t &other) {
            _extras.insert(_extras.end(), other.begin(), other.end());
        }

        seq_extras_pair_t &operator[](size_t index) {
            return _extras[index];
        }

        seq_context() = default;

        [[nodiscard]] bool empty() const { return _extras.empty(); };

        [[nodiscard]] size_t size() const { return _extras.size(); };

        seq_context(logging_level level_, seq_extras_vector_t &&parameters_, const char *logger_name_) : level(
                level_), logger_name(logger_name_), _extras(std::move(parameters_)) {};

        seq_context(logging_level level_, const seq_extras_vector_t &parameters_, const char *logger_name_)
                : level(level_), logger_name(logger_name_), _extras(parameters_) {};

        void add(std::string key_, stringified_value value_) {
            _extras.emplace_back(std::move(key_), std::move(value_));
        }

        logging_level level;
        const std::string logger_name;
    private:
        seq_extras_vector_t _extras;
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
            sstream << R"({"@t": ")" << time << R"(", "@mt":")" << _message << R"(", "@l":")"
                    << logging_level_strings[context.level] << R"(","Logger":")" << context.logger_name << "\"";
            if (context.empty()) {
                sstream << "}";
                return sstream.str();
            }
            auto parameters_specified = context.size();
            for (size_t i = 0; i < parameters_specified; ++i) {
                sstream << ",\"" << context[i].first << "\":\"" << context[i].second.str_val << "\"";
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
        logging_level verbosity = logging_level::debug;
        logging_level verbosity_seq = logging_level::verbose;

        seq() {
            finish_initialization({});
        }

        seq(const char *_name, seq_extras_vector_t &&_extras) : _shared_extras(std::move(_extras)) {
            finish_initialization(_name);
        }

        seq(const char *_name, seq_extras_pair_t &&_extra) : _shared_extras({std::move(_extra)}) {
            finish_initialization(_name);
        }

        explicit seq(const char *_name) {
            finish_initialization(_name);
        }

        ~seq() {
            _enrichers.clear();
            if (!_static_instance) {
                std::lock_guard<std::mutex> guard(_logs_mutex);
                unregister_logger(this);
                shared_instance().transfer_logs(_seq_dispatch_queue);
                return;
            }
            _s_terminating = true;
            std::unique_lock<std::mutex> lock{_s_thread_finished_mutex};
            _s_thread_finished.wait(lock);
            send_events_handler();
        }

        seq(seq const &) = delete;

        seq(seq &&) = delete;

        seq &operator=(seq const &) = delete;

        seq &operator=(seq &&) = delete;

        static void send_events_handler() {
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
                    auto request = http::Request{_s_endpoint};
                    request.send("POST", sstream.str(), {
                            "Content-type: application/json"
                    });
                } catch (const std::exception &e) {
                    std::cerr << "Error while trying to ingest logs:" << e.what() << std::endl;
                    std::cerr << "Endpoint: " << _s_endpoint << std::endl;
                }
            }
        }


        static void init(const std::string &_address, logging_level _verbosity, logging_level _seq_verbosity,
                         size_t _dispatch_interval) {
            if (_s_initialized) return;
            _s_verbosity = _verbosity;
            _s_verbosity_seq = _seq_verbosity;
            _s_endpoint = "http://" + _address + "/api/events/raw?clef";
            _s_initialized = true;
            _s_dispatch_interval = std::chrono::milliseconds(_dispatch_interval);
            _s_initialized_signal.notify_all();
        }


        void add_extra(std::string _key, stringified_value _val) {
            _shared_extras.emplace_back(std::move(_key), std::move(_val));
        }

        void add_enricher(std::function<void(seq_context &)> enricher_) {
            _enrichers.push_back(std::move(enricher_));
        }

//region instance logging method implementations
        void verbose(std::string message_, seq_extras_vector_t &&extras_) const {
            instance_log_generic<logging_level::verbose>(std::move(message_), std::move(extras_));
        }

        void debug(std::string message_, seq_extras_vector_t &&extras_) const {
            instance_log_generic<logging_level::debug>(std::move(message_), std::move(extras_));
        }

        void info(std::string message_, seq_extras_vector_t &&extras_) const {
            instance_log_generic<logging_level::info>(std::move(message_), std::move(extras_));
        }

        void warning(std::string message_, seq_extras_vector_t &&extras_) const {
            instance_log_generic<logging_level::warning>(std::move(message_), std::move(extras_));
        }

        void error(std::string message_, seq_extras_vector_t &&extras_) const {
            instance_log_generic<logging_level::error>(std::move(message_), std::move(extras_));
        }

        void fatal(std::string message_, seq_extras_vector_t &&extras_) const {
            instance_log_generic<logging_level::fatal>(std::move(message_), std::move(extras_));
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

        static void log_verbose(std::string message_, seq_extras_vector_t &&extras_) {
            shared_instance().instance_log_generic<logging_level::verbose>(std::move(message_), std::move(extras_));
        }

        static void log_debug(std::string message_, seq_extras_vector_t &&extras_) {
            shared_instance().instance_log_generic<logging_level::debug>(std::move(message_), std::move(extras_));
        }

        static void log_info(std::string message_, seq_extras_vector_t &&extras_) {
            shared_instance().instance_log_generic<logging_level::info>(std::move(message_), std::move(extras_));
        }

        static void log_warning(std::string message_, seq_extras_vector_t &&extras_) {
            shared_instance().instance_log_generic<logging_level::warning>(std::move(message_), std::move(extras_));
        }

        static void log_error(std::string message_, seq_extras_vector_t &&extras_) {
            shared_instance().instance_log_generic<logging_level::error>(std::move(message_), std::move(extras_));
        }

        static void log_fatal(std::string message_, seq_extras_vector_t &&extras_) {
            shared_instance().instance_log_generic<logging_level::fatal>(std::move(message_), std::move(extras_));
        }
//endregion
    private:
        inline static bool _s_initialized;
        inline static bool _s_terminating;
        inline static std::string _s_endpoint;
        inline static std::chrono::duration<long long, std::milli> _s_dispatch_interval;
        inline static std::mutex _s_thread_finished_mutex;
        inline static std::condition_variable _s_thread_finished;
        inline static logging_level _s_verbosity;
        inline static std::mutex _s_loggers_mutex;
        inline static std::vector<seq *> _s_loggers;
        inline static std::atomic_int32_t _s_logger_id{0};
        inline static logging_level _s_verbosity_seq;
        inline static std::mutex _s_initialized_signal_mutex;
        inline static std::condition_variable _s_initialized_signal;
        mutable std::vector<seq_log_entry *> _seq_dispatch_queue;
        mutable std::mutex _logs_mutex;

        bool _static_instance{false};
        char _name[32]{"Default\0"};

        seq_extras_vector_t _shared_extras;
        std::vector<std::function<void(seq_context &)>> _enrichers;
        const int32_t id = _s_logger_id++;

        seq(bool) {
            _s_initialized = false;
            _s_terminating = false;
            _s_verbosity = logging_level::verbose;
            _s_verbosity_seq = logging_level::verbose;
            _s_dispatch_interval = std::chrono::seconds(10);
            _static_instance = true;
            register_logger(this);
            start_thread();
        }

        static void send_events_loop_handler() {
            std::unique_lock<std::mutex> lock{_s_initialized_signal_mutex};
            _s_initialized_signal.wait(lock);
            while (!_s_terminating) {
                std::this_thread::sleep_for(_s_dispatch_interval);
                send_events_handler();
            }
            _s_thread_finished.notify_all();
        }

        template<logging_level L>
        void instance_log_generic(std::string message_, seq_extras_vector_t &&extras_) const {
            if (L < verbosity && L < verbosity_seq) return;
            enqueue(std::move(message_), make_context(L, extras_));
        }

        template<logging_level L>
        void instance_log_generic(std::string message_) const {
            if (L < verbosity && L < verbosity_seq) return;
            enqueue(std::move(message_), make_context(L));
        }

        void start_thread() {
            if (!_static_instance) return;
            std::thread t(&seq::send_events_loop_handler);
            t.detach();
        }

        [[nodiscard]] static seq &shared_instance() {
            static seq instance(true);
            return instance;
        }

        seq_context make_context(logging_level level_, seq_extras_vector_t extras_) const {
            auto ctx = seq_context(level_, std::move(extras_), _name);
            ctx.append(_shared_extras);
            if (_enrichers.empty()) return ctx;
            for (auto &enricher: _enrichers) {
                enricher(ctx);
            }
            return ctx;
        }

        seq_context make_context(logging_level level_) const {
            auto ctx = seq_context(level_, _shared_extras, _name);
            if (_enrichers.empty()) return ctx;
            for (auto &enricher: _enrichers) {
                enricher(ctx);
            }
            return ctx;
        }

        void enqueue(std::string message_, seq_context &&context_) const {
            auto *entry = new seq_log_entry(std::move(message_), std::move(context_));
            static const char esc_char = 27;

            if (entry->context.level >= verbosity_seq) {
                std::lock_guard<std::mutex> guard(_logs_mutex);
                _seq_dispatch_queue.push_back(entry);
            }

            if (entry->context.level < verbosity) return;
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

        void transfer_logs(std::vector<seq_log_entry *> &queue) {
            std::lock_guard<std::mutex> guard(_logs_mutex);
            _seq_dispatch_queue.reserve(_seq_dispatch_queue.size() + queue.size());
            _seq_dispatch_queue.insert(_seq_dispatch_queue.end(), queue.begin(), queue.end());
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
            verbosity = _s_verbosity;
            verbosity_seq = _s_verbosity_seq;
            std::strcpy(_name, name_);
            register_logger(this);
        }
    };
}