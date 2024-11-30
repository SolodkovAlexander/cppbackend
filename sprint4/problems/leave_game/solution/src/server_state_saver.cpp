#include "server_state_saver.h"

namespace serialization {

model::Dog* DogRepr::Restore(model::GameSession* session) const {
    if (session->GetMap()->GetDefaultBagCapacity() != bag_capacity_) {
        throw std::invalid_argument("Invalid bag capacity for restore dog"s);
    }
    if (!session->GetMap()->CheckRoadPosition(pos_)) {
        throw std::invalid_argument("Invalid position for restore dog"s);
    }

    auto dog = session->CreateDog(id_, name_, pos_, speed_);
    dog->SetDirection(direction_);
    for (const auto& bag_item : bag_items_) {
        if (bag_item.type > session->GetLostObjectTypeCount()) {
            throw std::invalid_argument("Invalid bag item type for restore dog"s);
        }
        dog->AddItemInBag(bag_item);
    }

    return dog;
}

void PlayerRepr::Restore(model::Dog* dog, model::GameSession* session, players::Players& players_engine) const {
    players_engine.Add(dog, session, token_, score_);
}

GameSessionRepr::GameSessionRepr(model::GameSession* session, game_scenarios::Application& app)
    : map_id_(*session->GetMap()->GetId())
    , lost_objects_(session->GetLostObjects())
    , lost_object_type_count_(session->GetLostObjectTypeCount()) {
    // Получаем представления собак
    auto dogs = session->GetDogs();
    dogs_.reserve(session->GetDogs().size());
    for (const auto& dog : dogs) {
        dogs_.emplace_back(DogRepr(dog));
    }

    // Получаем представления игроков
    for (const auto& [token, player] : app.GetPlayersEngine().GetPlayerInfos()) {
        if (player->GetSession() != session) {
            continue;
        }
        players_[player->GetId()] = PlayerRepr(player, token);
    }
}

void GameSessionRepr::Restore(game_scenarios::Application& app) const {
    auto map = app.GetGameEngine().FindMap(model::Map::Id{map_id_});
    if (!map) {
        throw std::invalid_argument("No map for restore game session");
    }
    if (app.GetMapLostObjectTypeCount(map_id_) != lost_object_type_count_) {
        throw std::invalid_argument("Invalid lost object type count to map for restore game session");
    }

    // Создаем игровую сессию
    auto session = app.GetGameEngine().CreateSession(map, lost_object_type_count_);

    // Создаем потерянные объекты
    for (const auto& lost_object : lost_objects_) {
        if (!session->GetMap()->CheckRoadPosition(lost_object.position)) {
            throw std::invalid_argument("Invalid lost object position for restore game session"s);
        }
        if (lost_object.type > lost_object_type_count_) {
            throw std::invalid_argument("Invalid lost object type for restore game session"s);
        }
    }
    session->SetLostObjects(lost_objects_);

    // Создаем собак, игроков
    assert(dogs_.size() == players_.size());
    for (size_t i = 0; i < dogs_.size(); ++i) {
        // Создаем собаку
        auto dog = dogs_.at(i).Restore(session);
        // Создаем игрока
        players_.at(dog->GetId()).Restore(dog, session, app.GetPlayersEngine());
    }
}

}  // namespace serialization

namespace server_state_saver {

ServerStateSaver::ServerStateSaver(game_scenarios::Application& app, const std::string& state_file, int save_state_period)
    : app_(app)
    , state_file_(state_file) {
    if (save_state_period > 0) {
        save_state_period_ = std::chrono::milliseconds(save_state_period);
    }
    if (!state_file.empty()) {
        state_file_tmp_ = state_file + "_tmp.state"s;
    }
}

/** Сохраняет состояние с течением времени. */
void ServerStateSaver::SaveState(std::chrono::milliseconds delta) {
    if (state_file_.empty()
        || !save_state_period_) {
        return;
    }

    if (delta + time_before_save > save_state_period_) {
        SaveState();
        time_before_save = 0ms;
    }
}

/** Сохраняет состояние сервера. */
void ServerStateSaver::SaveState() {
    // Если не указан файл сохранения состояния: выход
    if (state_file_.empty()) {
        return;
    }

    // Сохраняем состояние в временный файл
    std::ofstream file(state_file_tmp_, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        throw std::invalid_argument("Failed to open tmp state file for save state");
    }

    std::stringstream file_stream;
    SaveState(file_stream);

    file << file_stream.str();
    file.close();

    // Переименовываем файл из временного в основной файл
    std::filesystem::rename(state_file_tmp_,
                            state_file_);
}

/** Восстнавливает состояние сервера. */
void ServerStateSaver::RestoreState() {
    if (state_file_.empty()
        || !std::filesystem::exists(state_file_)) {
        return;
    }

    std::ifstream file(state_file_);
    if (!file.is_open()) {
        throw std::invalid_argument("Failed to open state file for restore state");
    }
    std::stringstream file_stream;
    file_stream << file.rdbuf();

    RestoreState(file_stream);
}

void ServerStateSaver::SaveState(std::stringstream& file_stream) {
    const auto& sessions = app_.GetGameEngine().GetSessions();

    boost::archive::text_oarchive oa{file_stream};
    std::vector<serialization::GameSessionRepr> session_reprs;
    session_reprs.reserve(sessions.size());
    for (const auto& session : sessions) {
        session_reprs.emplace_back(serialization::GameSessionRepr(session.get(), app_));
    }
    oa << session_reprs;
}

void ServerStateSaver::RestoreState(std::stringstream& file_stream) {
    boost::archive::text_iarchive ia{file_stream};
    std::vector<serialization::GameSessionRepr> session_reprs;
    ia >> session_reprs;
    for (const auto& session_repr : session_reprs) {
        session_repr.Restore(app_);
    }
}

}  // namespace server_state_saver