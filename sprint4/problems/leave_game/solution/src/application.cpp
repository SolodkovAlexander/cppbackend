#include "application.h"

namespace game_scenarios {

json::value Application::GetMapsShortInfo() const noexcept {
    return json_parser::MapsToShortJson(game_.GetMaps());
}

json::value Application::GetMapInfo(const std::string& map_id) const {
    auto map = game_.FindMap(model::Map::Id{map_id});
    if (!map) {
        throw AppErrorException("Map not found"s, AppErrorException::Category::InvalidMapId);
    }
    return json_parser::MapToJson(map, extra_data_);
}

json::value Application::GetPlayers(const Players::Token& player_token) {
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

json::value Application::JoinGame(const std::string& user_name, const std::string& map_id) {
    if (user_name.empty()) {
        throw AppErrorException("User name is empty"s, AppErrorException::Category::EmptyPlayerName);
    }

    auto map = game_.FindMap(Map::Id{map_id});
    if (!map) {
        throw AppErrorException("Map not found"s, AppErrorException::Category::InvalidMapId);
    }
    
    auto session = game_.FindSession(map);
    if (!session) {
        session = game_.CreateSession(map, GetMapLostObjectTypeCount(*(map->GetId())));
    }
    auto dog = session->CreateDog(user_name, randomize_spawn_points_);
    auto player_info = players_.Add(dog, session);

    return json::object{
        {"authToken"sv, player_info.token}, 
        {"playerId"sv, player_info.player->GetId()
    }};
}

json::value Application::GetGameState(const Players::Token& player_token) {
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
            {"bag"sv, bag_items_json},
            {"score"sv, player->GetScore()}
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

json::value Application::GetRecords(std::optional<int> start, std::optional<int> max_items) {
    if (!start) {
        start = 0;
    }
    if (!max_items) {
        max_items = 100;
    }
    if (*start < 0) {
        throw AppErrorException("Invalid start"s, AppErrorException::Category::InvalidStart);
    }
    if (*max_items < 0 || *max_items > 100) {
        throw AppErrorException("Invalid max items"s, AppErrorException::Category::InvalidMaxItems);
    }

    auto players_score = db_.GetPlayersScore(*start, *max_items);
    json::array players_score_json;
    for (const auto& player_score : players_score) {
        players_score_json.emplace_back(json::object{
            {"name"sv, player_score.name},
            {"score"sv, player_score.score},
            {"playTime"sv, std::chrono::duration_cast<std::chrono::seconds>(player_score.play_time_ms).count()}
        });
    }

    return players_score_json;
}

void Application::ActionPlayer(const Players::Token& player_token, const std::string& direction_str) {
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
        player->SetSpeed({0.0, 0.0});
    } else {
        player->ChangeDirection(*direction);
    }
}

bool Application::GetAutoTick() const noexcept {
    return auto_tick_enabled_;
}

void Application::Tick(std::chrono::milliseconds delta) {
    if (delta < 0ms) {
        throw AppErrorException("Whrong time"s, AppErrorException::Category::InvalidTime);
    }

    // Получаем игроков
    const auto& players = players_.GetPlayers();
    
    // Формируем игроков по сессиям
    std::unordered_map<GameSession*, std::vector<Player*>> session_players;
    for (const auto& player : players) {
        session_players[player->GetSession()].emplace_back(player.get());
    }

    // Доп. данные об игроках для формирования событий в игре
    static const double player_width = 0.6;
    static const double item_width = 0.0;
    static const double base_width = 0.5;

    // Идем по сессиям
    for (const auto& [session, players] : session_players) {
        size_t office_count = session->GetMap()->GetOffices().size();
        
        // Формируем информацию о базах и информацию о луте
        std::vector<collision_detector::Item> items;
        items.reserve(office_count + session->GetLostObjects().size());
        for (const auto& office : session->GetMap()->GetOffices()) {
            items.emplace_back(collision_detector::Item{{static_cast<double>(office.GetPosition().x), 
                                                         static_cast<double>(office.GetPosition().y)}, 
                                                         base_width});
        }
        auto lost_objects = session->GetLostObjects();
        for (const auto& lost_object : lost_objects) {
            items.emplace_back(collision_detector::Item{{lost_object.position.x, lost_object.position.y}, 
                                                         item_width});
        }

        // Формируем информацию об игроках
        std::vector<Player::State> player_next_states;
        player_next_states.reserve(players.size());
        std::vector<collision_detector::Gatherer> gatherers;
        gatherers.reserve(players.size());
        for (auto player : players) {
            auto player_next_state = player_next_states.emplace_back(players_.CalcPlayerNextState(player, delta));
            gatherers.emplace_back(collision_detector::Gatherer{{player->GetPosition().x, player->GetPosition().y},
                                                                {player_next_state.position.x, player_next_state.position.y},
                                                                player_width});
        }

        // Получаем события
        auto events = collision_detector::FindGatherEvents(collision_detector::Provider(gatherers, items));

        // Определяем информацию о луте на карте (для определения очков)
        auto map_loot_types = extra_data_.map_id_to_loot_types.at(*(session->GetMap()->GetId()));

        // Разбираем события получения предметов/посещения базы
        std::unordered_set<size_t> lost_objects_taken;
        for (const auto& event : events) {
            auto player = players.at(event.gatherer_id);

            // Определяем: предмет или база получены
            bool is_office_event{event.item_id < office_count};
            if (is_office_event) {
                // Подсчитываем очки за лут
                for (const auto& item : player->GetBagItems()) {
                    player->AddScore(map_loot_types.at(item.type).as_object().at("value"sv).as_int64());
                }

                // Очищаем сумку
                player->ClearBag();
                continue;
            }

            size_t lost_object_index = event.item_id - office_count;

            // Если по предмету уже прошлись: пропускаем событие
            if (lost_objects_taken.count(lost_object_index)) {
                continue;
            }

            // Если предмет смогли упаковать в рюкзак
            if (player->AddItemInBag(lost_object_index, lost_objects.at(lost_object_index).type)) {
                lost_objects_taken.insert(lost_object_index);
            }
        }

        // Удаляем полученные игроками предметы из списка потерянных
        for (size_t lost_object_index : lost_objects_taken) {
            session->RemoveLostObject(lost_object_index);
        }

        // Перемещаем и возможно останавливаем игроков
        for (size_t i = 0; i < players.size(); ++i) {
            players.at(i)->SetPosition(player_next_states.at(i).position);
            if (player_next_states.at(i).stopped) {
                players.at(i)->SetSpeed({});
            }
        }
    }
    
    // Генерируем новый лут
    GenerateMapsLostObjects(delta);

    // Уведомляем подписчиков сигнала tick
    tick_signal_(delta);
}

size_t Application::GetMapLostObjectTypeCount(const std::string& map_id) const {
    auto map = game_.FindMap(Map::Id{map_id});
    if (!map) {
        throw AppErrorException("Map not found"s, AppErrorException::Category::InvalidMapId);
    }
    return extra_data_.map_id_to_loot_types.at(*(map->GetId())).size();
}

void Application::GenerateMapsLostObjects(std::chrono::milliseconds delta)
{
  for (const auto &map : game_.GetMaps()) {
    auto session = game_.FindSession(&map);
    if (!session) {
        continue;
    }

    unsigned new_lost_object_count = loot_generator_.Generate(delta,
                                                              session->GetLostObjects().size(),
                                                              session->GetDogs().size());
    session->GenerateLostObjects(new_lost_object_count);
  }
}

} // namespace game_scenarios