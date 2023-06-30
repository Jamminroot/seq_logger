
#ifndef SEQ_EXAMPLE_USAGE_ADDITIONAL_UNIT_H
#define SEQ_EXAMPLE_USAGE_ADDITIONAL_UNIT_H

#include "src/seq.hpp"


class additional_unit {
    seq_logger::seq _log;
public:
    additional_unit();
    void do_something();
    ~additional_unit();
};


#endif //SEQ_EXAMPLE_USAGE_ADDITIONAL_UNIT_H
