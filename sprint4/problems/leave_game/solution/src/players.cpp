#include "players.h"

#include <algorithm>

namespace players {
using namespace model;

void Player::SetSpeed(const Dog::Speed& speed, std::chrono::milliseconds duration_time) { 
    // Если игрок двигался
    if (dog_->GetSpeed() != Dog::Speed{}) {
        // Игрок двигался: следовательно жил на протяжении времени duration_time
        //live_duration_ms_ += duration_time;

        // Если игрок только остановился: начинаем отсчет времени стоянки
        if (speed == Dog::Speed{}) {
            stop_duration_ms_ = 0ms;
        }
    } else {
        // Если игрок уже стоял
        // Если игрок начал двигаться: сбрасываем счетчик времени стоянки
        if (speed != Dog::Speed{}) {
            stop_duration_ms_ = std::nullopt;
        } else {
            // Если игрок продолжил стоять: добавляем время стоянки
            stop_duration_ms_ = *stop_duration_ms_ + duration_time;
        }
    }

    dog_->SetSpeed(speed);
}

void Player::ChangeDirection(Direction direction) {
    DimensionD speed_value(session_->GetMap()->GetDefaultSpeed());
    Dog::Speed speed{};
    switch (direction)
    {
        case Direction::NORTH: speed = {0.0, -speed_value}; break;
        case Direction::SOUTH: speed = {0.0, speed_value}; break;
        case Direction::WEST: speed = {-speed_value, 0.0}; break;
        case Direction::EAST: speed = {speed_value, 0.0}; break;
    }
    dog_->SetDirection(direction);

    SetSpeed(speed);
}

void Player::SetState(Player::State state, std::chrono::milliseconds duration_time) {
    dog_->SetPosition(state.position);
    SetSpeed({}, duration_time);
}

Player::State Player::GetNextState(std::chrono::milliseconds time_delta) {
    auto speed = dog_->GetSpeed();
    if (speed.x == 0.0 && speed.y == 0.0) {
        return {dog_->GetPosition(), true};
    }

    auto time_delta_d = std::chrono::duration<DimensionD>(time_delta).count();
    auto current_pos = dog_->GetPosition();
    Dog::Position next_pos{current_pos.x + (speed.x * time_delta_d), 
                            current_pos.y + (speed.y * time_delta_d)};
    const auto& roads = session_->GetMap()->GetRoads();
    
    // Есть ли дорога, которая содержит получившеюся позицию
    auto any_road_it = std::find_if(roads.begin(), roads.end(), [&next_pos](const Road& road){
        PointD min_road_pos{std::min(road.GetStart().x, road.GetEnd().x) - Road::HALF_WIDTH, 
                            std::min(road.GetStart().y, road.GetEnd().y) - Road::HALF_WIDTH};
        PointD max_road_pos{std::max(road.GetStart().x, road.GetEnd().x) + Road::HALF_WIDTH, 
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

int64_t Player::FindRoadIndex(Road::Position pos, std::unordered_set<size_t>& viewed_road_indeces) {
    const auto& roads = session_->GetMap()->GetRoads();
    for (size_t i = 0; i < roads.size(); ++i) {
        if (viewed_road_indeces.count(i)) {
            continue;
        }

        const auto& road = roads.at(i);
        Road::Position min_road_pos{std::min(road.GetStartPos().x, road.GetEndPos().x) - Road::HALF_WIDTH, 
                                    std::min(road.GetStartPos().y, road.GetEndPos().y) - Road::HALF_WIDTH};
        Road::Position max_road_pos{std::max(road.GetStartPos().x, road.GetEndPos().x) + Road::HALF_WIDTH, 
                                    std::max(road.GetStartPos().y, road.GetEndPos().y) + Road::HALF_WIDTH};
        if (pos.x >= min_road_pos.x && pos.x <= max_road_pos.x
            && pos.y >= min_road_pos.y && pos.y <= max_road_pos.y) {
            viewed_road_indeces.insert(i);
            return i;
        }
    }
    
    return -1;
}

Players::PlayerInfo Players::Add(Dog* dog, GameSession* session) {
    return Add(dog, session, Players::GeneratePlayerToken(), 0);
}

Players::PlayerInfo Players::Add(Dog* dog, GameSession* session, const Token& token, Player::Score score) {
    Players::PlayerInfo player_info{
        players_.emplace_back(std::make_unique<Player>(dog, session, score)).get(),
        token
    };
    player_by_token_[player_info.token] = player_info.player;
    return player_info;
}

Player* Players::FindByToken(const Token& token) {
    if (!player_by_token_.count(token)) {
        return nullptr;
    }
    return player_by_token_[token];
}

std::vector<Players::RetiredPlayerInfo> Players::CheckAndRemoveRetiredPlayers(std::chrono::milliseconds duration_time,
                                                                              std::chrono::milliseconds player_retirement_time_ms) {
    std::vector<RetiredPlayerInfo> retired_players;
    auto player_it = players_.begin();
    while (player_it != players_.end()) {
        // Если игрок стоит на месте больше времени, чем может выдержать его здоровье: игрок умирает
        if ((*player_it)->GetSpeed() == Dog::Speed{} 
            && (*player_it)->GetStopDuration() + duration_time >= player_retirement_time_ms) {
            // Получаем информацию об игроке
            retired_players.emplace_back(Players::RetiredPlayerInfo{(*player_it)->GetName(), 
                                                                    (*player_it)->GetScore(),
                                                                    (*player_it)->GetLiveDuration() + player_retirement_time_ms});
            // Убиваем собаку
            (*player_it)->GetSession()->RemoveDog((*player_it)->GetDog());

            // Забываем токен
            Token player_token;
            for (const auto& [token, other_player] : player_by_token_) {
                if (other_player == (*player_it).get()) {
                    player_token = token;
                    break;
                }
            }
            player_by_token_.erase(player_token);

            // Убиваем игрока
            player_it = players_.erase(player_it);
        } else {
            ++player_it;
        }
    }
    return retired_players;
}

Players::Token Players::GeneratePlayerToken() {
    static constexpr auto num_size = sizeof(std::mt19937_64::result_type)*2UL;

    static std::stringstream stream;
    stream << std::setfill('0') << std::setw(num_size) << std::hex << generator1_()
           << std::setfill('0') << std::setw(num_size) << std::hex << generator2_();
    auto token = stream.str();
    
    stream.str(std::string());
    stream.clear();

    return token;
}

} // namespace players
