#include <cmath>
#include <catch2/catch_test_macros.hpp>

#include <unordered_set>

#include "../src/loot_generator.h"
#include "../src/model.h"

using namespace std::literals;
using namespace model;

SCENARIO("Loot objects generation in game session") {
    using loot_gen::LootGenerator;
    using TimeInterval = LootGenerator::TimeInterval;

    // Create game map
    Map map{Map::Id{"map1"s}, "Map 1"s, 4.0};
    map.AddRoad(Road{Road::HORIZONTAL, Point{0, 0}, Coord{40}});
    map.AddRoad(Road{Road::VERTICAL, Point{40, 0}, Coord{30}});
    map.AddRoad(Road{Road::HORIZONTAL, Point{40, 30}, Coord{0}});
    map.AddRoad(Road{Road::VERTICAL, Point{0, 0}, Coord{30}});
    map.AddBuilding(Building{Rectangle{Point{5,5}, Size{30, 20}}});
    map.AddOffice(Office{Office::Id{"o0"s}, Point{40, 30}, Offset{5, 0}});  

    // Create game
    Game game;
    game.AddMap(map);

    GIVEN("a game session") {
        auto game_session = game.CreateSession(game.FindMap(Map::Id{"map1"s}));

        WHEN("no lost object types") {
            THEN("no lost objects are generated") {
                game_session->GenerateLostObjects(100, 0);
                REQUIRE(game_session->GetLostObjects().empty());
            }
        }

        WHEN("no lost objects count") {
            THEN("no lost objects are generated") {
                game_session->GenerateLostObjects(0, 100);
                REQUIRE(game_session->GetLostObjects().empty());
            }
        }

        WHEN("have lost objects and types") {
            THEN("all lost objects are generated with randomize types") {
                game_session->GenerateLostObjects(100, 100);
                REQUIRE(game_session->GetLostObjects().size() == 100);

                std::unordered_set<size_t> generated_lost_object_types;
                for (const auto& lost_object : game_session->GetLostObjects()) {
                    generated_lost_object_types.insert(lost_object.type);
                }                
                REQUIRE(generated_lost_object_types.size() > 5);
            }
        }
    }
}
