#pragma once

#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

using DimensionD = double;
using CoordD = DimensionD;

struct PointD {
    CoordD x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    static constexpr HorizontalTag HORIZONTAL{};
    static constexpr VerticalTag VERTICAL{};
    static constexpr DimensionD HALF_WIDTH = 0.4;

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name, const model::DimensionD& default_speed) noexcept
        : id_(std::move(id))
        , name_(std::move(name))
        , default_speed_(default_speed) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    DimensionD GetDefaultSpeed() const noexcept {
        return default_speed_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    DimensionD default_speed_;
};

class DirectionConvertException : public std::invalid_argument {
public: 
    using std::invalid_argument::invalid_argument;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};
std::string DirectionToString(Direction direction) noexcept;
Direction DirectionFromString(const std::string& direction);

class Dog {
public:
    struct Speed {
        DimensionD x, y;
    };

private:
    static constexpr PointD DEFAULT_POSITION = PointD{0.0, 0.0};
    static constexpr Speed DEFAULT_SPEED = Speed{0.0, 0.0};

public:
    using DogId = std::uint64_t;

    Dog(const std::string& name, DogId id, PointD position = DEFAULT_POSITION, Speed speed = DEFAULT_SPEED) 
        : name_(name)
        , id_(id)
        , position_(position)
        , speed_(speed)
    {}

public:
    DogId GetId() const noexcept {
        return id_;
    }
    PointD GetPosition() const noexcept {
        return position_;
    }
    void SetPosition(PointD position) {
        position_ = position;
    }
    Speed GetSpeed() const noexcept {
        return speed_;
    }
    void SetSpeed(Speed speed) {
        speed_ = speed;
    }
    Direction GetDirection() const noexcept {
        return direction_;
    }
    void SetDirection(Direction direction) {
        direction_ = direction;
    }
    const std::string& GetName() const noexcept {
        return name_;
    }

private:
    std::string name_;
    DogId id_;
    Direction direction_ = Direction::NORTH;
    PointD position_{0.0, 0.0};
    Speed speed_{0.0, 0.0};
};

class GameSession {
private:
    using Dogs = std::vector<std::unique_ptr<Dog>>;
    using DogIdToDog = std::unordered_map<std::uint64_t, Dog*>;

public:
    explicit GameSession(const Map* map) : 
        map_(map) 
    {}

    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

public:
    Dog* CreateDog(const std::string& name, bool randomize_spawn_point = false) {
        auto dog = dogs_.emplace_back(std::make_unique<Dog>(name, 
                                                            dogs_.size(), 
                                                            GenerateRoadPosition(randomize_spawn_point))).get();
        dog_id_to_dog_[dog->GetId()] = dog;
        return dog;
    }

    std::vector<Dog*> GetDogs() {
        std::vector<Dog*> dogs;
        dogs.reserve(dogs_.size());
        for (const auto& dog : dogs_) {
            dogs.emplace_back(dog.get());
        }
        return dogs;
    }

    const Map* GetMap() const {
        return map_;
    }

public:
struct LostObject {
    size_t type = 0;
    PointD position{0.0, 0.0};
};

const std::vector<LostObject>& GetLostObjects() const noexcept {
    return lost_objects_;
}

void GenerateLostObjects(unsigned lost_object_count, size_t lost_object_types) {
    if (lost_object_types == 0) {
        return;
    }

    std::random_device rand_device; 
    std::mt19937_64 rand_engine(rand_device());
    std::uniform_int_distribution<size_t> unif(0, lost_object_types - 1);

    for (unsigned i = 0; i < lost_object_count; ++i) {
        lost_objects_.push_back(LostObject{ unif(rand_engine), GenerateRoadPosition(true) });
    }
}

private:
    PointD GenerateRoadPosition(bool randomize = false) const noexcept {
        if (!randomize) {
            return PointD{CoordD(map_->GetRoads().at(0).GetStart().x), 
                          CoordD(map_->GetRoads().at(0).GetStart().y)};
        }

        std::random_device rand_device; 
        std::mt19937_64 rand_engine(rand_device());

        // Определяем дорогу
        std::uniform_int_distribution<std::mt19937_64::result_type> unif(0, map_->GetRoads().size() - 1);
        const auto& road = map_->GetRoads().at(unif(rand_engine));
        auto r_start = road.GetStart();
        auto r_end = road.GetEnd();
        
        // Определяем позицию на дороге
        PointD pos{0.0, 0.0}; 
        if (std::abs(r_start.x - r_end.x) > std::abs(r_start.y - r_end.y)) {
            std::uniform_real_distribution<double> unif_d(std::min(r_start.x, r_end.x), std::max(r_start.x, r_end.x));
            pos.x = unif_d(rand_engine);
            pos.y = (((pos.x - DimensionD(r_start.x)) * DimensionD(r_end.y - r_start.y)) / DimensionD(r_end.x - r_start.x)) + DimensionD(r_start.y);
        } else {
            std::uniform_real_distribution<double> unif_d(std::min(r_start.y, r_end.y), std::max(r_start.y, r_end.y));
            pos.y = unif_d(rand_engine);
            pos.x = (((pos.y - DimensionD(r_start.y)) * DimensionD(r_end.x - r_start.x)) / DimensionD(r_end.y - r_start.y)) + DimensionD(r_start.x);
        }
        return pos;
    }

private:
    Dogs dogs_;
    DogIdToDog dog_id_to_dog_;
    const Map* map_;
    std::vector<LostObject> lost_objects_;
};

class Game {
public:
    static constexpr DimensionD DEFAULT_SPEED = 1.0;

    Game(DimensionD map_default_speed = DEFAULT_SPEED)
         : map_default_speed_{map_default_speed}
    {}

public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

public:
    DimensionD GetMapDefaultSpeed() const noexcept {
        return map_default_speed_;
    }

    GameSession* CreateSession(const Map* map) {
        return sessions_.emplace_back(std::make_unique<GameSession>(map)).get();
    }
    GameSession* FindSession(const Map* map) const {
        auto it = std::find_if(sessions_.begin(), sessions_.end(), [map](const auto& session){ return session->GetMap() == map; });
        return (it != sessions_.end() ? it->get() : nullptr);
    }

private:
    DimensionD map_default_speed_;

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;

private:
    using Sessions = std::vector<std::unique_ptr<GameSession>>;

    Sessions sessions_;
};

}  // namespace model
