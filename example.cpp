
#include "additional_unit.h"

class entity_with_own_logger{
public:
    int _some_field;
    entity_with_own_logger(int some_field_) : _some_field(some_field_), _log( ("EntityWithLogger"+std::to_string(some_field_)).c_str()){
        _log.level_console = seq_logger::logging_level::debug;
        _log.level_seq = seq_logger::logging_level::verbose;
        _log.add_enricher([&](seq_logger::seq_context &ctx_) {
            ctx_.add("EnrichedThreadId", std::this_thread::get_id());
        });
        _log.add_enricher([&](seq_logger::seq_context &ctx_) {
            ctx_.add("EnrichedFieldValue", _some_field);
            if (_some_field>2) {
                ctx_.level--;
            }
        });
        _log.add_property("AddedProperty", time(NULL));
        std::thread t(&entity_with_own_logger::thread_handler, this);
        t.detach();
    }
    ~entity_with_own_logger(){};
private:
    seq_logger::seq _log;

    void thread_handler() {
        auto sleep_duration = std::chrono::milliseconds(5);
        for(auto i=0; i<3; ++i){
            std::this_thread::sleep_for(sleep_duration);
            _log.info("This is a message from a thread {MagicValue}", {{"MagicValue", i}});
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        _log.warning("Thread finished");
    }
};

int some_field;

int main() {
    using namespace seq_logger;
    seq::init("127.0.0.1:5341", logging_level::debug, logging_level::debug, 100, ""/*, "<SOME_SEQ_API_KEY>"*/, 1000,
              true);
    seq::base_level_console = seq_logger::logging_level::debug;
    seq::base_level_seq = seq_logger::logging_level::verbose;
    srand(time(NULL));
    seq::add_shared_property("ProcessRunIdentifier", rand());
    seq::add_shared_enricher([&](seq_logger::seq_context &ctx) {
        ctx.add("SomeField", some_field++);
        if (some_field>9){
            ctx.level++;
        }
    });

    seq::log_debug("Static logging", {{"PassedValue", "RandomValue"}});
    seq::log_debug("Static logging", {{"PassedValue", 3}});
    std::vector<std::shared_ptr<entity_with_own_logger>> vec;

    for(int i=0; i<3; i++){
        seq::log_debug("Creating new entity with logger!");
        vec.emplace_back(std::make_shared<entity_with_own_logger>(i));
    }
    entity_with_own_logger e1(1);

    seq::log_verbose("Should not be visible");
    seq::log_warning("Should be visible");
    auto au = additional_unit();
    au.do_something();
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    return 0;
}
