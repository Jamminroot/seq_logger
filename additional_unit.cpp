
#include "additional_unit.h"

void additional_unit::do_something() {
    _log.info("I did something");
}

additional_unit::additional_unit() :  _log( "AdditionalUnit"){
    _log.info("Additional unit created");
}

additional_unit::~additional_unit() {

}
