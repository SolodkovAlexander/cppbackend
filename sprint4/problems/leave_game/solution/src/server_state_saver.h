#pragma once

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

#include "application.h"
#include "model.h"
#include "players.h"

using namespace std::literals;

namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar& point.x;
    ar& point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
    ar& vec.x;
    ar& vec.y;
}

}  // namespace geom

namespace model {

template <typename Archive>
void serialize(Archive& ar, PointD& position, [[maybe_unused]] const unsigned version) {
    ar& position.x;
    ar& position.y;
}

template <typename Archive>
void serialize(Archive& ar, GameSession::LostObject& lost_object, [[maybe_unused]] const unsigned version) {
    ar& lost_object.type;
    ar& lost_object.position;
}

template <typename Archive>
void serialize(Archive& ar, Dog::BagItem& bag_item, [[maybe_unused]] const unsigned version) {
    ar& bag_item.id;
    ar& bag_item.type;
}

}  // namespace model

namespace serialization {

// SERIALIZE: Dog
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(model::Dog* dog)
        : id_(dog->GetId())
        , name_(dog->GetName())
        , pos_(dog->GetPosition())
        , bag_capacity_(dog->GetBagCapacity())
        , speed_(dog->GetSpeed())
        , direction_(dog->GetDirection())
        , bag_items_(std::move(dog->GetBagItems())) {
    }

    model::Dog* Restore(model::GameSession* session) const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& pos_;
        ar& bag_capacity_;
        ar& speed_;
        ar& direction_;
        ar& bag_items_;
    }

private:
    model::Dog::DogId id_ = model::Dog::DogId{0u};
    std::string name_;
    model::Dog::Position pos_{};
    size_t bag_capacity_ = 0;
    model::Dog::Speed speed_{};
    model::Direction direction_ = model::Direction::NORTH;
    std::vector<model::Dog::BagItem> bag_items_;
};

// SERIALIZE: Player
class PlayerRepr {
public:
    PlayerRepr() = default;
    explicit PlayerRepr(players::Player* player, const players::Players::Token& token)
        : score_(player->GetScore())
        , token_(token) {
    }

public:
    void Restore(model::Dog* dog, model::GameSession* session, players::Players& players_engine) const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& score_;
        ar& token_;
    }

private:
    players::Player::Score score_{0};
    players::Players::Token token_;
};

// SERIALIZE: GameSession
class GameSessionRepr {
public:
    GameSessionRepr() = default;
    explicit GameSessionRepr(model::GameSession* session, game_scenarios::Application& app);

public:
    void Restore(game_scenarios::Application& app) const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& map_id_;
        ar& lost_objects_;
        ar& lost_object_type_count_;
        ar& dogs_;
        ar& players_;
    }

private:
    std::string map_id_;
    std::vector<model::GameSession::LostObject> lost_objects_;
    size_t lost_object_type_count_ = 0;
    std::vector<DogRepr> dogs_;
    std::map<model::Dog::DogId, PlayerRepr> players_;
};

}  // namespace serialization

namespace server_state_saver {

class ServerStateSaver {
public:
    explicit ServerStateSaver(game_scenarios::Application& app, const std::string& state_file, int save_state_period);

public:
    void SaveState(std::chrono::milliseconds delta);
    void SaveState();
    void RestoreState();

private:
    void SaveState(std::stringstream& file_stream);
    void RestoreState(std::stringstream& file_stream);

private:
    game_scenarios::Application& app_;
    std::string state_file_;
    std::string state_file_tmp_;
    std::optional<std::chrono::milliseconds> save_state_period_{std::nullopt};
    std::chrono::milliseconds time_before_save{0ms};
};

}  //namespace server_state_saver
