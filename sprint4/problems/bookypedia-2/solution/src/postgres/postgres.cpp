#include "postgres.h"

#include <pqxx/pqxx>
#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    // Пока каждое обращение к репозиторию выполняется внутри отдельной транзакции
    // В будущих уроках вы узнаете про паттерн Unit of Work, при помощи которого сможете несколько
    // запросов выполнить в рамках одной транзакции.
    // Вы также может самостоятельно почитать информацию про этот паттерн и применить его здесь.
    pqxx::work work{connection_};
    work.exec_params(
        R"(INSERT INTO authors (id, name) VALUES ($1, $2) ON CONFLICT (id) DO UPDATE SET name=$2;)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAuthors() {
    pqxx::read_transaction r{connection_};

    constexpr auto query_text = R"(SELECT id, name FROM authors ORDER BY name ASC)"_zv;
    std::vector<domain::Author> authors;
    for (auto [id, name] : r.query<std::string, std::string>(query_text)) {
        authors.emplace_back(domain::Author(domain::AuthorId::FromString(id), name));
    }
    return authors;
}


void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4);)"_zv,
        book.GetId().ToString(), book.GetAuthorId(), book.GetTitle(), book.GetPublicationYear());
    work.commit();
}

std::vector<domain::Book> BookRepositoryImpl::GetAuthorBooks(const domain::Author& author) {
    pqxx::read_transaction r{connection_};

    auto query_text = "SELECT id, author_id, title, publication_year \
                       FROM books \
                       WHERE author_id = " + r.quote(author.GetId().ToString()) + " ORDER BY publication_year, title ASC";
    std::vector<domain::Book> books;
    for (auto [id, author_id, title, publication_year] : r.query<std::string, std::string, std::string, int>(query_text)) {
        books.emplace_back(domain::Book(domain::BookId::FromString(id), author_id, title, publication_year));
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetBooks() {
    pqxx::read_transaction r{connection_};

    constexpr auto query_text = R"(SELECT id, author_id, title, publication_year FROM books ORDER BY title ASC)"_zv;
    std::vector<domain::Book> books;
    for (auto [id, author_id, title, publication_year] : r.query<std::string, std::string, std::string, int>(query_text)) {
        books.emplace_back(domain::Book(domain::BookId::FromString(id), author_id, title, publication_year));
    }
    return books;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL,
    title varchar(100) NOT NULL,
    publication_year integer
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL,
    tag varchar(30) NOT NULL
);
)"_zv);

    // коммитим изменения
    work.commit();
}

}  // namespace postgres