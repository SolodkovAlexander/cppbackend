#pragma once

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <pqxx/pqxx>
#include <pqxx/zview.hxx>
#include <pqxx/connection>
#include <pqxx/transaction>

namespace database_control {

using namespace std::literals;
using pqxx::operator"" _zv;

struct PlayerScore {
    std::string name;
    size_t score = 0;
    std::chrono::milliseconds play_time_ms = 0ms;
};

class Database {

public:
    explicit Database(std::string db_url)
        : connection_{db_url} {
    }

public:
    void Prepare() {
        pqxx::work work{connection_};
        work.exec(R"(
            CREATE TABLE IF NOT EXISTS retired_players (
            id UUID CONSTRAINT rp_id_constraint PRIMARY KEY,
            name varchar(100) NOT NULL,
            score integer NOT NULL,
            play_time_ms integer NOT NULL);
        )"_zv);
        work.exec(R"(
            CREATE INDEX IF NOT EXISTS rp_idx ON retired_players (score DESC, play_time_ms, name);
        )"_zv);
        work.commit();
    }

    void AddPlayerScore(const PlayerScore& player_score) {
        pqxx::work work{connection_};
        work.exec_params(
            R"(INSERT INTO retired_players (id, name, score, play_time_ms) VALUES ($1, $2, $3, $4);)"_zv,
            Database::GenerateUUID(), player_score.name, player_score.score, player_score.play_time_ms.count());
        work.commit();
    }

    std::vector<PlayerScore> GetPlayersScore(int offset, int limit) {
        pqxx::read_transaction r{connection_};

        auto query_text = "SELECT name, score, play_time_ms FROM retired_players \
                           ORDER BY score DESC, play_time_ms ASC, name ASC \
                           LIMIT "s + std::to_string(limit) + " OFFSET "s + std::to_string(offset) + ";"s;
        
        std::vector<PlayerScore> players_score;
        for (auto [name, score, play_time_ms] : r.query<std::string, size_t, int>(query_text)) {
            players_score.emplace_back(PlayerScore{name, score, std::chrono::milliseconds(play_time_ms)});
        }
        return players_score;
    }

private:
    static std::string GenerateUUID() {
        return boost::uuids::to_string(boost::uuids::random_generator()());
    }

private:
    pqxx::connection connection_;
};

}  //namespace database_control
