#pragma once

#include <boost/json.hpp>
#include <boost/signals2.hpp>
#include <chrono>
#include <string>
#include <unordered_map>

#include "collision_detector.h"
#include "database_controller.h"
#include "json_parser.h"
#include "loot_generator.h"
#include "model.h"
#include "players.h"

namespace game_scenarios {
using namespace model;
using namespace players;

namespace json = boost::json;
namespace sig = boost::signals2;
using namespace std::literals;
 
class AppErrorException : public std::invalid_argument { 
public: 
    using std::invalid_argument::invalid_argument;

    enum class Category { 
        EmptyPlayerName,
        NoPlayerWithToken,
        InvalidMapId,
        InvalidDirection,
        InvalidTime,
        InvalidStart,
        InvalidMaxItems
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
    std::chrono::milliseconds player_retirement_time_ms{60s};
};

struct AppConfig {
    bool randomize_spawn_points = false; 
    bool auto_tick_enabled = false;
    std::string db_url;
};

class Application {
public:
    Application(Game&& game, ExtraData&& extra_data, const AppConfig& config = {}) 
        : game_(std::move(game))
        , extra_data_(std::move(extra_data))
        , randomize_spawn_points_(config.randomize_spawn_points)
        , auto_tick_enabled_(config.auto_tick_enabled)
        , loot_generator_(loot_gen::LootGenerator(extra_data.base_interval, extra_data.probability))
        , db_{config.db_url} {
        db_.Prepare();
    }

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

public:
    Game& GetGameEngine() noexcept { return game_; }
    Players& GetPlayersEngine() noexcept { return players_; }
    json::value GetMapsShortInfo() const noexcept;
    json::value GetMapInfo(const std::string& map_id) const;
    json::value GetPlayers(const Players::Token& player_token);
    json::value JoinGame(const std::string& user_name, const std::string& map_id);
    json::value GetGameState(const Players::Token& player_token);
    json::value GetRecords(std::optional<int> start, std::optional<int> max_items);
    void ActionPlayer(const Players::Token& player_token, const std::string& direction_str);

public:
    bool GetAutoTick() const noexcept;
    void Tick(std::chrono::milliseconds delta);

    // Добавляем обработчик сигнала tick и возвращаем объект connection для управления,
    // при помощи которого можно отписаться от сигнала
    using TickSignal = sig::signal<void(std::chrono::milliseconds delta)>;
    [[nodiscard]] sig::connection DoOnTick(const TickSignal::slot_type& handler) { return tick_signal_.connect(handler); }

public:
    size_t GetMapLostObjectTypeCount(const std::string& map_id) const;

private:
    void GenerateMapsLostObjects(std::chrono::milliseconds delta);

    template <typename PlayerInfo>
    void RememberRetiredPlayers(const std::vector<PlayerInfo>& player_infos) {
        for (const auto& player_info : player_infos) {
            db_.AddPlayerScore(player_info);
        }
    }

private:
    Game game_;
    ExtraData extra_data_;
    Players players_;
    bool randomize_spawn_points_;
    bool auto_tick_enabled_;
    loot_gen::LootGenerator loot_generator_;
    database_control::Database db_;
    TickSignal tick_signal_;
};

}  // namespace game_scenarios
