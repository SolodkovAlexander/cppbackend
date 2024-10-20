#pragma once

#include "model.h"

#include <boost/json.hpp>
#include <filesystem>

namespace json_parser {

namespace json = boost::json;
using namespace model;

model::Game LoadGame(const std::filesystem::path& json_path);

model::Map MapFromJson(const json::value& value, Game& game);
model::Road RoadFromJson(const json::value& value);
model::Building BuildingFromJson(const json::value& value);
model::Office OfficeFromJson(const json::value& value);

json::value MapsToShortJson(const Game::Maps& maps);
json::value MapToJson(const Map* map);
json::value RoadToJson(const Road& road);
json::value BuildingToJson(const Building& building);
json::value OfficeToJson(const Office& office);

}  // namespace json_parser