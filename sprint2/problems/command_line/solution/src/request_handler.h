#pragma once
#include <boost/algorithm/string.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <boost/url.hpp>
#include <filesystem>
#include <iostream>
#include <regex>
#include <variant>

#include "application.h"
#include "json_logger.h"
#include "http_server.h"

namespace http_handler {
namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace urls = boost::urls;
namespace sys = boost::system;
namespace fs = std::filesystem;
using namespace std::literals;
using namespace game_scenarios;

using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;
using RequestResponse = std::variant<StringResponse,FileResponse>;

class MakingResponseDurationLogger {
public:
    MakingResponseDurationLogger(RequestResponse& response) :
        response_(response)
    {}
    ~MakingResponseDurationLogger() {
        std::chrono::system_clock::time_point end_ts = std::chrono::system_clock::now();
        LogMadeResponseDuration(std::chrono::duration_cast<std::chrono::milliseconds>(end_ts - start_ts_).count());
    }

private:
    template <typename Duration>
    void LogMadeResponseDuration(Duration duration) {
        int code = 0;
        std::string content_type = "null"s;
        if (holds_alternative<StringResponse>(response_)) {
            auto resp = get<StringResponse>(response_);
            code = resp.result_int();
            try { content_type = resp.at(http::field::content_type); } catch (...) {}
        } else if (holds_alternative<FileResponse>(response_)) {
            code = get<FileResponse>(response_).result_int();
            try { content_type = get<FileResponse>(response_).at(http::field::content_type); } catch (...) {}
        }
        
        json_logger::LogData("response sent"sv,
                             boost::json::object{{"response_time", duration}, 
                                                 {"code", code},
                                                 {"content_type", content_type}});
    }

private:
    std::chrono::system_clock::time_point start_ts_ = std::chrono::system_clock::now();
    RequestResponse& response_;
};

enum class RequestType {
    Unknown,
    Api,
    StaticData
};

enum class ResponseErrorType {
    BadRequest,
    InvalidMethod,
    InvalidContentType,
    InvalidAuthorization,
    NoPlayerWithToken,
    InvalidJSON,
    EmptyPlayerName,
    InvalidMapId,
    MapNotFound,
    StaticDataFileNotFound,
    StaticDataFileNotSubPath
};

enum class ApiRequestType {
    Any,
    Map,
    Maps,
    GameJoin, 
    Players,
    GameState,
    Action,
    Tick  
};

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    explicit RequestHandler(Application& app, const std::string &static_data_path, Strand api_strand) :
          app_{app},
          static_data_path_{fs::weakly_canonical(static_data_path)},
          api_strand_{api_strand}
    {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        RequestType request_type = CheckRequestType(req);

        // 1. API request
        if (request_type == RequestType::Api) {
            auto handle = [self = shared_from_this(), send, req = std::forward<decltype(req)>(req)] {
                RequestResponse response;
                {
                    MakingResponseDurationLogger durationLogger(response);
                    response = self->HandleApiRequest(std::forward<decltype(req)>(req));
                }
                return self->SendResponse(std::move(response), std::move(send));
            };
            net::dispatch(api_strand_, handle);
            return;
        }

        // 2. Static data request
        if (request_type == RequestType::StaticData) {
            RequestResponse response;
            {
                MakingResponseDurationLogger durationLogger(response);
                response = HandleStaticDataRequest(std::move(req));
            }
            return SendResponse(std::move(response), std::move(send));
        }

        // 3. Bad request
        RequestResponse response;
        {
            MakingResponseDurationLogger durationLogger(response);
            response = MakeErrorResponse(ResponseErrorType::BadRequest, req);
        }
        return SendResponse(std::move(response), std::move(send));
    }

private:
    template <typename Body, typename Allocator>
    StringResponse HandleApiRequest(http::request<Body, http::basic_fields<Allocator>> req) {
        urls::decode_view url_decoded(req.target());

        if (url_decoded == "/api/v1/game/join"sv) {
            return HandleGameJoinRequest(req);
        }
        if (url_decoded == "/api/v1/game/players"sv) {
            return HandlePlayersRequest(req);
        }
        if (url_decoded == "/api/v1/game/state"sv) {
            return HandleGameStateRequest(req);
        }
        if (url_decoded == "/api/v1/game/player/action"sv) {
            return HandleActionRequest(req);
        }
        if (url_decoded == "/api/v1/game/tick"sv) {
            return HandleTickRequest(req);
        }
        if (url_decoded == "/api/v1/maps"sv) {
            return HandleMapsRequest(req);
        }

        // /api/v1/maps/{map_id}
        return HandleMapRequest(req);
    }

