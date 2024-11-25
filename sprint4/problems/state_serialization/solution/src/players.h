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
using namespace model;

class Player {
public:
    using PlayerScore = size_t;

public:
    struct State {
        Dog::DogPosition position;
        bool stopped;
    };
    
public:
    Player(Dog* dog, GameSession* session) :
        dog_(dog),
        session_(session)
     {}

public:
    Dog::DogId GetId() const { return dog_->GetId(); }
    GameSession* GetSession() const noexcept { return session_; }
    Dog* GetDog() const noexcept { return dog_; }
    Dog::DogPosition GetPosition() const { return dog_->GetPosition(); }
    void SetPosition(const Dog::DogPosition& pos) { return dog_->SetPosition(pos); }
    std::vector<Dog::BagItem> GetBagItems() const { return dog_->GetBagItems(); }    
    void AddScore(PlayerScore score) { score_ += score; }
    PlayerScore GetScore() const { return score_; }
    size_t ClearBag() { return dog_->ClearBag(); }
    void SetSpeed(const Dog::DogSpeed& speed) { dog_->SetSpeed(speed); }

    bool AddItemInBag(size_t item_id, size_t item_type) { 
        return dog_->AddItemInBag(Dog::BagItem{item_id, item_type});    
    }

    void ChangeDirection(Direction direction) {
        DimensionD speed_value(session_->GetMap()->GetDefaultSpeed());
        Dog::DogSpeed speed;
        switch (direction)
        {
            case Direction::NORTH: speed = Dog::DogSpeed{0.0, -speed_value}; break;
            case Direction::SOUTH: speed = Dog::DogSpeed{0.0, speed_value}; break;
            case Direction::WEST: speed = Dog::DogSpeed{-speed_value, 0.0}; break;
            case Direction::EAST: speed = Dog::DogSpeed{speed_value, 0.0}; break;
        }
        dog_->SetDirection(direction);
        dog_->SetSpeed(speed);
    }

    void SetState(Player::State state) {
        dog_->SetPosition(state.position);
        if (state.stopped) {
            dog_->SetSpeed(Dog::DogSpeed{});
        }
    }
    
    Player::State GetNextState(std::chrono::milliseconds time_delta) const {
        auto speed = dog_->GetSpeed();
        if (speed.x == 0.0 && speed.y == 0.0) {
            return {dog_->GetPosition(), true};
        }

        auto time_delta_d = std::chrono::duration<DimensionD>(time_delta).count();
        auto current_pos = dog_->GetPosition();
        auto next_pos = Dog::DogPosition{current_pos.x + (speed.x * time_delta_d), 
                                         current_pos.y + (speed.y * time_delta_d)};
        const auto& roads = session_->GetMap()->GetRoads();
        
        // Есть ли дорога, которая содержит получившеюся позицию
        auto any_road_it = std::find_if(roads.begin(), roads.end(), [&next_pos](const Road& road){
            GameSession::RoadPosition min_road_pos{std::min(road.GetStart().x, road.GetEnd().x) - Road::HALF_WIDTH, 
                                                   std::min(road.GetStart().y, road.GetEnd().y) - Road::HALF_WIDTH};
            GameSession::RoadPosition max_road_pos{std::max(road.GetStart().x, road.GetEnd().x) + Road::HALF_WIDTH, 
                                                   std::max(road.GetStart().y, road.GetEnd().y) + Road::HALF_WIDTH};
            return (next_pos.x >= min_road_pos.x && next_pos.x <= max_road_pos.x
                    && next_pos.y >= min_road_pos.y && next_pos.y <= max_road_pos.y);
        });
        if (any_road_it != roads.end()) {
            return {next_pos, false};
        }

        // Нет дороги, которая содержала бы вычисленную позицию: ищем границу какой-то дороги взависимости от направления
        next_pos = current_pos;
        std::unordered_set<size_t> viewed_road_indeces;
        while (true) {
            int64_t roadIndex = FindRoadIndex(next_pos, viewed_road_indeces);
            if (roadIndex == -1) {
                break;
            }

            const auto& road = roads.at(roadIndex);
            switch (dog_->GetDirection())
            {
            case Direction::NORTH: {
                next_pos.y = std::min(road.GetStart().y, road.GetEnd().y);
                next_pos.y -= Road::HALF_WIDTH;
                break;
            }
            case Direction::SOUTH: {
                next_pos.y = std::max(road.GetStart().y, road.GetEnd().y);
                next_pos.y += Road::HALF_WIDTH;
                break;
            }
            case Direction::WEST: {
                next_pos.x = std::min(road.GetStart().x, road.GetEnd().x);
                next_pos.x -= Road::HALF_WIDTH;
                break;
            }
            case Direction::EAST: {
                next_pos.x = std::max(road.GetStart().x, road.GetEnd().x);
                next_pos.x += Road::HALF_WIDTH;
                break;
            }
            }
        }
        return {next_pos, true};
    }

private:
    int64_t FindRoadIndex(const GameSession::RoadPosition& pos, std::unordered_set<size_t>& viewed_road_indeces) const {
        const auto& roads = session_->GetMap()->GetRoads();
        for (size_t i = 0; i < roads.size(); ++i) {
            if (viewed_road_indeces.count(i)) {
                continue;
            }

            const auto& road = roads.at(i);
            GameSession::RoadPosition min_road_pos{std::min(road.GetStart().x, road.GetEnd().x) - Road::HALF_WIDTH, 
                                                   std::min(road.GetStart().y, road.GetEnd().y) - Road::HALF_WIDTH};
            GameSession::RoadPosition max_road_pos{std::max(road.GetStart().x, road.GetEnd().x) + Road::HALF_WIDTH, 
                                                   std::max(road.GetStart().y, road.GetEnd().y) + Road::HALF_WIDTH};
            if (pos.x >= min_road_pos.x && pos.x <= max_road_pos.x
                && pos.y >= min_road_pos.y && pos.y <= max_road_pos.y) {
                viewed_road_indeces.insert(i);
                return i;
            }
        }
        
        return -1;
    }

private:
    Dog* dog_;
    GameSession* session_;
    PlayerScore score_{0};
};

class Players {
public:
    using Token = std::string;
    using PlayersContainer = std::vector<std::unique_ptr<Player>>;
    using PlayerByToken = std::unordered_map<Token, Player*>;

public:
    Players() = default;
    
    Players(const Players&) = delete;
    Players& operator=(const Players&) = delete;

public:
    struct PlayerInfo {
        Player* player;
        Token token;
    };

public:
    PlayerInfo Add(Dog* dog, GameSession* session) {
        PlayerInfo player_info{
            players_.emplace_back(std::make_unique<Player>(dog, session)).get(),
            Players::GeneratePlayerToken()
        };
        player_by_token_[player_info.token] = player_info.player;
        return player_info;
    }
    PlayerInfo Add(Dog* dog, GameSession* session, Token token) {
        PlayerInfo player_info{
            players_.emplace_back(std::make_unique<Player>(dog, session)).get(),
            token
        };
        player_by_token_[player_info.token] = player_info.player;
        return player_info;
    }

    Player* FindByToken(const Token& token) {
        if (!player_by_token_.count(token)) {
            return nullptr;
        }
        return player_by_token_[token];
    }

    const PlayersContainer& GetPlayers() const noexcept { return players_; }
    const PlayerByToken& GetPlayerInfos() const noexcept { return player_by_token_; }
    Player::State CalcPlayerNextState(Player* player, std::chrono::milliseconds time_delta) { return player->GetNextState(time_delta); }

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
