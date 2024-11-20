#pragma once

#include "model.h"

#include <boost/json.hpp>
#include <filesystem>

namespace json_loader {

namespace json = boost::json;
using namespace model;

model::Game LoadGame(const std::filesystem::path& json_path);

model::Map MapFromJson(const json::object& obj, Game& game);
model::Road RoadFromJson(const json::object& obj);
model::Building BuildingFromJson(const json::object& obj);
model::Office OfficeFromJson(const json::object& obj);

}  // namespace json_loader