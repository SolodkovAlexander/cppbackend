#pragma once

#include "model.h"

#include <chrono>
#include <iomanip>
#include <random>
#include <unordered_set>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace players {
using namespace model;

class Player {
public:
    struct State {
        Dog::Position position{};
        bool stopped;
    };
    
public:
    Player(Dog* dog, GameSession* session)
        : dog_(dog)
        , session_(session)
     {}

public:
    Dog::DogId GetId() const { return dog_->GetId(); }
    GameSession* GetSession() const noexcept { return session_; }
    Dog::Position GetPosition() const noexcept { return dog_->GetPosition(); }
    std::vector<Dog::BagItem> GetBagItems() const noexcept { return dog_->GetBagItems(); }    

    size_t GetScore() const noexcept { return score_; }
    void AddScore(size_t score) { score_ += score; }

    size_t ClearBag() { return dog_->ClearBag(); }
    bool AddItemInBag(size_t item_id, size_t item_type) { return dog_->AddItemInBag(Dog::BagItem{item_id, item_type}); }
    
    void SetSpeed(const Dog::Speed& speed) { dog_->SetSpeed(speed); }

    void ChangeDirection(Direction direction);

    void SetState(Player::State state);    
    Player::State GetNextState(std::chrono::milliseconds time_delta);

private:
    int64_t FindRoadIndex(Road::Position pos, std::unordered_set<size_t>& viewed_road_indeces);

private:
    Dog* dog_;
    GameSession* session_;
    size_t score_{0};
};

class Players {
public:
    using PlayersContainer = std::vector<std::unique_ptr<Player>>;
    using Token = std::string;

    struct PlayerInfo {
        Player* player;
        Token token;
    };

public:
    Players() = default;
    
    Players(const Players&) = delete;
    Players& operator=(const Players&) = delete;

public:
    PlayerInfo Add(Dog* dog, GameSession* session);

    Player* FindByToken(const Token& token);

    const PlayersContainer& GetPlayers() const noexcept { return players_; }

    Player::State CalcPlayerNextState(Player* player, std::chrono::milliseconds time_delta) { return player->GetNextState(time_delta); }

private:
    Token GeneratePlayerToken();

private:
    using PlayerByToken = std::unordered_map<Token, Player*>;

    PlayersContainer players_;
    PlayerByToken player_by_token_;

private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
};

}  // namespace players
