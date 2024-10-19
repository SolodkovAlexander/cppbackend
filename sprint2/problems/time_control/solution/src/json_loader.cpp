#include "json_loader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json_loader {
using namespace std::literals;

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::invalid_argument("Failed to open game file");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    auto game_data = json::parse(buffer.str()).as_object();

    Game game(
        game_data.contains("defaultDogSpeed"sv) ? game_data.at("defaultDogSpeed"sv).as_double() : Game::DEFAULT_SPEED
    );
    for (const auto& map_item : game_data.at("maps"s).as_array()) {
        game.AddMap(MapFromJson(map_item.as_object(), game));
    }

    return game;
}

Map MapFromJson(const json::object& obj, Game &game) {
    auto map = Map{
        Map::Id(obj.at("id"s).as_string().data()), 
        obj.at("name"s).as_string().data(),
        obj.contains("dogSpeed"sv) ? obj.at("dogSpeed"sv).as_double() : game.GetMapDefaultSpeed()
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

Road RoadFromJson(const json::object& obj) {
    auto start = Point{static_cast<Coord>(obj.at("x0"s).as_int64()), 
                        static_cast<Coord>(obj.at("y0"s).as_int64())};
    auto end_key = obj.contains("x1"s) ? "x1"s : "y1"s;
    auto end = Coord{static_cast<Coord>(obj.at(end_key).as_int64())};
    if (end_key == "x1"s) {
        return Road{Road::HORIZONTAL, start, end};
    }
    return Road{Road::VERTICAL, start, end};
}

Building BuildingFromJson(const json::object& obj) { 
    return Building{
        Rectangle{
            Point{static_cast<Coord>(obj.at("x"s).as_int64()), 
                  static_cast<Coord>(obj.at("y"s).as_int64())},
            Size{static_cast<Dimension>(obj.at("w"s).as_int64()), 
                 static_cast<Dimension>(obj.at("h"s).as_int64())}
        }                    
    };
}

Office OfficeFromJson(const json::object& obj) { 
    return Office{
        Office::Id(obj.at("id"s).as_string().data()), 
        Point{static_cast<Coord>(obj.at("x"s).as_int64()), 
              static_cast<Coord>(obj.at("y"s).as_int64())},
        Offset{static_cast<Dimension>(obj.at("offsetX"s).as_int64()), 
               static_cast<Dimension>(obj.at("offsetY"s).as_int64())}
    };
}

}  // namespace json_loader