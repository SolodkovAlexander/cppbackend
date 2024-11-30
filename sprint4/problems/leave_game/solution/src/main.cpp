#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/program_options.hpp>
#include <boost/signals2.hpp>
#include <iostream>
#include <thread>

#include "application.h"
#include "json_parser.h"
#include "json_logger.h"
#include "model.h"
#include "players.h"
#include "request_handler.h"
#include "server_state_saver.h"
#include "ticker.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sig = boost::signals2;
namespace sys = boost::system;

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

struct Args {
    int tick_period{-1};
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points;
    std::string state_file;
    int save_state_period{-1};
}; 

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"Allowed options"s};

    Args args;
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value(&args.tick_period)->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file"), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions ")
        ("state-file", po::value(&args.state_file)->value_name("file"), "application state file for backup")
        ("save-state-period", po::value(&args.save_state_period)->value_name("milliseconds"), "period of make backup");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        std::cout << desc;
        return std::nullopt;
    }
    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Config file path have not been specified"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("Static files root dir have not specified"s);
    }
    if (!vm.contains("tick-period"s)) {
        args.tick_period = -1;
    }
    args.randomize_spawn_points = vm.contains("randomize-spawn-points"s);

    if (!vm.contains("state-file"s)
        || !vm.contains("save-state-period"s)) {
        args.save_state_period = -1;
    }

    return args;
} 

int main(int argc, const char* argv[]) {
    json_logger::InitLogger();
    try {
        if (auto args = ParseCommandLine(argc, argv)) {
            std::filesystem::path config_file = args->config_file;
            std::string www_root = args->www_root;

            // 1. Создаем приложение, управляющее игрой и действиями в игре
            auto [game, extra_data] = json_parser::LoadGame<game_scenarios::ExtraData>(config_file);
            game_scenarios::Application app(std::move(game),
                                            std::move(extra_data),
                                            args->randomize_spawn_points,
                                            args->tick_period >= 0);

            // 1.1. Создаем помощник по сохранению состояния сервера
            server_state_saver::ServerStateSaver server_state_saver(app,
                                                                    args->state_file, 
                                                                    args->save_state_period);
            // 1.2. Восстанавливаем состояние сервера
            server_state_saver.RestoreState();

            // 1.2 Делаем обработку сигнала по tick: сохраняем состояние сервера
            // Лямбда-функция будет вызываться всякий раз, когда Application будет слать сигнал tick
            // Функция перестанет вызываться после разрушения save_state_connection.
            sig::scoped_connection save_state_connection;
            if (!args->state_file.empty() && args->save_state_period > 0) {
                save_state_connection = app.DoOnTick([total = 0ms, &server_state_saver](std::chrono::milliseconds delta) mutable {
                    // Делаем backup состояния приложения
                    server_state_saver.SaveState(delta);
                });
            }

            // 2. Инициализируем io_context
            const unsigned num_threads = std::thread::hardware_concurrency();
            net::io_context ioc(num_threads);

            // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
            // Подписываемся на сигналы и при их получении завершаем работу сервера
            net::signal_set signals(ioc, SIGINT, SIGTERM);
            signals.async_wait([&ioc, &server_state_saver](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
                if (!ec) {
                    ioc.stop();
                }
            });

            // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
            auto api_strand = net::make_strand(ioc);
            auto handler = std::make_shared<http_handler::RequestHandler>(app, www_root, api_strand);

            // 5. Настраиваем вызов метода RequestHandler::Tick
            auto ticker = std::make_shared<http_handler::Ticker>(api_strand, std::chrono::milliseconds(args->tick_period),
                [&app](std::chrono::milliseconds delta) { 
                    if (app.GetAutoTick()) {
                        app.Tick(delta);
                    }                    
                }
            );
            ticker->Start();

            // 6. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
            const auto address = net::ip::make_address("0.0.0.0");
            constexpr unsigned short port = 8080;
            http_server::ServeHttp(ioc, {address, port}, [handler](auto&& req, auto&& send) {
                (*handler)(std::forward<decltype(req)>(req), 
                        std::forward<decltype(send)>(send));
            });        

            // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
            json_logger::LogData("server started"sv, boost::json::object{{"port", port}, {"address", address.to_string()}});

            // 7. Запускаем обработку асинхронных операций
            RunWorkers(std::max(1u, num_threads), [&ioc] {
                ioc.run();
            });

            // 8. Сохраняем состояние сервера
            server_state_saver.SaveState();

            json_logger::LogData("server exited"sv, boost::json::object{{"code", 0}});
        }
    } catch (const std::exception& ex) {
        json_logger::LogData("server exited"sv, boost::json::object{{"code", EXIT_FAILURE}, {"exception", ex.what()}});
        return EXIT_FAILURE;
    }
}
