# Header-only SEQ Logging library

Started as a helper in one of my projects, and since I was missing a few things from existing libraries, I made own (although I did use some existing ideas).
Feel free to file PRs with your improvements, those would be greatly appreciated!

> **NOTE**: Standart C++17; For C++14 rewrite inline static variables - definitions somewhere

> **NOTE**: For VS projects you might want to adjust boldification in seq.hpp (`esc_char` and following codes `[1m` and `[0m`)

![Console Output Example](images/console_output.png)

![Seq Output Example](images/seq_output.png)

## Usage

1. Make sure to `::init` seq first - it needs to know where to send logs to:
```c++
using namespace seq_logger;
seq::init("192.168.0.156:5341", logging_level::verbose, logging_level::verbose, 10000);
```
First parameter is console output logging level, second is seq output logging level. Those values are inherited by instanced loggers.
2. Use static methods if you don't really need much for logging:
```c++
seq::log_debug("Static logging", {{"PassedValue", "RandomValue"}});
```
3. Create logger instance if you need cool stuff:
```c++
 seq_logger::seq log("IAmNamedLogger", {{"AndIAmAKeyValuePair", "Which will be added to all entries from this logger"}});
 ```
4. For instance loggers, you can:
* Adjust logging level with `log.verbosity = seq_logger::logging_level::debug` or seq verbosity with `log.verbosity_seq = seq_logger::logging_level::debug`
* Add enrichers (AKA dynamically-added fields):
```c++
log.add_enricher([&](seq_logger::seq_context &ctx_) {
            ctx_.add("Enriched field", some_field_captured_the_moment_output_is_printed);
        });
```

Note that those require a name by design.

## Installation

Add headers from `./src/` to your project.

## Example

Have a look at [example.cpp](./example.cpp)

## Thanks
This library uses [elnormous/HTTPRequest](https://github.com/elnormous/HTTPRequest) for HTTP requests.


