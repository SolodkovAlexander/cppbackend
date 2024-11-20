#pragma once
#include "http_server.h"
#include "model.h"
#include "json_logger.h"

#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <boost/url.hpp>
#include <filesystem>
#include <iostream>
#include <regex>
#include <variant>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace urls = boost::urls;
namespace sys = boost::system;
namespace fs = std::filesystem;
using namespace std::literals;
using namespace model;

class RequestHandler {
private:
    // Ответ, тело которого представлено в виде строки
    using StringResponse = http::response<http::string_body>;
    using FileResponse = http::response<http::file_body>;
    using RequestResponse = std::variant<std::monostate,StringResponse,FileResponse>;

public:
    explicit RequestHandler(model::Game& game, const std::string &static_data_path)
        : game_{game},
          static_data_path_{fs::weakly_canonical(static_data_path)}
    {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Calculate time: START
        std::chrono::system_clock::time_point start_ts_ = std::chrono::system_clock::now();

        RequestResponse response;

        // Check: HTTP method
        if (req.method() != http::verb::get 
            && req.method() != http::verb::head) {
            response = MakeStringResponse(http::status::bad_request, GetBadRequestResponseBodyStr(), req);
        }

        //Check: request type
        urls::decode_view url_decoded(req.target());
        if (!holds_alternative<StringResponse>(response)) {
            const static std::string api_url_begin = "/api/"s;
            if (url_decoded.starts_with(api_url_begin)) {
                // API request
                response = HandleApiRequest(std::move(req), url_decoded);
            }
        }

        // Static data request
        if (!holds_alternative<StringResponse>(response)) {
            response = HandleStaticDataRequest(std::move(req), url_decoded);
        }
        
        // Calculate time: FINISH
        std::chrono::system_clock::time_point end_ts = std::chrono::system_clock::now();

        // Log
        LogMadeResponseDuration(response, std::chrono::duration_cast<std::chrono::milliseconds>(end_ts - start_ts_).count());

        // Sending response
        SendResponse(std::move(response), std::move(send));
    }

private:
    template <typename Body, typename Allocator>
    StringResponse HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req, 
                                    urls::decode_view url_decoded) {
        // Check: /api/v1/maps
        if (url_decoded == "/api/v1/maps"s) {
            return MakeStringResponse(http::status::ok, json::serialize(MapsToShortJson(game_.GetMaps())), req);
        }

        // Check: /api/v1/maps/{map id}
        static const auto map_id_regex = std::regex(R"/(/api/v1/maps/(.+))/");
        std::smatch match_results;
        std::string target_name(url_decoded.begin(), url_decoded.end());
        if (!regex_match(target_name, match_results, map_id_regex)) {
            return MakeStringResponse(http::status::bad_request, GetBadRequestResponseBodyStr(), req);
        }

        // Check map existing
        auto map = game_.FindMap(model::Map::Id{match_results[1]});
        if (!map) {
            return MakeStringResponse(http::status::not_found, GetMapNotFoundResponseBodyStr(), req);
        }

        return MakeStringResponse(http::status::ok, json::serialize(MapToJson(map)), req);
    }

    template <typename Body, typename Allocator>
    RequestResponse HandleStaticDataRequest(http::request<Body, http::basic_fields<Allocator>>&& req, 
                                            urls::decode_view url_decoded) {
        // Check for valid path
        fs::path req_path{"." + std::string(url_decoded.begin(), url_decoded.end())};
        fs::path abs_path = fs::weakly_canonical(static_data_path_ / req_path);
        if (!IsSubPath(abs_path, static_data_path_)) {
            return MakeStringResponse(http::status::bad_request, GetNotSubPathResponseBodyStr(), req, ContentType::TEXT_PLAIN);
        }

        // Check for path is directory
        if (fs::exists(abs_path) && fs::is_directory(abs_path)) {
            abs_path = fs::weakly_canonical(abs_path / "./index.html"s);
        }

        // Open file
        http::file_body::value_type file;
        if (sys::error_code ec; file.open(abs_path.c_str(), beast::file_mode::read, ec), ec) {
            return MakeStringResponse(http::status::not_found, GetFileNotFoundResponseBodyStr(), req, ContentType::TEXT_PLAIN);
        }

        // Analyze content type
        auto content_type = ContentType::GetContentTypeByFileExtension(abs_path);
        if (content_type == ContentType::UNKNOWN) {
            content_type = ContentType::APPLICATION_OCTET_STREAM;
        }

        return MakeFileResponse(http::status::ok, std::move(file), req, content_type);
    }

    static std::string GetBadRequestResponseBodyStr() {
        static const auto result(json::serialize(json::object({
                                     {"code", "badRequest"s},
                                     {"message", "Bad request"s}
                                 })));
        return result;
    }
    static std::string GetMapNotFoundResponseBodyStr() {
        static const auto result(json::serialize(json::object({
                                     {"code", "mapNotFound"s},
                                     {"message", "Map not found"s}
                                 })));
        return result;
    }
    static std::string GetFileNotFoundResponseBodyStr() {
        static const auto result = "File not found"s;
        return result;
    }
    static std::string GetNotSubPathResponseBodyStr() {
        static const auto result = "No rights to path"s;
        return result;
    }

