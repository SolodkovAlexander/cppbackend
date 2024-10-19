#pragma once
#include "json_logger.h"
#include "model.h"
#include "http_server.h"
#include "players.h"

#include <boost/algorithm/string.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <boost/url.hpp>
#include <filesystem>
#include <iostream>
#include <regex>
#include <variant>

namespace http_handler {
namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace urls = boost::urls;
namespace sys = boost::system;
namespace fs = std::filesystem;
using namespace std::literals;
using namespace model;

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
    InvalidAuthorization,
    NoPlayerWithToken,
    InvalidJSON,
    EmptyPlayerName,
    InvalidMapId,
    InvalidMap,
    MapNotFound,
    StaticDataFileNotFound,
    StaticDataFileNotSubPath
};

enum class ApiRequestType {
    Any,
    Maps,
    GameJoin, 
    Players,
    GameState 
};

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    explicit RequestHandler(model::Game& game, players::Players& players, const std::string &static_data_path, Strand api_strand) :
          game_{game},
          players_{players},
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
    RequestResponse HandleApiRequest(http::request<Body, http::basic_fields<Allocator>> req) {
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

        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return MakeErrorResponse(ResponseErrorType::BadRequest, req);
        }
        if (url_decoded == "/api/v1/maps"sv) {
            return MakeStringResponse(http::status::ok, 
                                      json::serialize(MapsToShortJson(game_.GetMaps())), 
                                      req);
        }

        static const auto map_id_regex = std::regex(R"(/api/v1/maps/(.+))");
        std::smatch match_results;
        std::string target_name(url_decoded.begin(), url_decoded.end());
        if (!regex_match(target_name, match_results, map_id_regex)) {
            return MakeErrorResponse(ResponseErrorType::InvalidMap, req);
        }
        auto map = game_.FindMap(model::Map::Id{match_results[1]});
        if (!map) {
            return MakeErrorResponse(ResponseErrorType::MapNotFound, req);
        }

        return MakeStringResponse(http::status::ok, 
                                  json::serialize(MapToJson(map)), 
                                  req);
    }

    template <typename Body, typename Allocator>
    RequestResponse HandleGameJoinRequest(http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::post) {
            return MakeErrorResponse(ResponseErrorType::InvalidMethod, req, ApiRequestType::GameJoin);
        }

        std::string user_name;
        model::Map::Id map_id{""};
        try {
            auto req_data = json::parse(req.body()).as_object();
            user_name = req_data.at("userName"sv).as_string();
            *map_id =  req_data.at("mapId"sv).as_string();
        } catch (...) { 
            return MakeErrorResponse(ResponseErrorType::InvalidJSON, req, ApiRequestType::GameJoin);
        }

        if (user_name.empty()) {
            return MakeErrorResponse(ResponseErrorType::EmptyPlayerName, req, ApiRequestType::GameJoin);
        }
        auto map = game_.FindMap(map_id);
        if (!map) {
            return MakeErrorResponse(ResponseErrorType::InvalidMapId, req, ApiRequestType::GameJoin);
        }
        
        auto game_session = game_.FindSession(map);
        if (!game_session) {
            game_session = game_.CreateSession(map);
        }
        auto dog = game_session->CreateDog(user_name);
        auto player_info = players_.Add(dog, game_session);

        return MakeStringResponse(http::status::ok, 
                                  json::serialize(json::object{
                                     {"authToken"sv, player_info.token}, 
                                     {"playerId"sv, player_info.player->GetId()}
                                  }), 
                                  req, 
                                  ContentType::APPLICATION_JSON,
                                  "no-cache"sv);
    }

    template <typename Body, typename Allocator>
    RequestResponse HandlePlayersRequest(http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return MakeErrorResponse(ResponseErrorType::InvalidMethod, req, ApiRequestType::Players);
        }

        return ExecuteAuthorized(req, [&req, this](const players::Players::Token& token){
            auto player = players_.FindByToken(token);
            if (!player) {
                return MakeErrorResponse(ResponseErrorType::NoPlayerWithToken, req, ApiRequestType::Players);
            }
            json::object players_by_id;
            for (auto dog : player->GetSession()->GetDogs()) {
                players_by_id[std::to_string(dog->GetId())] = json::object{{"name"sv, dog->GetName()}};
            }

            return MakeStringResponse(http::status::ok, 
                                    json::serialize(players_by_id), 
                                    req, 
                                    ContentType::APPLICATION_JSON,
                                    "no-cache"sv);
        }, ApiRequestType::Players);
    }

    template <typename Body, typename Allocator>
    RequestResponse HandleGameStateRequest(http::request<Body, http::basic_fields<Allocator>>& req) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return MakeErrorResponse(ResponseErrorType::InvalidMethod, req, ApiRequestType::GameState);
        }
        
        return ExecuteAuthorized(req, [&req, this](const players::Players::Token& token){
            auto player = players_.FindByToken(token);
            if (!player) {
                return MakeErrorResponse(ResponseErrorType::NoPlayerWithToken, req, ApiRequestType::GameState);
            }
            json::object players_by_id;
            for (auto dog : player->GetSession()->GetDogs()) {
                players_by_id[std::to_string(dog->GetId())] = json::object{
                    {"pos"sv, json::array{dog->GetPosition().x, dog->GetPosition().y}},
                    {"speed"sv, json::array{dog->GetSpeed().x, dog->GetSpeed().y}},
                    {"dir"sv, std::string{dog->GetDirectionAsChar()}}
                };
            }

            return MakeStringResponse(http::status::ok, 
                                      json::serialize(json::object{{"players"sv, players_by_id}}), 
                                      req,
                                      ContentType::APPLICATION_JSON,
                                      "no-cache"sv);
        }, ApiRequestType::GameState);
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
                                             std::optional<std::string_view> cache_control = {}) {
        StringResponse response(status, request.version());
        response.body() = body;
        response.content_length(body.size());
        response.keep_alive(request.keep_alive());

        // Headers
        response.set(http::field::content_type, content_type);
        if (cache_control) {
            response.set(http::field::cache_control, *cache_control);
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
    static StringResponse MakeErrorResponse(ResponseErrorType response_error_type,
                                            http::request<Body, http::basic_fields<Allocator>>& req,
                                            ApiRequestType request_type = ApiRequestType::Any) {
        StringResponse result;
        switch (request_type) {
        case ApiRequestType::Players: {
            switch (response_error_type) {
                case ResponseErrorType::InvalidMethod: {
                    result = MakeStringResponse<Body, Allocator>(http::status::method_not_allowed, 
                                                                    json::serialize(json::object{
                                                                        {"code"sv, "invalidMethod"sv}, 
                                                                        {"message"sv, "Invalid method"sv}
                                                                    }), 
                                                                    req, 
                                                                    ContentType::APPLICATION_JSON,
                                                                    "no-cache"sv);
                    result.set(http::field::allow, "GET, HEAD"sv);
                    break;
                }
                case ResponseErrorType::InvalidAuthorization: {
                    result = MakeStringResponse<Body, Allocator>(http::status::unauthorized, 
                                                                    json::serialize(json::object{
                                                                        {"code"sv, "invalidToken"sv}, 
                                                                        {"message"sv, "Authorization header is missing"sv}
                                                                    }), 
                                                                    req, 
                                                                    ContentType::APPLICATION_JSON,
                                                                    "no-cache"sv);
                    break;
                }
                case ResponseErrorType::NoPlayerWithToken: {
                    result = MakeStringResponse<Body, Allocator>(http::status::unauthorized, 
                                                                    json::serialize(json::object{
                                                                        {"code"sv, "unknownToken"sv}, 
                                                                        {"message"sv, "Player token has not been found"sv}
                                                                    }), 
                                                                    req, 
                                                                    ContentType::APPLICATION_JSON,
                                                                    "no-cache"sv);
                    break;
                }
            }
            break;
        }
        case ApiRequestType::GameState: {
            switch (response_error_type) {
                case ResponseErrorType::InvalidMethod: {
                    result = MakeStringResponse<Body, Allocator>(http::status::method_not_allowed, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidMethod"sv}, 
                                                                    {"message"sv, "Invalid method"sv}
                                                                }), 
                                                                req, 
                                                                ContentType::APPLICATION_JSON,
                                                                "no-cache"sv);
                    result.set(http::field::allow, "GET, HEAD"sv);
                    break;
                }
                case ResponseErrorType::InvalidAuthorization: {
                    result = MakeStringResponse<Body, Allocator>(http::status::unauthorized, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidToken"sv}, 
                                                                    {"message"sv, "Authorization header is required"sv}
                                                                }), 
                                                                req, 
                                                                ContentType::APPLICATION_JSON,
                                                                "no-cache"sv);
                    break;
                }
                case ResponseErrorType::NoPlayerWithToken: {
                    result = MakeStringResponse<Body, Allocator>(http::status::unauthorized, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "unknownToken"sv}, 
                                                                    {"message"sv, "Player token has not been found"sv}
                                                                }), 
                                                                req, 
                                                                ContentType::APPLICATION_JSON,
                                                                "no-cache"sv);
                    break;
                }
            }
            break;
        }
        case ApiRequestType::GameJoin: {
            switch (response_error_type) {
                case ResponseErrorType::InvalidMethod: {
                    result = MakeStringResponse<Body, Allocator>(http::status::method_not_allowed, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidMethod"sv}, 
                                                                    {"message"sv, "Only POST method is expected"sv}
                                                                }), 
                                                                req, 
                                                                ContentType::APPLICATION_JSON,
                                                                "no-cache"sv);
                    result.set(http::field::allow, "POST"sv);
                    break;
                }
                case ResponseErrorType::EmptyPlayerName: {
                    result = MakeStringResponse<Body, Allocator>(http::status::bad_request, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidArgument"sv}, 
                                                                    {"message"sv, "Invalid name"sv}
                                                                }), 
                                                                req, 
                                                                ContentType::APPLICATION_JSON,
                                                                "no-cache"sv);
                    break;
                }
                case ResponseErrorType::InvalidMapId: {
                    result = MakeStringResponse<Body, Allocator>(http::status::not_found, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "mapNotFound"sv}, 
                                                                    {"message"sv, "Map not found"sv}
                                                                }), 
                                                                req, 
                                                                ContentType::APPLICATION_JSON, 
                                                                "no-cache"sv);
                    break;
                }
                case ResponseErrorType::InvalidJSON: {
                    result = MakeStringResponse<Body, Allocator>(http::status::bad_request, 
                                                                json::serialize(json::object{
                                                                    {"code"sv, "invalidArgument"sv}, 
                                                                    {"message"sv, "Join game request parse error"sv}
                                                                }), 
                                                                req, 
                                                                ContentType::APPLICATION_JSON,
                                                                "no-cache"sv);
                    break;
                }
            }
            break;
        }
        }

        switch (response_error_type) {
            case ResponseErrorType::BadRequest:
            case ResponseErrorType::InvalidMap: {
                result = MakeStringResponse<Body, Allocator>(http::status::bad_request, 
                                                             json::serialize(json::object({
                                                                {"code", "badRequest"s},
                                                                {"message", "Bad request"s}
                                                             })), 
                                                             req);
                break;
            }
            case ResponseErrorType::MapNotFound: {
                result = MakeStringResponse<Body, Allocator>(http::status::not_found, 
                                                             json::serialize(json::object({
                                                                {"code", "mapNotFound"s},
                                                                {"message", "Map not found"s}
                                                             })), 
                                                             req);
                break;
            }
            case ResponseErrorType::StaticDataFileNotFound: {
                result = MakeStringResponse(http::status::not_found, 
                                            "File not found"sv, 
                                            req, 
                                            ContentType::TEXT_PLAIN);
                break;
            }
            case ResponseErrorType::StaticDataFileNotSubPath: {
                result = MakeStringResponse(http::status::bad_request, 
                                            "No rights to path"sv, 
                                            req, 
                                            ContentType::TEXT_PLAIN);
                break;
            }
        }
        return result;
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
    static json::array MapsToShortJson(const Game::Maps& maps);
    static json::object MapToJson(const Map* map);
    static json::object RoadToJson(const Road& road);
    static json::object BuildingToJson(const Building& building);
    static json::object OfficeToJson(const Office& office);

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
    model::Game& game_;
    players::Players& players_;
    fs::path static_data_path_;
    Strand api_strand_;
};

}  // namespace http_handler
