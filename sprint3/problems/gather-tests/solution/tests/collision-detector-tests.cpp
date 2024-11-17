#define _USE_MATH_DEFINES

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <sstream>
#include <vector>

#include "../src/collision_detector.h"

using namespace std::literals;
using namespace collision_detector;
using namespace geom;

namespace Catch {
template<>
struct StringMaker<collision_detector::GatheringEvent> {
  static std::string convert(collision_detector::GatheringEvent const& value) {
      std::ostringstream tmp;
      tmp << "(" << value.gatherer_id << "," << value.item_id << "," << value.sq_distance << "," << value.time << ")";

      return tmp.str();
  }
};

template <typename Events>
struct AreEventsEqualMatcher : Catch::Matchers::MatcherGenericBase {
    AreEventsEqualMatcher(Events events)
        : events_{std::move(events)} {
    }
    AreEventsEqualMatcher(AreEventsEqualMatcher&&) = default;

    template <typename OtherEvents>
    bool match(OtherEvents other) const {
        using Catch::Matchers::WithinAbs;

        REQUIRE(events_.size() == other.size());
        for (size_t i = 0; i < events_.size(); ++i) {
            CHECK(events_.at(i).item_id == other.at(i).item_id);
            CHECK(events_.at(i).gatherer_id == other.at(i).gatherer_id);
            CHECK_THAT(other.at(i).sq_distance, WithinAbs(events_.at(i).sq_distance, 1e-10));
            CHECK_THAT(other.at(i).time, WithinAbs(events_.at(i).time, 1e-10));
        }
        return true;
    }

    std::string describe() const override {
        // Описание свойства, проверяемого матчером:
        return "Is permutation of: "s + Catch::rangeToString(events_);
    }

private:
    Events events_;
};

template<typename Events>
AreEventsEqualMatcher<Events> AreEventsEqual(Events&& events) {
    return AreEventsEqualMatcher<Events>{std::forward<Events>(events)};
}

}  // namespace Catch

class ItemGathererProviderTest : public ItemGathererProvider {
public:
    ItemGathererProviderTest() = default;

    ItemGathererProviderTest(std::vector<Gatherer> gatherers, std::vector<Item> items = {})
        : gatherers_(std::move(gatherers))
        , items_(std::move(items))
      {}

public:
    size_t ItemsCount() const {
        return items_.size();
    }
    Item GetItem(size_t idx) const {
        return items_.at(idx);
    }
    size_t GatherersCount() const {
        return gatherers_.size();
    }
    Gatherer GetGatherer(size_t idx) const {
        return gatherers_.at(idx);
    }

private:
    std::vector<Gatherer> gatherers_;
    std::vector<Item> items_;
};

SCENARIO("Check no events") {
    std::vector<Gatherer> gatherers;
    gatherers.emplace_back(Gatherer{{0.0, 0.0},{1.0, 0.0}, 1.0});

    GIVEN("Have provider without any data") {
        REQUIRE(FindGatherEvents(ItemGathererProviderTest{}).empty());
        REQUIRE(FindGatherEvents(ItemGathererProviderTest{gatherers}).empty());
        REQUIRE(FindGatherEvents(ItemGathererProviderTest{{}, {{{0.0, 0.0}, 1.0}}}).empty());
    }

    GIVEN("Have provider with data, but no events") {
        auto events = FindGatherEvents(ItemGathererProviderTest{gatherers, {{{-2.0, 0.0}, 0.5}}});
        WHEN("one gatherer and item on line before gatherer start pos") {
            THEN("no events") { REQUIRE(events.empty()); }
        }
        events = FindGatherEvents(ItemGathererProviderTest{gatherers, {{{2.0, 0.0}, 0.5}}});
        WHEN("one gatherer and item on line after gatherer finish pos") {
            THEN("no events") { REQUIRE(events.empty()); }
        }
        events = FindGatherEvents(ItemGathererProviderTest{gatherers, {{{0.5, 2.0}, 0.5}}});
        WHEN("one gatherer and item above the segment") {
            THEN("no events") { REQUIRE(events.empty()); }
        }
        events = FindGatherEvents(ItemGathererProviderTest{gatherers, {{{0.5, 2.0}, 0.5}}});
        WHEN("one gatherer and item below the segment") {
            THEN("no events") { REQUIRE(events.empty()); }
        }
        events = FindGatherEvents(ItemGathererProviderTest{gatherers, {{{0.5, -2.0}, 0.5}}});
        WHEN("one gatherer and item after finish pos below the line") {
            THEN("no events") { REQUIRE(events.empty()); }
        }
        events = FindGatherEvents(ItemGathererProviderTest{gatherers, {{{-2, -2.0}, 0.5}}});
        WHEN("one gatherer and item before start pos below the line") {
            THEN("no events") { REQUIRE(events.empty()); }
        }
        events = FindGatherEvents(ItemGathererProviderTest{gatherers, {{{-2, 2.0}, 0.5}}});
        WHEN("one gatherer and item before start pos above the line") {
            THEN("no events") { REQUIRE(events.empty()); }
        }
        events = FindGatherEvents(ItemGathererProviderTest{gatherers, {{{2, 2.0}, 0.5}}});
        WHEN("one gatherer and item after finish pos above the line") {
            THEN("no events") { REQUIRE(events.empty()); }
        }
        events = FindGatherEvents(ItemGathererProviderTest{{{{0.0, 0.0},{0.0, 0.0}, 1.0}}});
        WHEN("one gatherer no moving") {
            THEN("no events") { REQUIRE(events.empty()); }
        }
        events = FindGatherEvents(ItemGathererProviderTest{{{{0.0, 0.0},{0.0, 0.0}, 1.0}},
                                                           {{{0.0, 0.0}, 0.5}}});
        WHEN("one gatherer no moving and item on start post") {
            THEN("no events") { REQUIRE(events.empty()); }
        }
    }
}

