#include "json_parser.h"

#include <sstream>
#include <stdexcept>

namespace json_parser {
using namespace std::literals;

Road RoadFromJson(const json::value& value) {
    const auto& obj = value.as_object();
    auto start = Point{static_cast<Coord>(obj.at("x0"s).as_int64()), 
                        static_cast<Coord>(obj.at("y0"s).as_int64())};
    auto end_key = obj.contains("x1"s) ? "x1"s : "y1"s;
    auto end = Coord{static_cast<Coord>(obj.at(end_key).as_int64())};
    if (end_key == "x1"s) {
        return Road{Road::HORIZONTAL, start, end};
    }
    return Road{Road::VERTICAL, start, end};
}

Building BuildingFromJson(const json::value& value) { 
    const auto& obj = value.as_object();
    return Building{
        Rectangle{
            Point{static_cast<Coord>(obj.at("x"s).as_int64()), 
                  static_cast<Coord>(obj.at("y"s).as_int64())},
            Size{static_cast<Dimension>(obj.at("w"s).as_int64()), 
                 static_cast<Dimension>(obj.at("h"s).as_int64())}
        }                    
    };
}

Office OfficeFromJson(const json::value& value) { 
    const auto& obj = value.as_object();
    return Office{
        Office::Id(obj.at("id"s).as_string().data()), 
        Point{static_cast<Coord>(obj.at("x"s).as_int64()), 
              static_cast<Coord>(obj.at("y"s).as_int64())},
        Offset{static_cast<Dimension>(obj.at("offsetX"s).as_int64()), 
               static_cast<Dimension>(obj.at("offsetY"s).as_int64())}
    };
}

json::value MapsToShortJson(const Game::Maps& maps) {
    json::array maps_data;
    for (const auto& map : maps) {
        maps_data.emplace_back(json::object({
            {"id", *map.GetId()},
            {"name", map.GetName()}
        }));
    }
    return maps_data;
}

json::value RoadToJson(const Road& road) {
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

json::value BuildingToJson(const Building& building) {
    return json::object({
        {"x", building.GetBounds().position.x},
        {"y", building.GetBounds().position.y},
        {"w", building.GetBounds().size.width},
        {"h", building.GetBounds().size.height}
    });
}

json::value OfficeToJson(const Office& office) {
    return json::object({
        {"id", *(office.GetId())},
        {"x", office.GetPosition().x},
        {"y", office.GetPosition().y},
        {"offsetX", office.GetOffset().dx},
        {"offsetY", office.GetOffset().dy}
    });
}

}  // namespace json_parser