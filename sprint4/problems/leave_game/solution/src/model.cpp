#include "model.h"

#include <stdexcept>

namespace model {
using namespace std::literals;

std::vector<Dog::BagItem> Dog::GetBagItems() const noexcept {
    std::vector<Dog::BagItem> items;
    items.reserve(bag_.size());
    for (const auto& item : bag_) {
        if (item) {
            items.emplace_back(*item);
        }
    }
    return items;
}

bool Dog::AddItemInBag(Dog::BagItem item) {
    auto empty_place_it = std::find(bag_.begin(), bag_.end(), std::nullopt);
    if (empty_place_it == bag_.end()) {
        return false;
    }
    *(*empty_place_it) = item;
    return true;
}

size_t Dog::ClearBag() {
    size_t item_count = bag_.size() - std::count(bag_.begin(), bag_.end(), std::nullopt);
    bag_ = std::vector<std::optional<Dog::BagItem>>{bag_.size(), std::nullopt};
    return item_count;
}

Dog* GameSession::CreateDog(const std::string& name, bool randomize_spawn_point) {
    auto dog = dogs_.emplace_back(std::make_unique<Dog>(name, 
                                                        dogs_.size(), 
                                                        GenerateRoadPosition(randomize_spawn_point),
                                                        Dog::Speed{},
                                                        map_->GetDefaultBagCapacity())).get();
    dog_id_to_dog_[dog->GetId()] = dog;
    return dog;
}

Dog* GameSession::CreateDog(Dog::DogId id, const std::string& name, const Dog::Position& pos, const Dog::Speed& speed) {
    auto dog = dogs_.emplace_back(std::make_unique<Dog>(name, 
                                                        id,
                                                        pos,
                                                        speed,
                                                        map_->GetDefaultBagCapacity())).get();
    dog_id_to_dog_[dog->GetId()] = dog;
    return dog;
}

std::vector<Dog*> GameSession::GetDogs() const {
    std::vector<Dog*> dogs;
    dogs.reserve(dogs_.size());
    for (const auto& dog : dogs_) {
        dogs.emplace_back(dog.get());
    }
    return dogs;
}

void GameSession::RemoveDog(Dog* dog) {
    dog_id_to_dog_.erase(dog->GetId());
    auto dog_it = std::find_if(dogs_.begin(), dogs_.end(), [dog](const std::unique_ptr<Dog>& item){ return item.get() == dog; });
    dogs_.erase(dog_it);
}

void GameSession::GenerateLostObjects(unsigned lost_object_count) {
    if (lost_object_type_count_ == 0) {
        return;
    }

    std::random_device rand_device; 
    std::mt19937_64 rand_engine(rand_device());
    std::uniform_int_distribution<size_t> unif(0, lost_object_type_count_ - 1);

    for (unsigned i = 0; i < lost_object_count; ++i) {
        lost_objects_.push_back({ unif(rand_engine), GenerateRoadPosition(true) });
    }
}

Road::Position GameSession::GenerateRoadPosition(bool randomize) const noexcept {
    if (!randomize) {
        return map_->GetRoads().at(0).GetStartPos();
    }

    std::random_device rand_device; 
    std::mt19937_64 rand_engine(rand_device());

    // Определяем дорогу
    std::uniform_int_distribution<std::mt19937_64::result_type> unif(0, map_->GetRoads().size() - 1);
    const auto& road = map_->GetRoads().at(unif(rand_engine));
    auto r_start = road.GetStartPos();
    auto r_end = road.GetEndPos();
    
    // Определяем позицию на дороге
    Road::Position pos{};
    if (std::abs(r_start.x - r_end.x) > std::abs(r_start.y - r_end.y)) {
        std::uniform_real_distribution<double> unif_d(std::min(r_start.x, r_end.x), std::max(r_start.x, r_end.x));
        pos.x = unif_d(rand_engine);
        pos.y = (((pos.x - r_start.x) * (r_end.y - r_start.y)) / (r_end.x - r_start.x)) + (r_start.y);
    } else {
        std::uniform_real_distribution<double> unif_d(std::min(r_start.y, r_end.y), std::max(r_start.y, r_end.y));
        pos.y = unif_d(rand_engine);
        pos.x = (((pos.y - (r_start.y)) * (r_end.x - r_start.x)) / (r_end.y - r_start.y)) + (r_start.x);
    }
    return pos;
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (const std::exception&) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (const std::exception&) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

std::string DirectionToString(Direction direction) noexcept {
    using namespace std::literals;
    static const auto info = std::unordered_map<Direction,std::string>{
        {Direction::NORTH, "U"s},
        {Direction::SOUTH, "D"s},
        {Direction::WEST, "L"s},
        {Direction::EAST, "R"s}
    };
    return info.at(direction);
}

Direction DirectionFromString(const std::string& direction) {
    using namespace std::literals;
    static const auto info = std::unordered_map<std::string,Direction>{
        {"U"s, Direction::NORTH},
        {"D"s, Direction::SOUTH},
        {"L"s, Direction::WEST},
        {"R"s, Direction::EAST}
    };
    if (!info.count(direction)) {
        throw DirectionConvertException("No direction with string"s);
    }
    return info.at(direction);
}

}  // namespace model