SCENARIO("Check existing events") {
    std::vector<Gatherer> gatherers;
    gatherers.emplace_back(Gatherer{{0.0, 0.0},{10.0, 0.0}, 1.0});

    GIVEN("one gatherer and some items item") {
        WHEN("items on gatherer's segment on line") {
            std::vector<GatheringEvent> events = FindGatherEvents(ItemGathererProviderTest{gatherers, {{{0.0, 0.0}, 0.5},
                                                                                                       {{1.0, 0.0}, 0.5},
                                                                                                       {{3.0, 0.0}, 2.0},
                                                                                                       {{5.0, 0.0}, 1.0},
                                                                                                       {{7.0, 0.0}, 20.0},
                                                                                                       {{10.0, 0.0}, 1.0}}});
            THEN("some events") { 
                CHECK_THAT(events, Catch::AreEventsEqual(std::vector{GatheringEvent{0,0,0,0},
                                                                     GatheringEvent{1,0,0,0.1},
                                                                     GatheringEvent{2,0,0,0.3},
                                                                     GatheringEvent{3,0,0,0.5},
                                                                     GatheringEvent{4,0,0,0.7},
                                                                     GatheringEvent{5,0,0,1.0}}));
            }
        }
        WHEN("items on gatherer's segment on line (gatherer moving inverse)") {
            std::vector<GatheringEvent> events = FindGatherEvents(ItemGathererProviderTest{{{{0.0, 0.0},{-10.0, 0.0}, 1.0}}, 
                                                                                           {{{0.0, 0.0}, 0.5},
                                                                                            {{-1.0, 0.0}, 0.5},
                                                                                            {{-3.0, 0.0}, 2.0},
                                                                                            {{-5.0, 0.0}, 1.0},
                                                                                            {{-7.0, 0.0}, 20.0},
                                                                                            {{-10.0, 0.0}, 1.0}}});
            THEN("some events") { 
                CHECK_THAT(events, Catch::AreEventsEqual(std::vector{GatheringEvent{0,0,0,0},
                                                                     GatheringEvent{1,0,0,0.1},
                                                                     GatheringEvent{2,0,0,0.3},
                                                                     GatheringEvent{3,0,0,0.5},
                                                                     GatheringEvent{4,0,0,0.7},
                                                                     GatheringEvent{5,0,0,1.0}}));
            }
        }
        WHEN("items on gatherer's segment below/above line") {
            std::vector<GatheringEvent> events = FindGatherEvents(ItemGathererProviderTest{gatherers, {{{1.0, -2.0}, 0.5},
                                                                                                       {{1.0, -1.5}, 0.5},
                                                                                                       {{1.0, -1.0}, 0.5},
                                                                                                       {{1.0, -0.5}, 0.5},
                                                                                                       {{1.0, 0.5}, 0.5},
                                                                                                       {{1.0, 1.0}, 0.5},
                                                                                                       {{1.0, 1.5}, 0.5},
                                                                                                       {{1.0, 2.0}, 0.5}}});
            THEN("some events") { 
                CHECK_THAT(events, Catch::AreEventsEqual(std::vector{GatheringEvent{1,0,2.25,0.1},
                                                                     GatheringEvent{2,0,1.0,0.1},
                                                                     GatheringEvent{3,0,0.25,0.1},
                                                                     GatheringEvent{4,0,0.25,0.1},
                                                                     GatheringEvent{5,0,1.0,0.1},
                                                                     GatheringEvent{6,0,2.25,0.1}}));
            }
        }
        WHEN("items on gatherer's segment below/above line (gatherer moving inverse)") {
            std::vector<GatheringEvent> events = FindGatherEvents(ItemGathererProviderTest{{{{0.0, 0.0},{-10.0, 0.0}, 1.0}}, 
                                                                                           {{{-1.0, -2.0}, 0.5},
                                                                                            {{-1.0, -1.5}, 0.5},
                                                                                            {{-1.0, -1.0}, 0.5},
                                                                                            {{-1.0, -0.5}, 0.5},
                                                                                            {{-1.0, 0.5}, 0.5},
                                                                                            {{-1.0, 1.0}, 0.5},
                                                                                            {{-1.0, 1.5}, 0.5},
                                                                                            {{-1.0, 2.0}, 0.5}}});
            THEN("some events") { 
                CHECK_THAT(events, Catch::AreEventsEqual(std::vector{GatheringEvent{1,0,2.25,0.1},
                                                                     GatheringEvent{2,0,1.0,0.1},
                                                                     GatheringEvent{3,0,0.25,0.1},
                                                                     GatheringEvent{4,0,0.25,0.1},
                                                                     GatheringEvent{5,0,1.0,0.1},
                                                                     GatheringEvent{6,0,2.25,0.1}}));
            }
        }
    }
}
