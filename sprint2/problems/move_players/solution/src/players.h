#pragma once

#include "model.h"

#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace players {

class Player {
public:
    Player(model::Dog* dog, model::GameSession* session) :
        dog_(dog),
        session_(session)
     {}

public:
    model::Dog::DogId GetId() const {
        return dog_->GetId();
    }
    model::GameSession* GetSession() const {
        return session_;
    }
    void Move(std::optional<model::Dog::Direction> direction) {
        auto dog_speed = dog_->GetSpeed();
        if (!direction) {
            dog_speed = model::Dog::Speed{0.0, 0.0};
        } else {
            model::DimensionD speed_value(std::abs(std::max(dog_speed.x, dog_speed.y)));
            switch (*direction)
            {
            case model::Dog::Direction::NORTH: dog_speed = model::Dog::Speed{0.0, -speed_value}; break;
            case model::Dog::Direction::SOUTH: dog_speed = model::Dog::Speed{0.0, speed_value}; break;
            case model::Dog::Direction::WEST: dog_speed = model::Dog::Speed{-speed_value, 0.0}; break;
            case model::Dog::Direction::EAST: dog_speed = model::Dog::Speed{speed_value, 0.0}; break;
            }
        }        
        dog_->SetSpeed(dog_speed);
    }

private:
    model::GameSession* session_;
    model::Dog* dog_;
};

class Players {
public:
    Players() = default;
    
    Players(const Players&) = delete;
    Players& operator=(const Players&) = delete;

public:
    using Token = std::string;

    struct PlayerInfo {
        Player* player;
        Token token;
    };

public:
    PlayerInfo Add(model::Dog* dog, model::GameSession* session) {
        PlayerInfo player_info{
            players_.emplace_back(std::make_unique<Player>(dog, session)).get(),
            Players::GeneratePlayerToken()
        };
        player_by_token_[player_info.token] = player_info.player;
        return player_info;
    }

    Player* FindByDogIdAndMapId(uint64_t dog_id, model::Map::Id map_id) {
        return nullptr;
    }

    Player* FindByToken(const Token& token) {
        if (!player_by_token_.count(token)) {
            return nullptr;
        }
        return player_by_token_[token];
    }

private:
    Token GeneratePlayerToken() {
        static constexpr auto num_size = sizeof(std::mt19937_64::result_type)*2UL;

        static std::stringstream stream;
        stream << std::setfill('0') << std::setw(num_size) << std::hex << generator1_()
               << std::setfill('0') << std::setw(num_size) << std::hex << generator2_();
        auto token = stream.str();
        
        stream.str(std::string());
        stream.clear();

        return token;
    }

private:
    using PlayersContainer = std::vector<std::unique_ptr<Player>>;
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
