#pragma once

#include <boost/json.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/formatting_ostream.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)

namespace json_logger {

void LogFormatter(boost::log::record_view const& rec, boost::log::formatting_ostream& strm);

void InitLogger();

void LogData(std::string_view message, const boost::json::value& additional_data_value);

}  // namespace json_logger