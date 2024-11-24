#include <catch2/catch_test_macros.hpp>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;

    void Save(const domain::Author& author) override {
        saved_authors.emplace_back(author);
    }
    std::vector<domain::Author> GetAuthors() override {
        std::sort(saved_authors.begin(), saved_authors.end(), [](const domain::Author& lhs, const domain::Author& rhs) {
            return lhs.GetName() < rhs.GetName();
        });
        return saved_authors;
    }
};

struct MockBookRepository : domain::BookRepository {
    std::vector<domain::Book> saved_books;

    void Save(const domain::Book& book) override {
        saved_books.emplace_back(book);
    }
    std::vector<domain::Book> GetAuthorBooks(const domain::Author& author) override {
        std::vector<domain::Book> books(GetBooks());
    
        auto author_id = author.GetId().ToString();
        std::remove_if(books.begin(), books.end(), [&author_id](const domain::Book& book){
            return book.GetAuthorId() == author_id;
        });
        return books;
    }
    std::vector<domain::Book> GetBooks() override {
        std::sort(saved_books.begin(), saved_books.end(), [](const domain::Book& lhs, const domain::Book& rhs) {
            return lhs.GetTitle() < rhs.GetTitle();
        });
        return saved_books;
    }
};

struct Fixture {
    MockAuthorRepository authors;
    MockBookRepository books;
};

}  // namespace

SCENARIO_METHOD(Fixture, "Author Adding") {
    GIVEN("Use cases") {
        app::UseCasesImpl use_cases{authors, books};

        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            use_cases.AddAuthor(author_name);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(authors.saved_authors.size() == 1);
                CHECK(authors.saved_authors.at(0).GetName() == author_name);
                CHECK(authors.saved_authors.at(0).GetId() != domain::AuthorId{});
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Book Adding") {
    GIVEN("Use cases") {
        app::UseCasesImpl use_cases{authors, books};
        const auto author_name = "Joanne Rowling";
        use_cases.AddAuthor(author_name);

        WHEN("Adding a book") {
            const auto book_author_id = authors.saved_authors.at(0).GetId().ToString();
            const auto book_title = "Harry Potter and the Chamber of Secrets";
            const auto book_publication_year = 1998;
            use_cases.AddBook(book_author_id, book_title, book_publication_year);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(books.saved_books.size() == 1);
                CHECK(books.saved_books.at(0).GetId() != domain::BookId{});
                CHECK(books.saved_books.at(0).GetAuthorId() == authors.saved_authors.at(0).GetId().ToString());
                CHECK(books.saved_books.at(0).GetTitle() == book_title);
                CHECK(books.saved_books.at(0).GetPublicationYear() == book_publication_year);
            }
        }
    }
}