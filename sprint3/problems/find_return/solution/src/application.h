#pragma once

#include "json_parser.h"
#include "loot_generator.h"
#include "model.h"
#include "players.h"

#include <boost/json.hpp>
#include <string>
#include <unordered_map>

namespace game_scenarios {
using namespace model;
using namespace players;

namespace json = boost::json;
using namespace std::literals;
 
class AppErrorException : public std::invalid_argument { 
public: 
    using std::invalid_argument::invalid_argument;

    enum class Category { 
        EmptyPlayerName,
        NoPlayerWithToken,
        InvalidMapId,
        InvalidDirection,
        InvalidTime
    }; 

public: 
    explicit AppErrorException(std::string error, AppErrorException::Category category)
         : std::invalid_argument(error)
         , category_(category)
    {} 
 
    AppErrorException::Category GetCategory() const noexcept { 
        return category_;
    }
 
private: 
    AppErrorException::Category category_;
}; 

struct ExtraData {
    ExtraData() = default;

    loot_gen::LootGenerator::TimeInterval base_interval{0ms};
    double probability{0.0};
    std::unordered_map<std::string, json::array> map_id_to_loot_types;
};

class Application {
public:
    Application(Game&& game, ExtraData&& extra_data, bool randomize_spawn_points = false, bool auto_tick_enabled = false) 
        : game_(std::move(game))
        , extra_data_(std::move(extra_data))
        , randomize_spawn_points_(randomize_spawn_points)
        , auto_tick_enabled_(auto_tick_enabled)
        , loot_generator_(loot_gen::LootGenerator(extra_data.base_interval, extra_data.probability)) {
    }

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

public:
    bool GetAutoTick() const noexcept {
        return auto_tick_enabled_;
    }

public:
    json::value GetMapsShortInfo() const noexcept {
        return json_parser::MapsToShortJson(game_.GetMaps());
    }

    json::value GetMapInfo(const std::string& map_id) const {
        auto map = game_.FindMap(model::Map::Id{map_id});
        if (!map) {
            throw AppErrorException("Map not found"s, AppErrorException::Category::InvalidMapId);
        }
        return json_parser::MapToJson(map, extra_data_);
    }

    json::value GetPlayers(const Players::Token& player_token) {
        auto player = players_.FindByToken(player_token);
        if (!player) {
            throw AppErrorException("No player with token"s, AppErrorException::Category::NoPlayerWithToken);
        }
        
        json::object players_by_id;
        for (const auto& dog : player->GetSession()->GetDogs()) {
            players_by_id[std::to_string(dog->GetId())] = json::object{{"name"sv, dog->GetName()}};
        }
        return players_by_id;
    }

    json::value JoinGame(const std::string& user_name, const std::string& map_id) {
        if (user_name.empty()) {
            throw AppErrorException("User name is empty"s, AppErrorException::Category::EmptyPlayerName);
        }

        auto map = game_.FindMap(Map::Id{map_id});
        if (!map) {
            throw AppErrorException("Map not found"s, AppErrorException::Category::InvalidMapId);
        }
        
        auto game_session = game_.FindSession(map);
        if (!game_session) {
            game_session = game_.CreateSession(map);
        }
        auto dog = game_session->CreateDog(user_name, randomize_spawn_points_);
        auto player_info = players_.Add(dog, game_session);

        return json::object{
            {"authToken"sv, player_info.token}, 
            {"playerId"sv, player_info.player->GetId()
        }};
    }
    
    json::value GetGameState(const Players::Token& player_token) {
        auto player = players_.FindByToken(player_token);
        if (!player) {
            throw AppErrorException("No player with token"s, AppErrorException::Category::NoPlayerWithToken);
        }
        
        json::object players_by_id;
        for (const auto& dog : player->GetSession()->GetDogs()) {
            const auto bag_items = dog->GetBagItems();
            json::array bag_items_json;
            bag_items_json.reserve(bag_items.size());
            for (const auto& item : bag_items) {
                bag_items_json.emplace_back(json::object{
                    {"id"sv, item.id},
                    {"type"sv, item.type}
                });
            }
            players_by_id[std::to_string(dog->GetId())] = json::object{
                {"pos"sv, json::array{dog->GetPosition().x, dog->GetPosition().y}},
                {"speed"sv, json::array{dog->GetSpeed().x, dog->GetSpeed().y}},
                {"dir"sv, DirectionToString(dog->GetDirection())},
                {"bag"sv, bag_items_json}
            };
        }

        auto lost_objects = player->GetSession()->GetLostObjects();
        json::object lost_objects_by_id;
        for (size_t i = 0; i < lost_objects.size(); ++i) {
            lost_objects_by_id[std::to_string(i)] = json::object{
                {"type"sv, lost_objects.at(i).type},
                {"pos"sv, json::array{lost_objects.at(i).position.x, lost_objects.at(i).position.y}}
            };
        }

        return json::object{{"players"sv, players_by_id},
                            {"lostObjects"sv, lost_objects_by_id}};
    }

    void ActionPlayer(const Players::Token& player_token, const std::string& direction_str) {
        std::optional<Direction> direction;
        if (!direction_str.empty()) {
             try {
                direction = DirectionFromString(direction_str);
            } catch (const DirectionConvertException& e) { 
                throw AppErrorException("Failed to parse direction"s, AppErrorException::Category::InvalidDirection);
            }
        }

        auto player = players_.FindByToken(player_token);
        if (!player) {
            throw AppErrorException("No player with token"s, AppErrorException::Category::NoPlayerWithToken);
        }        

        if (!direction) {
            player->SetSpeed(Dog::Speed{0.0, 0.0});
        } else {
            player->ChangeDirection(*direction);
        }
    }

public:
    void Tick(std::chrono::milliseconds delta) {
        if (delta < 0ms) {
            throw AppErrorException("Whrong time"s, AppErrorException::Category::InvalidTime);
        }
        
        players_.MoveAllPlayers(delta);
        GenerateMapsLostObjects(delta);
    }

private:
    void GenerateMapsLostObjects(std::chrono::milliseconds delta) {
        for (const auto& map : game_.GetMaps()) {
            auto session = game_.FindSession(&map);
            if (!session) {
                continue;
            }

            unsigned new_lost_object_count = loot_generator_.Generate(delta, 
                                                                      session->GetLostObjects().size(),
                                                                      session->GetDogs().size());
            session->GenerateLostObjects(new_lost_object_count, 
                                         extra_data_.map_id_to_loot_types.at(*(map.GetId())).size());
        }
    }

private:
    Game game_;
    ExtraData extra_data_;
    Players players_;
    bool randomize_spawn_points_;
    bool auto_tick_enabled_;
    loot_gen::LootGenerator loot_generator_;
};

}  // namespace game_scenarios
