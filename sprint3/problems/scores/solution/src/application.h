#pragma once

#include "collision_detector.h"
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

struct LootInfo {
    size_t type;
    size_t value;
};

    ExtraData() = default;

    loot_gen::LootGenerator::TimeInterval base_interval{0ms};
    double probability{0.0};
    std::unordered_map<std::string, json::array> map_id_to_loot_types;
    std::unordered_map<std::string, std::unordered_map<size_t, size_t>> map_to_loot_type_score;
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
    json::value GetMapsShortInfo() const noexcept;
    json::value GetMapInfo(const std::string& map_id) const;
    json::value GetPlayers(const Players::Token& player_token);
    json::value JoinGame(const std::string& user_name, const std::string& map_id);    
    json::value GetGameState(const Players::Token& player_token);
    void ActionPlayer(const Players::Token& player_token, const std::string& direction_str);

public:
    bool GetAutoTick() const noexcept;
    void Tick(std::chrono::milliseconds delta);

private:
    void GenerateMapsLostObjects(std::chrono::milliseconds delta);

private:
    Game game_;
    ExtraData extra_data_;
    Players players_;
    bool randomize_spawn_points_;
    bool auto_tick_enabled_;
    loot_gen::LootGenerator loot_generator_;
};

}  // namespace game_scenarios
