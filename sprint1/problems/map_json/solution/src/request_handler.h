#pragma once
#include "http_server.h"
#include "model.h"

#include <boost/json.hpp>
#include <regex>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using namespace std::literals;
using namespace model;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Bad request-s bodies
        static const auto bad_request_response_body = json::serialize(json::object({
                                                           {"code", "badRequest"s},
                                                           {"message", "Bad request"s}
                                                       }));
        static const auto map_not_found_response_body = json::serialize(json::object({
                                                            {"code", "mapNotFound"s},
                                                            {"message", "Map not found"s}
                                                        }));
        // Not GET request
        if (req.method() != http::verb::get) {
            send(RequestHandler::MakeStringResponse(http::status::bad_request,
                                                    bad_request_response_body,
                                                    req));
            return;
        }

        // Request: /api/v1/maps
        if (req.target() == "/api/v1/maps"s) {
            send(RequestHandler::MakeStringResponse(http::status::ok, 
                                                    json::serialize(RequestHandler::MapsToShortJson(game_.GetMaps())), 
                                                    req));
            return;
        }

        // Request: /api/v1/maps/{map id}
        static const auto map_id_regex = std::regex(R"/(/api/v1/maps/(.+))/");
        std::smatch match_results;
        std::string target_name(req.target().begin(), req.target().end());
        if (!regex_match(target_name, match_results, map_id_regex)) {
            send(RequestHandler::MakeStringResponse(http::status::bad_request,
                                                    bad_request_response_body,
                                                    req));
            return;
        }

        // Request: /api/v1/maps/{map id}, no map with id
        auto map = game_.FindMap(model::Map::Id{match_results[1]});
        if (!map) {
            send(RequestHandler::MakeStringResponse(http::status::bad_request,
                                                    map_not_found_response_body,
                                                    req));
            return;
        }

        // Request: /api/v1/maps/{map id}, response
        send(RequestHandler::MakeStringResponse(http::status::ok, 
                                                json::serialize(RequestHandler::MapToJson(map)), 
                                                req));
        return;
    }

private:
    struct ContentType {
        ContentType() = delete;
        constexpr static std::string_view APPLICATION_JSON = "application/json"sv;
    };
    
    // Ответ, тело которого представлено в виде строки
    using StringResponse = http::response<http::string_body>;

    template <typename Body, typename Allocator>
    static StringResponse MakeStringResponse(http::status status, std::string_view body, http::request<Body, http::basic_fields<Allocator>>& request) {
        StringResponse response(status, request.version());
        response.set(http::field::content_type, ContentType::APPLICATION_JSON);
        response.body() = body;
        response.content_length(body.size());
        response.keep_alive(request.keep_alive());
        return response;
    }

private:
    static json::array MapsToShortJson(const Game::Maps& maps);
    static json::object MapToJson(const Map* map);
    static json::object RoadToJson(const Road& road);
    static json::object BuildingToJson(const Building& building);
    static json::object OfficeToJson(const Office& office);

private:
    model::Game& game_;
};

}  // namespace http_handler
