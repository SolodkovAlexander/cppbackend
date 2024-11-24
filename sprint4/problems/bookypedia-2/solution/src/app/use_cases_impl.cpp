#include "use_cases_impl.h"

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save({AuthorId::New(), name});
}
std::vector<domain::Author> UseCasesImpl::GetAuthors() {
    return authors_.GetAuthors();
}

void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, int publication_year) {
    books_.Save({BookId::New(), author_id, title, publication_year});
}
std::vector<domain::Book> UseCasesImpl::GetAuthorBooks(const domain::Author& author) {
    return books_.GetAuthorBooks(author);
}
std::vector<domain::Book> UseCasesImpl::GetBooks() {
    return books_.GetBooks();
}

}  // namespace app