    template <typename Body, typename Allocator>
    StringResponse HandleGameJoinRequest(http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::post) {
            return MakeErrorResponse(ResponseErrorType::InvalidMethod, req, ApiRequestType::GameJoin);
        }

        std::string user_name, map_id;
        try {
            auto params = json::parse(req.body());
            user_name = params.as_object().at("userName"sv).as_string().c_str();
            map_id =  params.as_object().at("mapId"sv).as_string().c_str();
        } catch (...) { 
            return MakeErrorResponse(ResponseErrorType::InvalidJSON, req, ApiRequestType::GameJoin);
        }

        json::value join_game_result;
        try {
            join_game_result = app_.JoinGame(user_name, map_id);
        } catch (const AppErrorException& e) { 
            return MakeErrorResponse(e.GetCategory(), req, ApiRequestType::GameJoin);
        }
        return MakeStringResponse(http::status::ok, json::serialize(join_game_result), req);
    }

    template <typename Body, typename Allocator>
    StringResponse HandlePlayersRequest(http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return MakeErrorResponse(ResponseErrorType::InvalidMethod, req, ApiRequestType::Players);
        }

        return ExecuteAuthorized(req, [&req, this](const auto& token) {
            json::value players_state;
            try {
                players_state = app_.GetPlayers(token);
            } catch (const AppErrorException& e) { 
                return MakeErrorResponse(e.GetCategory(), req, ApiRequestType::Players);
            }
            return MakeStringResponse(http::status::ok, json::serialize(players_state), req);
        }, ApiRequestType::Players);
    }

    template <typename Body, typename Allocator>
    StringResponse HandleGameStateRequest(http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return MakeErrorResponse(ResponseErrorType::InvalidMethod, req, ApiRequestType::GameState);
        }
        
        return ExecuteAuthorized(req, [&req, this](const auto& token) {
            json::value game_state;
            try {
                game_state = app_.GetGameState(token);
            } catch (const AppErrorException& e) { 
                return MakeErrorResponse(e.GetCategory(), req, ApiRequestType::GameState);
            }
            return MakeStringResponse(http::status::ok, json::serialize(game_state), req);
        }, ApiRequestType::GameState);
    }

    template <typename Body, typename Allocator>
    StringResponse HandleActionRequest(http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::post) {
            return MakeErrorResponse(ResponseErrorType::InvalidMethod, req, ApiRequestType::Action);
        }
        if (boost::algorithm::to_lower_copy(std::string(req[http::field::content_type])) != ContentType::APPLICATION_JSON) {
            return MakeErrorResponse(ResponseErrorType::InvalidContentType, req, ApiRequestType::Action);
        }
        
        return ExecuteAuthorized(req, [&req, this](const players::Players::Token& token) {
            std::string direction_str;
            try {
                auto req_data = json::parse(req.body()).as_object();
                direction_str = req_data.at("move"sv).as_string().c_str();
            } catch (...) { 
                return MakeErrorResponse(ResponseErrorType::InvalidJSON, req, ApiRequestType::Action);
            }

            try {
                app_.ActionPlayer(token, direction_str);
            } catch (const AppErrorException& e) { 
                return MakeErrorResponse(e.GetCategory(), req, ApiRequestType::Action);
            }
            return MakeStringResponse(http::status::ok, json::serialize(json::object{}), req);
        }, ApiRequestType::Action);
    }

    template <typename Body, typename Allocator>
    StringResponse HandleTickRequest(http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::post) {
            return MakeErrorResponse(ResponseErrorType::InvalidMethod, req, ApiRequestType::Tick);
        }
        if (app_.GetAutoTick()) {
            return MakeErrorResponse(ResponseErrorType::BadRequest, req, ApiRequestType::Tick);
        }
        
        std::chrono::milliseconds time_delta{0ms};
        try {
            auto req_data = json::parse(req.body()).as_object();
            time_delta = std::chrono::milliseconds(req_data.at("timeDelta"sv).as_int64());
        } catch (...) { 
            return MakeErrorResponse(ResponseErrorType::InvalidJSON, req, ApiRequestType::Tick);
        }

        try {
            app_.Tick(time_delta);
        } catch (const AppErrorException& e) { 
            return MakeErrorResponse(e.GetCategory(), req, ApiRequestType::Tick);
        }
        return MakeStringResponse(http::status::ok, json::serialize(json::object{}), req);
    }

    template <typename Body, typename Allocator>
    StringResponse HandleMapsRequest(http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return MakeErrorResponse(ResponseErrorType::BadRequest, req, ApiRequestType::Maps);
        }
        return MakeStringResponse(http::status::ok, json::serialize(app_.GetMapsShortInfo()), req);
    }

    template <typename Body, typename Allocator>
    StringResponse HandleMapRequest(http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return MakeErrorResponse(ResponseErrorType::BadRequest, req, ApiRequestType::Map);
        }
        
        urls::decode_view url_decoded(req.target());

        static const auto map_id_regex = std::regex(R"(/api/v1/maps/(.+))");
        std::smatch match_results;
        std::string target_name(url_decoded.begin(), url_decoded.end());
        if (!regex_match(target_name, match_results, map_id_regex)) {
            return MakeErrorResponse(ResponseErrorType::InvalidMapId, req, ApiRequestType::Map);
        }
        
        json::value map_info;
        try {
            map_info = app_.GetMapInfo(match_results[1]);
        } catch (const AppErrorException& e) { 
            return MakeErrorResponse(ResponseErrorType::MapNotFound, req, ApiRequestType::Map);
        }

        return MakeStringResponse(http::status::ok, json::serialize(map_info), req);
    }

    template <typename Body, typename Allocator>
    RequestResponse HandleStaticDataRequest(http::request<Body, http::basic_fields<Allocator>>&& req) {
        urls::decode_view url_decoded(req.target());

        fs::path req_path{"." + std::string(url_decoded.begin(), url_decoded.end())};
        fs::path abs_path = fs::weakly_canonical(static_data_path_ / req_path);
        if (!IsSubPath(abs_path, static_data_path_)) {
            return MakeErrorResponse(ResponseErrorType::StaticDataFileNotSubPath, req);
        }

        if (fs::exists(abs_path) && fs::is_directory(abs_path)) {
            abs_path = fs::weakly_canonical(abs_path / "./index.html"s);
        }

        http::file_body::value_type file;
        if (sys::error_code ec; file.open(abs_path.c_str(), beast::file_mode::read, ec), ec) {
            return MakeErrorResponse(ResponseErrorType::StaticDataFileNotFound, req);
        }

        auto content_type = ContentType::GetContentTypeByFileExtension(abs_path);
        if (content_type == ContentType::UNKNOWN) {
            content_type = ContentType::APPLICATION_OCTET_STREAM;
        }

        return MakeFileResponse(http::status::ok, std::move(file), req, content_type);
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
                                             std::string_view content_type = ContentType::APPLICATION_JSON,
                                             bool with_cache = false) {
        StringResponse response(status, request.version());
        response.body() = body;
        response.content_length(body.size());
        response.keep_alive(request.keep_alive());
        response.set(http::field::content_type, content_type);
        if (!with_cache) {
            response.set(http::field::cache_control, "no-cache"sv);
        }

        return response;
    }

    template <typename Fn, typename Body, typename Allocator>
    static StringResponse ExecuteAuthorized(http::request<Body, http::basic_fields<Allocator>>& req, 
                                            Fn&& action, 
                                            ApiRequestType request_type) {
        if (auto token = TryExtractToken(std::string(req[http::field::authorization]))) {
            return action(*token);
        } else {
            return MakeErrorResponse(ResponseErrorType::InvalidAuthorization, req, request_type);
        }
    }
    
    static std::optional<std::string> TryExtractToken(std::string&& auth_header) {
        static const auto token_regex = std::regex(R"(Bearer\s([0-9a-fA-F]{32}))");
        std::smatch match_results;
        if (!regex_match(auth_header, match_results, token_regex)) {
            return {};
        }
        return boost::algorithm::to_lower_copy(match_results[1].str());
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

    template <typename Body, typename Allocator>
    static StringResponse MakeErrorResponse(ResponseErrorType error_type,
                                            http::request<Body, http::basic_fields<Allocator>>& req,
                                            ApiRequestType request_type = ApiRequestType::Any) {
        std::optional<StringResponse> result;

        switch (request_type) {
        case ApiRequestType::Players: {
            switch (error_type) {
                case ResponseErrorType::InvalidMethod: {
                    result = MakeStringResponse<Body, Allocator>(http::status::method_not_allowed, 
                                                                 json::serialize(json::object{
                                                                     {"code"sv, "invalidMethod"sv}, 
                                                                     {"message"sv, "Invalid method"sv}
                                                                 }), 
                                                                 req);
                    (*result).set(http::field::allow, "GET, HEAD"sv);
                    break;
                }
                case ResponseErrorType::InvalidAuthorization: {
                    result = MakeStringResponse<Body, Allocator>(http::status::unauthorized, 
                                                                 json::serialize(json::object{
                                                                     {"code"sv, "invalidToken"sv}, 
                                                                     {"message"sv, "Authorization header is missing"sv}
                                                                 }), 
                                                                 req);
                    break;
                }
                case ResponseErrorType::NoPlayerWithToken: {
                    result = MakeStringResponse<Body, Allocator>(http::status::unauthorized, 
                                                                 json::serialize(json::object{
                                                                     {"code"sv, "unknownToken"sv}, 
                                                                     {"message"sv, "Player token has not been found"sv}
                                                                 }), 
                                                                 req);
                    break;
                }
            }
            break;
        }
        case ApiRequestType::GameState: {
            switch (error_type) {
                case ResponseErrorType::InvalidMethod: {
                    result = MakeStringResponse<Body, Allocator>(http::status::method_not_allowed, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidMethod"sv}, 
                                                                    {"message"sv, "Invalid method"sv}
                                                                }), 
                                                                req);
                    (*result).set(http::field::allow, "GET, HEAD"sv);
                    break;
                }
                case ResponseErrorType::InvalidAuthorization: {
                    result = MakeStringResponse<Body, Allocator>(http::status::unauthorized, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidToken"sv}, 
                                                                    {"message"sv, "Authorization header is required"sv}
                                                                }), 
                                                                req);
                    break;
                }
                case ResponseErrorType::NoPlayerWithToken: {
                    result = MakeStringResponse<Body, Allocator>(http::status::unauthorized, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "unknownToken"sv}, 
                                                                    {"message"sv, "Player token has not been found"sv}
                                                                }), 
                                                                req);
                    break;
                }
            }
            break;
        }
        case ApiRequestType::GameJoin: {
            switch (error_type) {
                case ResponseErrorType::InvalidMethod: {
                    result = MakeStringResponse<Body, Allocator>(http::status::method_not_allowed, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidMethod"sv}, 
                                                                    {"message"sv, "Only POST method is expected"sv}
                                                                }), 
                                                                req);
                    (*result).set(http::field::allow, "POST"sv);
                    break;
                }
                case ResponseErrorType::EmptyPlayerName: {
                    result = MakeStringResponse<Body, Allocator>(http::status::bad_request, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidArgument"sv}, 
                                                                    {"message"sv, "Invalid name"sv}
                                                                }), 
                                                                req);
                    break;
                }
                case ResponseErrorType::InvalidMapId: {
                    result = MakeStringResponse<Body, Allocator>(http::status::not_found, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "mapNotFound"sv}, 
                                                                    {"message"sv, "Map not found"sv}
                                                                }), 
                                                                req);
                    break;
                }
                case ResponseErrorType::InvalidJSON: {
                    result = MakeStringResponse<Body, Allocator>(http::status::bad_request, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidArgument"sv}, 
                                                                    {"message"sv, "Join game request parse error"sv}
                                                                }), 
                                                                req);
                    break;
                }
            }
            break;
        }
        case ApiRequestType::Action: {
            switch (error_type)
            {
                case ResponseErrorType::InvalidMethod: {
                    result = MakeStringResponse<Body, Allocator>(http::status::method_not_allowed, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidMethod"sv}, 
                                                                    {"message"sv, "Invalid method"sv}
                                                                }), 
                                                                req);
                    (*result).set(http::field::allow, "POST"sv);
                    break;
                }
                case ResponseErrorType::InvalidContentType: {
                    result = MakeStringResponse<Body, Allocator>(http::status::bad_request, 
                                                                json::serialize(json::object({
                                                                    {"code", "invalidArgument"s},
                                                                    {"message", "Invalid content type"s}
                                                                })), 
                                                                req);
                    break;
                }
                case ResponseErrorType::InvalidJSON: {
                    result = MakeStringResponse<Body, Allocator>(http::status::bad_request, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidArgument"sv}, 
                                                                    {"message"sv, "Failed to parse action"sv}
                                                                }), 
                                                                req);
                    break;
                }
                case ResponseErrorType::NoPlayerWithToken: {
                    result = MakeStringResponse<Body, Allocator>(http::status::unauthorized, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "unknownToken"sv}, 
                                                                    {"message"sv, "Player token has not been found"sv}
                                                                }), 
                                                                req);
                    break;
                }
                case ResponseErrorType::InvalidAuthorization: {
                    result = MakeStringResponse<Body, Allocator>(http::status::unauthorized, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidToken"sv}, 
                                                                    {"message"sv, "Authorization header is required"sv}
                                                                }), 
                                                                req);
                    break;
                }
            }
            break;
        }
        case ApiRequestType::Tick: {
            switch (error_type)
            {
                case ResponseErrorType::BadRequest: {
                    result = MakeStringResponse<Body, Allocator>(http::status::bad_request, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidArgument"sv}, 
                                                                    {"message"sv, "Invalid endpoint"sv}
                                                                }), 
                                                                req);
                    break;
                }
                case ResponseErrorType::InvalidMethod: {
                    result = MakeStringResponse<Body, Allocator>(http::status::method_not_allowed, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidMethod"sv}, 
                                                                    {"message"sv, "Invalid method"sv}
                                                                }), 
                                                                req);
                    (*result).set(http::field::allow, "POST"sv);
                    break;
                }
                case ResponseErrorType::InvalidJSON: {
                    result = MakeStringResponse<Body, Allocator>(http::status::bad_request, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidArgument"sv}, 
                                                                    {"message"sv, "Failed to parse tick request JSON"sv}
                                                                }), 
                                                                req);
                    break;
                }
            }
            break;
        }
        case ApiRequestType::Maps: {
            switch (error_type) {
                case ResponseErrorType::BadRequest: {
                    result = MakeStringResponse<Body, Allocator>(http::status::bad_request, 
                                                                json::serialize(json::object({
                                                                    {"code", "badRequest"s},
                                                                    {"message", "Bad request"s}
                                                                })), 
                                                                req,
                                                                ContentType::APPLICATION_JSON,
                                                                true);
                    break;
                }
            }
            break;
        }
        case ApiRequestType::Map: {
            switch (error_type) {
                case ResponseErrorType::BadRequest:
                case ResponseErrorType::InvalidMapId: {
                    result = MakeStringResponse<Body, Allocator>(http::status::bad_request, 
                                                                json::serialize(json::object({
                                                                    {"code", "badRequest"s},
                                                                    {"message", "Bad request"s}
                                                                })), 
                                                                req,
                                                                ContentType::APPLICATION_JSON,
                                                                true);
                    break;
                }
                case ResponseErrorType::MapNotFound: {
                    result = MakeStringResponse<Body, Allocator>(http::status::not_found, 
                                                                json::serialize(json::object({
                                                                    {"code", "mapNotFound"s},
                                                                    {"message", "Map not found"s}
                                                                })), 
                                                                req,
                                                                ContentType::APPLICATION_JSON,
                                                                true);
                    break;
                }
            }
            break;
        }
        }
        if (result) {
            return *result;
        }        

        switch (error_type) {
            case ResponseErrorType::BadRequest: {
                result = MakeStringResponse<Body, Allocator>(http::status::bad_request, 
                                                             json::serialize(json::object({
                                                                {"code", "badRequest"s},
                                                                {"message", "Bad request"s}
                                                             })), 
                                                             req,
                                                             ContentType::APPLICATION_JSON,
                                                             true);
                break;
            }
            case ResponseErrorType::StaticDataFileNotFound: {
                result = MakeStringResponse(http::status::not_found, 
                                            "File not found"sv, 
                                            req, 
                                            ContentType::TEXT_PLAIN,
                                            true);
                break;
            }
            case ResponseErrorType::StaticDataFileNotSubPath: {
                result = MakeStringResponse(http::status::bad_request, 
                                            "No rights to path"sv, 
                                            req, 
                                            ContentType::TEXT_PLAIN,
                                            true);
                break;
            }
        }
        return *result;
    }

    template <typename Body, typename Allocator>
    static StringResponse MakeErrorResponse(AppErrorException::Category error_category,
                                            http::request<Body, http::basic_fields<Allocator>>& req,
                                            ApiRequestType request_type) {
        using ErrCategory = AppErrorException::Category;
        using ErrCategoryInfo = std::unordered_map<AppErrorException::Category, ResponseErrorType>;
        static const ErrCategoryInfo err_category_to_err_type{
            {ErrCategory::EmptyPlayerName, ResponseErrorType::EmptyPlayerName},
            {ErrCategory::InvalidMapId, ResponseErrorType::InvalidMapId},
            {ErrCategory::NoPlayerWithToken, ResponseErrorType::NoPlayerWithToken},
            {ErrCategory::InvalidDirection, ResponseErrorType::InvalidJSON},
            {ErrCategory::InvalidTime, ResponseErrorType::InvalidJSON}
        };
        return MakeErrorResponse(err_category_to_err_type.at(error_category), req, request_type);
    }

    template <typename Send>
    void SendResponse(RequestResponse&& response, Send&& send) {
        if (holds_alternative<StringResponse>(response)) {
            send(get<StringResponse>(response));
        } else if (holds_alternative<FileResponse>(response)) {
            send(get<FileResponse>(response));
        }
    }

private:
    static bool IsSubPath(fs::path path, fs::path base);

    template <typename Body, typename Allocator>
    static RequestType CheckRequestType(const http::request<Body, http::basic_fields<Allocator>> &req) {
        urls::decode_view url_decoded(req.target());
        if (url_decoded.starts_with("/api/"sv)) {
            return RequestType::Api;
        }        
        return RequestType::StaticData;
    }

private:
    fs::path static_data_path_;
    Strand api_strand_;

private:
    Application& app_;
};

}  // namespace http_handler
