#pragma once

#include <memory>
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
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

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

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
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
};

class Dog {
public:
    using DogId = std::uint64_t;

    Dog(const std::string& name, DogId id) : 
        name_(name),
        id_(id)
    {}

public:
    DogId GetId() const {
        return id_;
    }
    std::string GetName() const {
        return name_;
    }

private:
    std::string name_;
    DogId id_;
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
    Dog* CreateDog(const std::string& name) {
        auto dog = dogs_.emplace_back(std::make_unique<Dog>(name, dogs_.size())).get();
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

private:
    Dogs dogs_;
    DogIdToDog dog_id_to_dog_;
    const Map* map_;
};

class Game {
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
    GameSession* CreateSession(const Map* map) {
        return sessions_.emplace_back(std::make_unique<GameSession>(map)).get();
    }
    GameSession* FindSession(const Map* map) const {
        auto it = std::find_if(sessions_.begin(), sessions_.end(), [map](const auto& session){ return session->GetMap() == map; });
        return (it != sessions_.end() ? it->get() : nullptr);
    }

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
