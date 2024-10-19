#pragma once

#include "model.h"

#include <iomanip>
#include <optional>
#include <random>
#include <unordered_set>
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
    void ChangeDirection(std::optional<model::Dog::Direction> direction) {
        auto dog_speed = dog_->GetSpeed();
        if (!direction) {
            dog_speed = model::Dog::Speed{0.0, 0.0};
        } else {
            model::DimensionD speed_value(session_->GetMap()->GetDefaultSpeed());
            switch (*direction)
            {
            case model::Dog::Direction::NORTH: dog_speed = model::Dog::Speed{0.0, -speed_value}; break;
            case model::Dog::Direction::SOUTH: dog_speed = model::Dog::Speed{0.0, speed_value}; break;
            case model::Dog::Direction::WEST: dog_speed = model::Dog::Speed{-speed_value, 0.0}; break;
            case model::Dog::Direction::EAST: dog_speed = model::Dog::Speed{speed_value, 0.0}; break;
            }
            dog_->SetDirection(*direction);
        }        
        dog_->SetSpeed(dog_speed);
    }

    void Move(int time_ms) {
        auto speed = dog_->GetSpeed();
        if (speed.x == 0.0 && speed.y == 0.0) {
            return;
        }

        auto time_s_d = model::DimensionD(time_ms) * 0.001;
        auto current_pos = dog_->GetPosition();
        auto next_pos = model::PointD{current_pos.x + (speed.x * time_s_d), current_pos.y + (speed.y * time_s_d)};

        const auto& roads = session_->GetMap()->GetRoads();
        
        // Есть ли дорога, которая содержит получившеюся позицию
        auto any_road_it = std::find_if(roads.begin(), roads.end(), [&next_pos](const model::Road& road){ 
            if (road.IsHorizontal()) {
                return (next_pos.x >= road.GetStart().x 
                        && next_pos.x <= road.GetEnd().x
                        && next_pos.y >= model::DimensionD(road.GetStart().y) - 0.4
                        && next_pos.y <= model::DimensionD(road.GetStart().y) + 0.4);
            }
            return (next_pos.y >= road.GetStart().y
                    && next_pos.y <= road.GetEnd().y
                    && next_pos.x >= model::DimensionD(road.GetStart().x) - 0.4
                    && next_pos.x <= model::DimensionD(road.GetStart().x) + 0.4);
        });
        if (any_road_it != roads.end()) {
            dog_->SetPosition(next_pos);
            return;
        }

        // Нет дороги, которая содержала бы вычисленную позицию: ищем границу какой-то дороги взависимости от направления
        next_pos = current_pos;
        bool next_pos_on_road = true;
        std::unordered_set<size_t> viewed_road_indeces;
        while (next_pos_on_road) {
            int64_t roadIndex = FindRoadIndex(next_pos, viewed_road_indeces);
            if (roadIndex == -1) {
                break;
            }

            const auto& road = roads.at(roadIndex);
            switch (dog_->GetDirection())
            {
            case model::Dog::Direction::NORTH: {
                next_pos.y = std::min(road.GetStart().y, road.GetEnd().y);
                next_pos.y -= 0.4;
                break;
            }
            case model::Dog::Direction::SOUTH: {
                next_pos.y = std::max(road.GetStart().y, road.GetEnd().y);
                next_pos.y += 0.4;
                break;
            }
            case model::Dog::Direction::WEST: {
                next_pos.x = std::min(road.GetStart().x, road.GetEnd().x);
                next_pos.x -= 0.4;
                break;
            }
            case model::Dog::Direction::EAST: {
                next_pos.x = std::max(road.GetStart().x, road.GetEnd().x);
                next_pos.x += 0.4;
                break;
            }
            }
        }
        dog_->SetSpeed(model::Dog::Speed{0.0, 0.0});
        dog_->SetPosition(next_pos);
    }

private:
    int64_t FindRoadIndex(model::PointD pos, std::unordered_set<size_t>& viewed_road_indeces) {
        const auto& roads = session_->GetMap()->GetRoads();
        for (size_t i = 0; i < roads.size(); ++i) {
            if (viewed_road_indeces.count(i)) {
                continue;
            }

            const auto& road = roads.at(i);
            if ((road.IsHorizontal()
                 && pos.x >= road.GetStart().x 
                 && pos.x <= road.GetEnd().x
                 && pos.y >= model::DimensionD(road.GetStart().y) - 0.4
                 && pos.y <= model::DimensionD(road.GetStart().y) + 0.4)
                || (road.IsVertical()
                    && pos.y >= road.GetStart().y
                    && pos.y <= road.GetEnd().y
                    && pos.x >= model::DimensionD(road.GetStart().x) - 0.4
                    && pos.x <= model::DimensionD(road.GetStart().x) + 0.4)) {
                viewed_road_indeces.insert(i);
                return i;
            }
        }
        
        return -1;
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

    void MoveAllPlayers(int time_ms) {
        for (const auto& player : players_) {
            player->Move(time_ms);
        }
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
