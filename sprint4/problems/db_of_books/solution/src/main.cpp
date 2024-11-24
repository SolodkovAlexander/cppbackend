#include "sdk.h"

#include <boost/core/detail/string_view.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <pqxx/pqxx>

/*
    TODO: использовать Serialization из boost для получения объектов для запросов.
*/

namespace json = boost::json;
using namespace std::literals;

// libpqxx использует zero-terminated символьные литералы вроде "abc"_zv;
using pqxx::operator"" _zv;

void PrepareDataBase(pqxx::connection& db_connection) {
    pqxx::work w(db_connection);

    // Создаем таблицы
    w.exec(
        "CREATE TABLE IF NOT EXISTS books \
         (id SERIAL PRIMARY KEY, \
          author varchar(100) NOT NULL, \
          title varchar(100) NOT NULL, \
          year integer NOT NULL, \
          ISBN char(13) UNIQUE);"_zv
    );

    w.commit();

    db_connection.prepare("add_book"_zv, "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4)"_zv);
}

struct Book {
    int id{0};
    std::string title;
    std::string author;
    int year{0};
    std::optional<std::string> ISBN;

    static Book FromJson(const json::value& book_json) {
        const auto book_json_obj = book_json.as_object();
        return Book{
            0,
            book_json_obj.at("title"sv).as_string().data(),
            book_json_obj.at("author"sv).as_string().data(),
            static_cast<int>(book_json_obj.at("year"sv).as_int64()),
            book_json_obj.at("ISBN"sv).is_null() ? std::nullopt : std::optional<std::string>{book_json_obj.at("ISBN"sv).as_string().data()}
        };
    }
};

std::ostream& operator<<(std::ostream& out, const Book& book) {
    auto book_json_obj = json::object{
        {"id"sv, book.id},
        {"title"sv, book.title},
        {"author"sv, book.author},
        {"year"sv, book.year}
    };
    if (book.ISBN) {
        book_json_obj["ISBN"sv] = *book.ISBN;
    } else {
        book_json_obj["ISBN"sv] = nullptr;
    }

    out << json::serialize(book_json_obj);
    return out;
}

void HandleRequestAddBook(pqxx::connection& db_connection, Book book) {
    constexpr auto prepared_query_name = "add_book"_zv;

    pqxx::work w(db_connection);
    w.exec_prepared(prepared_query_name, book.title, book.author, book.year, book.ISBN);
    w.commit();
}

void HandleRequestSelectBooks(pqxx::connection& db_connection) {
    pqxx::read_transaction r(db_connection);
    constexpr auto selectQueryText = "SELECT id, title, author, year, ISBN FROM books ORDER BY year DESC, title ASC, author ASC, ISBN ASC"_zv;
    
    bool first_record = true;
    std::cout << '[';
    for (auto [id, title, author, year, ISBN] : r.query<int, std::string, std::string, int, std::optional<std::string>>(selectQueryText)) {
        if (!first_record) { std::cout << ','; }

        std::cout << Book{id, title, author, year, ISBN};

        if (first_record) { first_record = false; }
    }
    std::cout << ']' << std::endl;
}

void HandleRequests(pqxx::connection& db_connection) {
    std::string command_json;
    while (std::getline(std::cin, command_json)) {
        // Парсим команду
        auto command_data = json::parse(command_json).as_object();
        std::string command = command_data.at("action"sv).as_string().data();

        if (command == "add_book"sv) { // Добавление книги
            try {
                HandleRequestAddBook(db_connection, Book::FromJson(command_data.at("payload"sv)));
                std::cout << json::serialize(json::object{{"result"sv, true}}) << std::endl;
            } catch (const std::exception& e) {
                std::cout << json::serialize(json::object{{"result"sv, false}}) << std::endl;
            }
        } else if (command == "all_books"sv) { // Запрос всех книг
            HandleRequestSelectBooks(db_connection);
        } else if (command == "exit"sv) { // Выход
            break;
        }
    }
}

int main(int argc, const char* argv[]) {
    try {
        if (argc == 1) {
            std::cout << "Usage: connect_db <conn-string>\n"sv;
            return EXIT_SUCCESS;
        } else if (argc != 2) {
            std::cerr << "Invalid command line\n"sv;
            return EXIT_FAILURE;
        }

        // Подключаемся к БД, указывая её параметры в качестве аргумента
        pqxx::connection conn{argv[1]};

        // Подготавливаем БД для запросов
        PrepareDataBase(conn);

        // Обрабатываем запросы
        HandleRequests(conn);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
