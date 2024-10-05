#include "request_handler.h"

namespace http_handler {

json::array RequestHandler::MapsToShortJson(const Game::Maps& maps) {
    json::array maps_data;
    for (const auto& map : maps) {
        maps_data.emplace_back(json::object({
            {"id", *map.GetId()},
            {"name", map.GetName()}
        }));
    }
    return maps_data;
}

json::object RequestHandler::MapToJson(const Map* map) {
    json::object map_data;
    map_data["id"] = *(map->GetId());
    map_data["name"] = map->GetName();

    json::array roads;
    for (const auto& road : map->GetRoads()) {
        roads.emplace_back(RequestHandler::RoadToJson(road));
    }
    map_data["roads"] = std::move(roads);

    json::array buildings;
    for (const auto& building : map->GetBuildings()) {
        buildings.emplace_back(RequestHandler::BuildingToJson(building));
    }
    map_data["buildings"] = std::move(buildings);

    json::array offices;
    for (const auto& office : map->GetOffices()) {
        offices.emplace_back(RequestHandler::OfficeToJson(office));
    }
    map_data["offices"] = std::move(offices);

    return map_data;
}

json::object RequestHandler::RoadToJson(const Road& road) {
    json::object road_data({
        {"x0", road.GetStart().x},
        {"y0", road.GetStart().y}
    });
    if (road.IsHorizontal()) {
        road_data["x1"] = road.GetEnd().x;
    } else {
        road_data["y1"] = road.GetEnd().y;
    }
    return road_data;
}

json::object RequestHandler::BuildingToJson(const Building& building) {
    return json::object({
        {"x", building.GetBounds().position.x},
        {"y", building.GetBounds().position.y},
        {"w", building.GetBounds().size.width},
        {"h", building.GetBounds().size.height}
    });
}

json::object RequestHandler::OfficeToJson(const Office& office) {
    return json::object({
        {"id", *(office.GetId())},
        {"x", office.GetPosition().x},
        {"y", office.GetPosition().y},
        {"offsetX", office.GetOffset().dx},
        {"offsetY", office.GetOffset().dy}
    });
}

}  // namespace http_handler
