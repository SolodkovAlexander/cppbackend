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

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;
    
    // Ответ, тело которого представлено в виде строки
    using StringResponse = http::response<http::string_body>;
    
    struct ContentType {
        ContentType() = delete;
        constexpr static std::string_view APPLICATION_JSON = "application/json"sv;
    };

    // Создаёт StringResponse с заданными параметрами
    StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                    bool keep_alive,
                                    std::string_view content_type = ContentType::APPLICATION_JSON) {
        StringResponse response(status, http_version);
        response.set(http::field::content_type, content_type);
        response.body() = body;
        response.content_length(body.size());
        response.keep_alive(keep_alive);
        return response;
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        static const auto make_good_request_body = [&req](const std::string& response) {
            std::string request_body;

            return request_body;
        };

        json::object bad_request_body;
        bad_request_body["code"] = "badRequest"s;
        bad_request_body["message"] = "Bad request"s;

        if (req.method() != http::verb::get) {
            send(MakeStringResponse(http::status::bad_request,
                                    json::serialize(bad_request_body),
                                    req.version(),
                                    req.keep_alive()));
            return;
        }

        // /api/v1/maps
        if (req.target() == "/api/v1/maps"s) {
            // JSON с картами
            json::array maps;
            for (const auto& map : game_.GetMaps()) {
                json::object map_data;
                map_data["id"] = *map.GetId();
                map_data["name"] = map.GetName();
                maps.emplace_back(map_data);
            }
            
            send(MakeStringResponse(http::status::ok,
                                    json::serialize(maps),
                                    req.version(),
                                    req.keep_alive()));
            return;
        }

        // /api/v1/maps/{id-карты}
        static const auto map_id_regex = std::regex(R"/(/api/v1/maps/(.+)?)/");
        std::smatch match_results;
        std::string target_name(req.target().begin(), req.target().end());
        if (!regex_match(target_name, match_results, map_id_regex)) {
            send(MakeStringResponse(http::status::bad_request,
                                    json::serialize(bad_request_body),
                                    req.version(),
                                    req.keep_alive()));
            return;
        }
        auto map = game_.FindMap(model::Map::Id{match_results[1]});
        if (!map) {
            bad_request_body["code"] = "mapNotFound"s;
            bad_request_body["message"] = "Map not found"s;
            send(MakeStringResponse(http::status::bad_request,
                                    json::serialize(bad_request_body),
                                    req.version(),
                                    req.keep_alive()));
            return;
        }

        // /api/v1/maps/{id-карты}
        json::object map_data;
        map_data["id"] = *(map->GetId());
        map_data["name"] = map->GetName();

        {
            json::array roads;
            for (const auto& road : map->GetRoads()) {
                json::object road_data;
                road_data["x0"] = road.GetStart().x;
                road_data["y0"] = road.GetStart().y;
                if (road.IsHorizontal()) {
                    road_data["x1"] = road.GetEnd().x;
                } else {
                    road_data["y1"] = road.GetEnd().y;
                }
                roads.emplace_back(std::move(road_data));
            }
            map_data["roads"] = std::move(roads);
        }
        {
            json::array buildings;
            for (const auto& building : map->GetBuildings()) {
                json::object building_data;
                building_data["x"] = building.GetBounds().position.x;
                building_data["y"] = building.GetBounds().position.y;
                building_data["w"] = building.GetBounds().size.width;
                building_data["h"] = building.GetBounds().size.height;
                buildings.emplace_back(std::move(building_data));
            }
            map_data["buildings"] = std::move(buildings);
        }
        {
            json::array offices;
            for (const auto& office : map->GetOffices()) {
                json::object office_data;
                office_data["id"] = *(office.GetId());
                office_data["x"] = office.GetPosition().x;
                office_data["y"] = office.GetPosition().y;
                office_data["offsetX"] = office.GetOffset().dx;
                office_data["offsetY"] = office.GetOffset().dy;
                offices.emplace_back(std::move(office_data));
            }
            map_data["offices"] = std::move(offices);
        }

        send(MakeStringResponse(http::status::ok,
                                json::serialize(map_data),
                                req.version(),
                                req.keep_alive()));
        return;
    }

private:
    model::Game& game_;
};

}  // namespace http_handler
