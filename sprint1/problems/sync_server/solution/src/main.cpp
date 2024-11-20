#ifdef WIN32
#include <sdkddkver.h>
#endif
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio.hpp>
#include <chrono>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <thread>
#include <optional>


namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;
namespace beast = boost::beast;
namespace http = beast::http;

// Запрос клиента, тело которого представлено в виде строки
using StringRequest = http::request<http::string_body>;
// Ответ нашего сервера, тело которого представлено в виде строки
using StringResponse = http::response<http::string_body>;

// Структура ContentType задаёт область видимости для констант,
// задающий значения HTTP-заголовка Content-Type
struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    // При необходимости внутрь ContentType можно добавить и другие типы контента
};

// Метод чтения 1 запроса клиента к нашему серверу
std::optional<StringRequest> ReadRequest(tcp::socket& socket, beast::flat_buffer& buffer) {
    beast::error_code ec;
    StringRequest req;
    // Считываем из socket запрос req, используя buffer для хранения данных.
    // В ec функция запишет код ошибки.
    // socket берет на себя роль потока
    http::read(socket, buffer, req, ec);

    // Клиент завершил соединение с нашим сервером
    if (ec == http::error::end_of_stream) {
        return std::nullopt;
    }
    // Остальные ошибки просто через исключения
    if (ec) {
        throw std::runtime_error("Failed to read request: "s.append(ec.message()));
    }
    return req;
}

// Создаёт StringResponse с заданными параметрами
StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                  bool keep_alive,
                                  std::string_view content_type = ContentType::TEXT_HTML) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    return response;
}

StringResponse HandleRequest(StringRequest&& req) {
    static const auto make_request_body = [&req]() {
        std::string request_body;
        switch (req.method())
        {
        case http::verb::get:
        case http::verb::head: {
            std::string target_name;
            if (!req.target().empty()) {
                target_name = std::string(req.target().begin() + 1, req.target().end());
            }
            request_body = "Hello, "s + target_name;
            break;
        }
        default: request_body = "Invalid method"sv;
        }

        return request_body;
    };

    auto response = MakeStringResponse(http::status::ok,
                                       make_request_body(),
                                       req.version(),
                                       req.keep_alive());
    switch (req.method())
    {
    case http::verb::get: break;
    case http::verb::head: {
        response.body() = ""sv;
        break;
    }
    default: {
        response.result(http::status::method_not_allowed);
        response.set("Allow"sv, "GET, HEAD"sv);
        break;
    }
    }

    return response;
}

// Метод обработки запросов после подключения клиента к нашему серверу
template <typename RequestHandler>
void HandleConnection(tcp::socket& socket, RequestHandler&& handle_request) {
    try {
        // Буфер для чтения данных в рамках текущей сессии.
        beast::flat_buffer buffer;

        // Продолжаем обработку запросов, пока клиент их отправляет
        while (auto request = ReadRequest(socket, buffer)) {
            // Делегируем обработку запроса функции handle_request
            // Получаем ответ на запрос
            StringResponse response = handle_request(*std::move(request));

            // Отправляем ответ на запрос клиенту
            http::write(socket, response);

            // Прекращаем обработку запросов, если семантика ответа требует это ???
            if (response.need_eof()) {
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    beast::error_code ec;
    // Запрещаем дальнейшую отправку данных через этот сокет - HTTP-сессия завершена.
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

int main() {
    // Контекст работы асинхронных операций
    // Или контекст для выполнения синхронных и асинхронных операций ввода/вывода
    net::io_context ioc;

    // Адрес и порт, с которого сервер будет принимать подключения
    // В данном случаем сервер будет принимать любое подключение (от всех сетевых интерфейсах этого компьютера) по порту port_for_listen
    const auto address_for_listen = net::ip::make_address("0.0.0.0");
    constexpr unsigned short port_for_listen = 8080;
    
    // Объект, позволяющий принимать tcp-подключения к сокету
    tcp::acceptor acceptor(ioc, {address_for_listen, port_for_listen});

    std::cout << "Server has started..."sv << std::endl;
    while (true) {
        // Создаем сокет (для двустороннего обмена данными с удалённым компьютером)
        tcp::socket socket(ioc);

        // Ожидаем подключение клиента
        acceptor.accept(socket);

        // Подключение выполнено: в отдельном потоке выполняем обработку запросов с клиента
        std::thread t(
            [](tcp::socket socket) {
                // Вызываем HandleConnection, передавая ей функцию-обработчик запроса
                HandleConnection(socket, HandleRequest);
            },
            std::move(socket));
        t.detach();
    }

    // Выведите строчку "Server has started...", когда сервер будет готов принимать подключения
}
