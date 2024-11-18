#pragma once

#include "model.h"

#include <boost/json.hpp>
#include <filesystem>
#include <fstream>

namespace json_parser {

namespace json = boost::json;
using namespace model;

model::Road RoadFromJson(const json::value& value);
model::Building BuildingFromJson(const json::value& value);
model::Office OfficeFromJson(const json::value& value);

template <typename ExtraData>
model::Map MapFromJson(const json::value& value, Game& game, ExtraData &extra_data) {
    using namespace std::literals;

    const auto& obj = value.as_object();
    std::string map_id = obj.at("id"s).as_string().data();
    auto map = Map{
        Map::Id(map_id), 
        obj.at("name"s).as_string().data(),
        obj.count("dogSpeed"s) ? obj.at("dogSpeed"s).as_double() : game.GetMapDefaultSpeed(),
        obj.count("bagCapacity"s) ? obj.at("bagCapacity"s).as_int64() : game.GetMapDefaultBagCapacity()
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

    extra_data.map_id_to_loot_types[map_id] = obj.at("lootTypes"s).as_array();
    
    return map;
}

template <typename ExtraData>
std::tuple<model::Game, ExtraData> LoadGame(const std::filesystem::path& json_path) {
    using namespace std::literals;
    using namespace std::chrono;

    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::invalid_argument("Failed to open game file");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    auto game_data = json::parse(buffer.str()).as_object();

    auto loot_generator_config = game_data.at("lootGeneratorConfig"sv).as_object();
    ExtraData extra_data;
    double base_interval = loot_generator_config.at("period"sv).as_double();
    extra_data.base_interval = duration_cast<milliseconds>(duration_cast<seconds>(duration<double>(base_interval)));
    extra_data.probability = loot_generator_config.at("probability"sv).as_double();

    Game game(game_data.contains("defaultDogSpeed"sv) 
              ? game_data.at("defaultDogSpeed"sv).as_double() 
              : Game::DEFAULT_SPEED,
              game_data.contains("defaultBagCapacity"sv) 
              ? game_data.at("defaultBagCapacity"sv).as_int64() 
              : Game::DEFAULT_BAG_CAPACITY);
    for (const auto& map_item : game_data.at("maps"s).as_array()) {
        game.AddMap(MapFromJson(map_item.as_object(), game, extra_data));
    }

    return { std::move(game), extra_data };
}

json::value MapsToShortJson(const Game::Maps& maps);
json::value RoadToJson(const Road& road);
json::value BuildingToJson(const Building& building);
json::value OfficeToJson(const Office& office);

template <typename ExtraData>
json::value MapToJson(const Map* map, const ExtraData& extra_data) {
    json::object map_data;
    map_data["id"] = *(map->GetId());
    map_data["name"] = map->GetName();
    map_data["lootTypes"] = extra_data.map_id_to_loot_types.at(*(map->GetId()));

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

}  // namespace json_parser