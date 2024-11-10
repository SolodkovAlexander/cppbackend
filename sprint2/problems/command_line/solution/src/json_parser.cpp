#include "json_parser.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json_parser {
using namespace std::literals;

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::invalid_argument("Failed to open game file");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    auto game_data = json::parse(buffer.str()).as_object();

    Game game(game_data.contains("defaultDogSpeed"sv) 
              ? game_data.at("defaultDogSpeed"sv).as_double() 
              : Game::DEFAULT_SPEED);
    for (const auto& map_item : game_data.at("maps"s).as_array()) {
        game.AddMap(MapFromJson(map_item.as_object(), game));
    }

    return game;
}

Map MapFromJson(const json::value& value, Game &game) {
    const auto& obj = value.as_object();
    auto map = Map{
        Map::Id(obj.at("id"s).as_string().data()), 
        obj.at("name"s).as_string().data(),
        obj.count("dogSpeed"s) ? obj.at("dogSpeed"s).as_double() : game.GetMapDefaultSpeed()
    };

    assert(obj.at("roads"s).as_array().size() > 0);
    for (const auto& item : obj.at("roads"s).as_array()) {
        map.AddRoad(RoadFromJson(item.as_object()));
    }
    for (const auto& item : obj.at("buildings"s).as_array()) {
        map.AddBuilding(BuildingFromJson(item.as_object()));
    }
    for (const auto& item : obj.at("offices"s).as_array()) {
        map.AddOffice(OfficeFromJson(item.as_object()));
    }

    return map;
}

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

json::value MapToJson(const Map* map) {
    json::object map_data;
    map_data["id"] = *(map->GetId());
    map_data["name"] = map->GetName();

    json::array roads;
    for (const auto& road : map->GetRoads()) {
        roads.emplace_back(RoadToJson(road));
    }
    map_data["roads"] = std::move(roads);

    json::array buildings;
    for (const auto& building : map->GetBuildings()) {
        buildings.emplace_back(BuildingToJson(building));
    }
    map_data["buildings"] = std::move(buildings);

    json::array offices;
    for (const auto& office : map->GetOffices()) {
        offices.emplace_back(OfficeToJson(office));
    }
    map_data["offices"] = std::move(offices);

    return map_data;
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