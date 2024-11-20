#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <sstream>

namespace json_loader {

namespace json = boost::json;
using namespace std::literals;

model::Game LoadGame(const std::filesystem::path& json_path) {
    using namespace model;
    Game game;

    // Загрузить модель игры из файла
    // Загрузить содержимое файла json_path, например, в виде строки
    std::ifstream file(json_path);
    if (!file.is_open()) {
        return game;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    // Распарсить строку как JSON, используя boost::json::parse
    try {
        auto game_data = json::parse(buffer.str());
        for (const auto& map_item : game_data.as_object().at("maps"s).as_array()) {
            auto map_data = map_item.as_object();
            auto map = Map{
                Map::Id(map_data.at("id"s).as_string().data()), 
                map_data.at("name"s).as_string().data()
            };

            // Дороги
            assert(map_data.at("roads"s).as_array().size() > 0);
            for (const auto& item : map_data.at("roads"s).as_array()) {
                auto road_data = item.as_object();
                auto start = Point{static_cast<Coord>(road_data.at("x0"s).as_int64()), 
                                   static_cast<Coord>(road_data.at("y0"s).as_int64())};
                auto end_key = road_data.contains("x1"s) ? "x1"s : "y1"s;
                auto end = Coord{static_cast<Coord>(road_data.at(end_key).as_int64())};
                if (end_key == "x1"s) {
                     map.AddRoad(Road{Road::HORIZONTAL, start, end});
                } else {
                    map.AddRoad(Road{Road::VERTICAL, start, end});
                }
            }

            // Здания
            for (const auto& item : map_data.at("buildings"s).as_array()) {
                auto building_data = item.as_object();
                map.AddBuilding(Building{
                    Rectangle{
                        Point{static_cast<Coord>(building_data.at("x"s).as_int64()), 
                              static_cast<Coord>(building_data.at("y"s).as_int64())},
                        Size{static_cast<Dimension>(building_data.at("w"s).as_int64()), 
                             static_cast<Dimension>(building_data.at("h"s).as_int64())}
                    }                    
                });
            }

            // Оффисы
            for (const auto& item : map_data.at("offices"s).as_array()) {
                auto office_data = item.as_object();
                map.AddOffice(Office{
                    Office::Id(office_data.at("id"s).as_string().data()), 
                    Point{static_cast<Coord>(office_data.at("x"s).as_int64()), 
                          static_cast<Coord>(office_data.at("y"s).as_int64())},
                    Offset{static_cast<Dimension>(office_data.at("offsetX"s).as_int64()), 
                           static_cast<Dimension>(office_data.at("offsetY"s).as_int64())}
                });
            }
            
            game.AddMap(std::move(map));
        }
    } catch (...) {
        throw;
    }    

    return game;
}

}  // namespace json_loader
