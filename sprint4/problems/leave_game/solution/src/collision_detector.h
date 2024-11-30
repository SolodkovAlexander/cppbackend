#pragma once

#include "geom.h"

#include <algorithm>
#include <vector>

namespace collision_detector {

struct CollectionResult {
    bool IsCollected(double collect_radius) const {
        return proj_ratio >= 0 && proj_ratio <= 1 && sq_distance <= collect_radius * collect_radius;
    }

    // квадрат расстояния до точки
    double sq_distance;
    // доля пройденного отрезка
    double proj_ratio;
};

// Движемся из точки a в точку b и пытаемся подобрать точку c.
CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c);

struct Item {
    geom::Point2D position;
    double width;
};

struct Gatherer {
    geom::Point2D start_pos;
    geom::Point2D end_pos;
    double width;
};

class ItemGathererProvider {
protected:
    ~ItemGathererProvider() = default;

public:
    virtual size_t ItemsCount() const = 0;
    virtual Item GetItem(size_t idx) const = 0;
    virtual size_t GatherersCount() const = 0;
    virtual Gatherer GetGatherer(size_t idx) const = 0;
};

struct GatheringEvent {
    size_t item_id;
    size_t gatherer_id;
    double sq_distance;
    double time;
};

class Provider : public ItemGathererProvider {
public:
    Provider() = default;

    Provider(const std::vector<Gatherer>& gatherers, const std::vector<Item>& items)
        : gatherers_(gatherers)
        , items_(items)
      {}

public:
    size_t ItemsCount() const { return items_.size(); }
    Item GetItem(size_t idx) const { return items_.at(idx); }
    size_t GatherersCount() const { return gatherers_.size(); }
    Gatherer GetGatherer(size_t idx) const { return gatherers_.at(idx); }

private:
    std::vector<Gatherer> gatherers_;
    std::vector<Item> items_;
};

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider);

}  // namespace collision_detector