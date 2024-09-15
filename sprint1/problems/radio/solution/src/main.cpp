#include "audio.h"
#include <boost/asio.hpp>
#include <iostream>

namespace net = boost::asio;
// TCP больше не нужен, импортируем имя UDP
using net::ip::udp;

using namespace std::literals;

void StartServer(uint16_t port) {
    Player player(ma_format_u8, 1);
    
    // Количество байт оной датаграммы по UDP для отправки 
    static const size_t max_buffer_size = 65000;
    try {
            boost::asio::io_context io_context;
            udp::socket socket(io_context, udp::endpoint(udp::v4(), port));

            // Запускаем сервер в цикле, чтобы можно было работать со многими клиентами
            for (;;) {
                // Создаём буфер достаточного размера, чтобы вместить датаграмму.
                std::array<char, max_buffer_size> recv_buf;
                udp::endpoint remote_endpoint;

                // Получаем не только данные, но и endpoint клиента
                auto size = socket.receive_from(boost::asio::buffer(recv_buf), remote_endpoint);
                std::cout << "Client datagramm receiving done"sv << std::endl;

                // Определяем количество фреймов в датаграмме
                size_t frames = size / player.GetFrameSize();

                player.PlayBuffer(recv_buf.data(), frames, 1.5s);
                std::cout << "Playing done" << std::endl;
            }
        } catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
}

void StartClient(uint16_t port, std::string&& ip_addr, Recorder::RecordingResult&& record, int frame_size) {
    if (!record.frames) {
        return;
    }

    static net::io_context io_context;

    // Перед отправкой данных нужно открыть сокет. 
    // При открытии указываем протокол (IPv4 или IPv6) вместо endpoint.
    static udp::socket socket(io_context, udp::v4());

    // Количество байт оной датаграммы по UDP для отправки 
    static const size_t max_buffer_size = 65000;

    // Вычисляем сколько поместиться в одной датаграмме фреймов
    int frame_count_per_datagramm = max_buffer_size / frame_size;

    // Вычисляем максимальный размер 1 датаграммы с учетом размера 1 фрейма
    int max_datagramm_size = frame_count_per_datagramm * frame_size;

    try {
        boost::system::error_code ec;
        auto endpoint = udp::endpoint(net::ip::make_address(ip_addr, ec), port);

        // Вычисляем общее количиство байт для отправки
        size_t byte_count = record.frames * frame_size;
        if (record.data.size() < byte_count) {
            throw std::logic_error("Recorder::RecordingResult has invalid data"s);
        }

        // Вычисляем сколько датаграмм надо будет отправить
        int datagramm_count = byte_count / max_datagramm_size;

        // Запускаем цикл отправки датаграмм максимального размера
        auto data_ptr = record.data.data();
        while (byte_count >= max_datagramm_size) {
            socket.send_to(net::buffer(data_ptr, max_datagramm_size), endpoint);
            byte_count -= max_datagramm_size;
            data_ptr += max_datagramm_size;
        }

        // Отправляем оставшиеся байты
        if (byte_count > 0) {
            socket.send_to(net::buffer(data_ptr, byte_count), endpoint);
        }        
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: "sv << argv[0] << " <client/server> <port>"sv << std::endl;
        return 1;
    }

    // Определяем режим запуска программы
    std::string programm_mode(argv[1]);
    if (programm_mode == "client"sv) {
        Recorder recorder(ma_format_u8, 1);
        while (true) {
            std::string ip_addr;
            std::cout << "Usage: print IP-address to send record: ";
            std::getline(std::cin, ip_addr);

            auto rec_result = recorder.Record(65000, 1.5s);
            std::cout << "Recording done" << std::endl;

            StartClient(atoi(argv[2]), std::move(ip_addr), std::move(rec_result), recorder.GetFrameSize());
            std::cout << "Sending record done" << std::endl;
        }
    } else if (programm_mode == "server"sv) {
        StartServer(atoi(argv[2]));
    } else {
        std::cout << "Usage: "sv << argv[0] << " <client/server> <port>"sv << std::endl;
        return 1;
    }

    return 0;
}
