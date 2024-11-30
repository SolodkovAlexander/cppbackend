#pragma once

#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "geom.h"
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
public:
    using Position = geom::Point2D;

private:
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

    bool IsHorizontal() const noexcept { return start_.y == end_.y; }
    bool IsVertical() const noexcept { return start_.x == end_.x; }

    Point GetStart() const noexcept { return start_; }
    Position GetStartPos() const noexcept { return {static_cast<CoordD>(start_.x), static_cast<CoordD>(start_.y)}; }

    Point GetEnd() const noexcept { return end_; }
    Position GetEndPos() const noexcept { return {static_cast<CoordD>(end_.x), static_cast<CoordD>(end_.y)}; }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

public:
    const Rectangle& GetBounds() const noexcept { return bounds_; }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

public:
    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

public:
    const Id& GetId() const noexcept { return id_; }
    Point GetPosition() const noexcept { return position_; }
    Offset GetOffset() const noexcept { return offset_; }

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

    Map(Id id, std::string name, model::DimensionD default_speed, size_t default_bag_capacity) noexcept
        : id_(std::move(id))
        , name_(std::move(name))
        , default_speed_(default_speed)
        , default_bag_capacity_(default_bag_capacity) {
    }

    const Id& GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }
    const Buildings& GetBuildings() const noexcept { return buildings_; }
    const Roads& GetRoads() const noexcept { return roads_; }
    const Offices& GetOffices() const noexcept { return offices_; }
    DimensionD GetDefaultSpeed() const noexcept { return default_speed_; }
    size_t GetDefaultBagCapacity() const noexcept { return default_bag_capacity_; }

    void AddRoad(const Road& road) { roads_.emplace_back(road); }
    void AddBuilding(const Building& building) { buildings_.emplace_back(building); }

    void AddOffice(Office office);

    bool CheckRoadPosition(Road::Position pos) const noexcept { return true; }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    DimensionD default_speed_;
    size_t default_bag_capacity_;
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
    using Speed = geom::Vec2D;
    using Position = geom::Point2D;

private:
    static constexpr Position DEFAULT_POSITION = Position{};
    static constexpr Speed DEFAULT_SPEED = Speed{};
    static constexpr size_t DEFAULT_BAG_CAPACITY = 3;

public:
    using DogId = std::uint64_t;

    Dog(const std::string& name, 
        DogId id, 
        Position position = DEFAULT_POSITION, 
        Speed speed = DEFAULT_SPEED, 
        size_t bag_capacity = DEFAULT_BAG_CAPACITY)
        : name_(name)
        , id_(id)
        , position_(position)
        , speed_(speed)
        , bag_{bag_capacity, std::nullopt} {
    }

public:
    struct BagItem {
        size_t id;
        size_t type;
    };

public:
    size_t GetBagCapacity() const noexcept { return bag_.size(); }
    std::vector<BagItem> GetBagItems() const noexcept;
    bool AddItemInBag(BagItem item);
    size_t ClearBag();

    Position GetPosition() const noexcept { return position_; }
    void SetPosition(const Position& position) { position_ = position; }

    Speed GetSpeed() const noexcept { return speed_; }
    void SetSpeed(const Speed& speed) { speed_ = speed; }

    Direction GetDirection() const noexcept { return direction_; }
    void SetDirection(Direction direction) { direction_ = direction; }

    DogId GetId() const noexcept { return id_; }

    const std::string& GetName() const noexcept { return name_; }

private:
    std::string name_;
    DogId id_;
    Position position_{0.0, 0.0};
    Speed speed_ = DEFAULT_SPEED;
    Direction direction_ = Direction::NORTH;
    std::vector<std::optional<BagItem>> bag_;
};

class GameSession {
private:
    using Dogs = std::vector<std::unique_ptr<Dog>>;
    using DogIdToDog = std::unordered_map<std::uint64_t, Dog*>;

public:
struct LostObject {
    size_t type = 0;
    Road::Position position{};
};

public:
    explicit GameSession(const Map* map, size_t lost_object_type_count = 0)
        : map_(map) 
        , lost_object_type_count_(lost_object_type_count)
    {}

    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

public:
    Dog* CreateDog(const std::string& name, bool randomize_spawn_point = false);
    Dog* CreateDog(Dog::DogId id, const std::string& name, const Dog::Position& pos, const Dog::Speed& speed);
    std::vector<Dog*> GetDogs() const;
    void RemoveDog(Dog* dog);

    size_t GetLostObjectTypeCount() const noexcept { return lost_object_type_count_; }
    const std::vector<LostObject>& GetLostObjects() const noexcept { return lost_objects_; }
    void SetLostObjects(const std::vector<LostObject>& lost_objects) { lost_objects_ = lost_objects; }
    void GenerateLostObjects(unsigned lost_object_count);
    void RemoveLostObjects(const std::set<size_t>& lost_object_indeces) {
        for (auto index_it = lost_object_indeces.rbegin(); index_it != lost_object_indeces.rend(); ++index_it) {
            lost_objects_.erase(lost_objects_.begin() + *index_it); 
        }
    }

    const Map* GetMap() const noexcept { return map_; }

private:
    Road::Position GenerateRoadPosition(bool randomize = false) const noexcept;

private:
    const Map* map_;
    size_t lost_object_type_count_ = 0;
    Dogs dogs_;
    DogIdToDog dog_id_to_dog_;
    std::vector<LostObject> lost_objects_;
};

class Game {
public:
    using Sessions = std::vector<std::unique_ptr<GameSession>>;

    static constexpr DimensionD DEFAULT_SPEED = 1.0;
    static constexpr size_t DEFAULT_BAG_CAPACITY = 3;

public:
    Game(DimensionD map_default_speed = DEFAULT_SPEED, size_t map_default_bag_capacity = DEFAULT_BAG_CAPACITY)
         : map_default_speed_{map_default_speed}
         , map_default_bag_capacity_{map_default_bag_capacity}
    {}

public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept { return maps_; }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

public:
    DimensionD GetMapDefaultSpeed() const noexcept { return map_default_speed_; }
    size_t GetMapDefaultBagCapacity() const noexcept { return map_default_bag_capacity_; }

    const Sessions& GetSessions() const noexcept { return sessions_; }
    GameSession* CreateSession(const Map* map, size_t lost_object_type_count = 0) { 
        return sessions_.emplace_back(std::make_unique<GameSession>(map, lost_object_type_count)).get(); 
    }
    GameSession* FindSession(const Map* map) const {
        auto it = std::find_if(sessions_.begin(), sessions_.end(), [map](const auto& session){ return session->GetMap() == map; });
        return (it != sessions_.end() ? it->get() : nullptr);
    }

private:
    DimensionD map_default_speed_;
    size_t map_default_bag_capacity_;

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;

private:
    Sessions sessions_;
};

}  // namespace model