private:
    struct ContentType {
        ContentType() = delete;
        constexpr static std::string_view TEXT_HTML = "text/html"sv;
        constexpr static std::string_view TEXT_CSS = "text/css"sv;
        constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
        constexpr static std::string_view TEXT_JAVASCRIPT = "text/javascript"sv;
        constexpr static std::string_view APPLICATION_JSON = "application/json"sv;
        constexpr static std::string_view APPLICATION_XML = "application/xml"sv;
        constexpr static std::string_view APPLICATION_OCTET_STREAM = "application/octet-stream"sv;
        constexpr static std::string_view IMAGE_PNG = "image/png"sv;
        constexpr static std::string_view IMAGE_JPEG = "image/jpeg"sv;
        constexpr static std::string_view IMAGE_GIF = "image/gif"sv;
        constexpr static std::string_view IMAGE_BMP = "image/bmp"sv;
        constexpr static std::string_view IMAGE_MICROSOFT_ICON = "image/vnd.microsoft.icon"sv;
        constexpr static std::string_view IMAGE_TIFF = "image/tiff"sv;
        constexpr static std::string_view IMAGE_SVG_XML = "image/svg+xml"sv;
        constexpr static std::string_view AUDIO_MPEG = "audio/mpeg"sv;
        constexpr static std::string_view UNKNOWN = ""sv;
        
        static std::string_view GetContentTypeByFileExtension(fs::path file_path);
    };

    template <typename Body, typename Allocator>
    static StringResponse MakeStringResponse(http::status status, 
                                             std::string_view body, 
                                             http::request<Body, http::basic_fields<Allocator>>& request,
                                             std::string_view content_type = ContentType::APPLICATION_JSON) {
        StringResponse response(status, request.version());
        response.set(http::field::content_type, content_type);
        response.body() = body;
        response.content_length(body.size());
        response.keep_alive(request.keep_alive());
        return response;
    }

    template <typename Body, typename Allocator>
    static FileResponse MakeFileResponse(http::status status, 
                                         http::file_body::value_type&& file, 
                                         http::request<Body, http::basic_fields<Allocator>>& request,
                                         std::string_view content_type) {
        FileResponse response(status, request.version());
        response.set(http::field::content_type, content_type);
        response.keep_alive(request.keep_alive());
        response.body() = std::move(file);
        response.prepare_payload();
        return response;
    }

    template <typename Send>
    void SendResponse(RequestResponse&& response, Send&& send) {
        if (holds_alternative<StringResponse>(response)) {
            send(get<StringResponse>(response));
        } else if (holds_alternative<FileResponse>(response)) {
            send(get<FileResponse>(response));
        }
    }

    template <typename Duration>
    void LogMadeResponseDuration(const RequestResponse& response, Duration duration) {
        if (holds_alternative<std::monostate>(response)) {
            return;
        }

        int code = 0;
        std::string content_type = "null"s;
        if (holds_alternative<StringResponse>(response)) {
            auto resp = get<StringResponse>(response);
            code = resp.result_int();
            try { content_type = resp.at(http::field::content_type); } catch (...) {}
        } else if (holds_alternative<FileResponse>(response)) {
            code = get<FileResponse>(response).result_int();
            try { content_type = get<FileResponse>(response).at(http::field::content_type); } catch (...) {}
        }
        json_logger::LogData("response sent"sv,
                             boost::json::object{{"response_time", duration}, 
                                                 {"code", code},
                                                 {"content_type", content_type}});
    }

    template <typename Duration, typename ResponseAsValueType>
    void LogMadeResponseDurationImpl(const ResponseAsValueType& response, Duration duration) {
        if (holds_alternative<std::monostate>(response)) {
            return;
        }

        int code = 0;
        std::string content_type;
        code = get<ResponseAsValueType>(response).result_int();
        try { 
            content_type = get<FileResponse>(response).at(http::field::content_type); 
        } catch (...) {}
        if (content_type.empty()) {
            content_type = "null"s;
        }
        json_logger::LogData("response sent"sv,
                             boost::json::object{{"response_time", duration}, 
                                                 {"code", code},
                                                 {"content_type", content_type}});
    }

private:
    static json::array MapsToShortJson(const Game::Maps& maps);
    static json::object MapToJson(const Map* map);
    static json::object RoadToJson(const Road& road);
    static json::object BuildingToJson(const Building& building);
    static json::object OfficeToJson(const Office& office);

private:
    static bool IsSubPath(fs::path path, fs::path base);

private:
    model::Game& game_;
    fs::path static_data_path_;
};

}  // namespace http_handler
