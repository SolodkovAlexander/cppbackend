#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/date_time.hpp> //для вывода времени

#include "json_logger.h"

namespace json_logger {

using namespace std::literals;
namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace json = boost::json;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

void LogFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    // Момент времени
    auto ts = *rec[timestamp];

    auto message_object = json::object{
        {"timestamp"s, to_iso_extended_string(ts)}
    };

    try {
        message_object["data"] = *rec[additional_data];
    } catch (const std::exception&) {}

    message_object["message"] = *rec[logging::expressions::smessage];

    // Выводим сообщение
    strm << json::serialize(message_object);
}

void InitLogger() {
    // Добавляем стандартные атрибуты (timestamp)
    logging::add_common_attributes();

    // Устанавливаем новый формат
    logging::add_console_log( 
        std::cout,
        keywords::format = &LogFormatter,
        keywords::auto_flush = true
    ); 
}

void LogData(std::string_view message, const boost::json::value& additional_data_value) {
    BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data, additional_data_value) << message;
}

}  // namespace json_logger