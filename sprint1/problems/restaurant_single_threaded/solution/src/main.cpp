#ifdef WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <mutex>
#include <memory>
#include <sstream>
#include <syncstream>
#include <unordered_map>

namespace net = boost::asio;
namespace sys = boost::system;
namespace ph = std::placeholders;
using namespace std::chrono;
using namespace std::literals;
using Timer = net::steady_timer;

class Hamburger {
public:
    [[nodiscard]] bool IsCutletRoasted() const {
        return cutlet_roasted_;
    }
    void SetCutletRoasted() {
        if (IsCutletRoasted()) {  // Котлету можно жарить только один раз
            throw std::logic_error("Cutlet has been roasted already"s);
        }
        cutlet_roasted_ = true;
    }

    [[nodiscard]] bool HasOnion() const {
        return has_onion_;
    }
    // Добавляем лук
    void AddOnion() {
        if (IsPacked()) {  // Если гамбургер упакован, класть лук в него нельзя
            throw std::logic_error("Hamburger has been packed already"s);
        }
        AssureCutletRoasted();  // Лук разрешается класть лишь после прожаривания котлеты
        has_onion_ = true;
    }

    [[nodiscard]] bool IsPacked() const {
        return is_packed_;
    }
    void Pack() {
        AssureCutletRoasted();  // Нельзя упаковывать гамбургер, если котлета не прожарена
        is_packed_ = true;
    }

private:
    // Убеждаемся, что котлета прожарена
    void AssureCutletRoasted() const {
        if (!cutlet_roasted_) {
            throw std::logic_error("Bread has not been roasted yet"s);
        }
    }

    bool cutlet_roasted_ = false;  // Обжарена ли котлета?
    bool has_onion_ = false;       // Есть ли лук?
    bool is_packed_ = false;       // Упакован ли гамбургер?
};

std::ostream& operator<<(std::ostream& os, const Hamburger& h) {
    return os << "Hamburger: "sv << (h.IsCutletRoasted() ? "roasted cutlet"sv : " raw cutlet"sv)
              << (h.HasOnion() ? ", onion"sv : ""sv)
              << (h.IsPacked() ? ", packed"sv : ", not packed"sv);
}

class Logger {
public:
    explicit Logger(std::string id)
        : id_(std::move(id)) {
    }

    void LogMessage(std::string_view message) const {
        std::osyncstream os{std::cout}; // для асинхр. операций
        os << id_ << "> ["sv << duration<double>(steady_clock::now() - start_time_).count()
           << "s] "sv << message << std::endl;
    }

private:
    std::string id_;
    steady_clock::time_point start_time_{steady_clock::now()};
};

// Функция, которая будет вызвана по окончании обработки заказа
using OrderHandler = std::function<void(sys::error_code ec, int id, Hamburger* hamburger)>;

class Order : public std::enable_shared_from_this<Order> {
public:
    Order(net::io_context& io, int id, bool with_onion, OrderHandler handler) :
        io_{io},
        id_{id},
        with_onion_{with_onion},
        handler_{std::move(handler)}
    {}

    // Запускает асинхронное выполнение заказа
    void Execute() {
        logger_.LogMessage("Order has been started."sv);

        // Заказ - сделать бургер.
        // Чтобы сделать бургер, нужно приготовить для бургера ингридиенты.\
        // Приготовить ингридиенты:
        //1. ОБЯЗАТЕЛЬНО: пожарить котлету
        //2. ОПЦИОНАЛЬНО: замариновать лук

        // Жарим котлету
        RoastCutlet(); // синхр.

        // Маринуем лук
        if (with_onion_) {
            MarinadeOnion(); // синхр.
        }
    }

private:
    void RoastCutlet() {
        // Пожарить котлету - означает еще и выполнить в результате жарки котлеты что-то
        logger_.LogMessage("Start roasting cutlet"sv);

        // Выполняем асинхронно лямбду (когда будет ожидание, то выполнится через 1с)
        roast_timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnRoasted(ec);
        }); // асинхр.
    }
    void OnRoasted(sys::error_code ec) {
        // Когда котлета пожарена - запоминаем это! (SetCutletRoasted)
        // Но после любого готового ингридиента, нам требуется проверка: 
        // достаточно ли текущего ингридиента для готовности заказа  (CheckReadiness)
        if (ec) {
            logger_.LogMessage("Roast error : "s + ec.what());
        } else {
            logger_.LogMessage("Cutlet has been roasted."sv);
            hamburger_.SetCutletRoasted(); // Жареная котлета идет сразу в бургер
        }
        CheckReadiness(ec);
    }
    
    void MarinadeOnion() {
        // Замариновать лук - означает еще и выполнить в результате маринования лука что-то
        logger_.LogMessage("Start marinading onion"sv);
        marinade_timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnOnionMarinaded(ec);
        }); // асинхр.
    }
    void OnOnionMarinaded(sys::error_code ec) {
        // Когда лук замаренован - запоминаем это! (onion_marinaded_)
        // Но после любого готового ингридиента, нам требуется проверка: 
        // достаточно ли текущего ингридиента для готовности заказа  (CheckReadiness)
        if (ec) {
            logger_.LogMessage("Marinade onion error: "s + ec.what());
        } else {
            logger_.LogMessage("Onion has been marinaded."sv);
            onion_marinaded_ = true; // Маринованый лук не идет сразу в бургер! (лук не нужен или еще не очередь лука добавляться к бургеру)
        }
        CheckReadiness(ec);
    }

    void CheckReadiness(sys::error_code ec) {
        if (delivered_) {
            // Выходим, если заказ уже доставлен либо клиента уведомили об ошибке
            return;
        }
        if (ec) {
            // В случае ошибки уведомляем клиента о невозможности выполнить заказ
            return Deliver(ec); // синхр.
        }

        // Далее рассматривается случаи, которые возможны после завершения приготовления какого-либо из ингридиентов.
        // ! Добавление лука и маринование лука разные вещи
        // ! Кроме добавления других ингридиентов (AddOnion) возможно еще и упаковка гамбургера (Pack)

        // Отдельно рассматриваем добавления лука в гамбургер!
        // Самое время добавить лук
        if (CanAddOnion()) {
            logger_.LogMessage("Add onion"sv);
            hamburger_.AddOnion(); // синхр.
        }

        // Если все компоненты гамбургера готовы, упаковываем его
        if (IsReadyToPack()) {
            Pack(); // синхр.
        }
    }

    void Deliver(sys::error_code ec) {
        // Защита заказа от повторной доставки
        delivered_ = true;
        // Доставляем гамбургер в случае успеха либо nullptr, если возникла ошибка
        handler_(ec, id_, ec ? nullptr : &hamburger_);
    }

    [[nodiscard]] bool CanAddOnion() const {
        // Почему не добавить проверку на with_onion_ ?

        // Лук можно добавить, если котлета обжарена, лук замаринован, но пока не добавлен
        return hamburger_.IsCutletRoasted() && onion_marinaded_ && !hamburger_.HasOnion();
    }

    [[nodiscard]] bool IsReadyToPack() const {
        // Если котлета обжарена и лук добавлен, как просили, гамбургер можно упаковывать
        return hamburger_.IsCutletRoasted() && (!with_onion_ || hamburger_.HasOnion());
    }

    void Pack() {
        logger_.LogMessage("Packing"sv);

        // Просто потребляем ресурсы процессора в течение 0,5 с.
        auto start = steady_clock::now();
        while (steady_clock::now() - start < 500ms) {
        }

        hamburger_.Pack();
        logger_.LogMessage("Packed"sv);

        // Как только гамбургер упакован: выполняем доставку
        Deliver({});
    }

private:
    net::io_context& io_; // Для асинхронных операций
    int id_; // id заказа
    bool with_onion_; // нужен лук
    OrderHandler handler_;
    Logger logger_{std::to_string(id_)};

private:
    Timer roast_timer_{io_, 1s};
    Timer marinade_timer_{io_, 2s};

private:    
    Hamburger hamburger_;
    bool onion_marinaded_ = false; // лук маринован?
    bool delivered_ = false; // Заказ доставлен?
};

class Restaurant {
public:
    explicit Restaurant(net::io_context& io)
        : io_(io) {
    }

    int MakeHamburger(bool with_onion, OrderHandler handler) {
        const int order_id = ++next_order_id_;

        // Объект класса Order тоже асинхронно что-то делает (из-за io_)
        // Не удаляется объект класса Order, т.к. внутри используется shared_ptr на тот же самый объект Order
        // (продление жизни, за счет подачи в лямбду)
        std::make_shared<Order>(io_, order_id, with_onion, handler)->Execute();

        // Возвращаем номер заказа (круто - номер заказа получим раньше, чем бургер сделается)
        return order_id;
    }

private:
    net::io_context& io_;
    int next_order_id_ = 0;
};

int main() {
    // Движок, в котором выполняются асинхронные операции (io.run() - останавливает текущий поток и запускает все асинх. операции и ждет завершения всех асинхр. операций)
    net::io_context io;

    // Асинхронный ресторан (из-за io)
    Restaurant restaurant{io};
    Logger logger{"main"s};

    struct OrderResult {
        sys::error_code ec;
        Hamburger hamburger;
    };

    // Хотим получать наши бургеры в orders
    std::unordered_map<int, OrderResult> orders;
    auto handle_result = [&orders](sys::error_code ec, int id, Hamburger* h) {
        orders.emplace(id, OrderResult{ec, ec ? Hamburger{} : *h});
    };

    // Создаем асинхронно 2 бургера
    const int id1 = restaurant.MakeHamburger(false, handle_result);
    const int id2 = restaurant.MakeHamburger(true, handle_result);

    // До вызова io.run() никакие заказы не выполняются
    assert(orders.empty());
    io.run();

    // После вызова io.run() все заказы быть выполнены
    assert(orders.size() == 2u);
    {
        // Проверяем заказ без лука
        const auto& o = orders.at(id1);
        assert(!o.ec);
        assert(o.hamburger.IsCutletRoasted());
        assert(o.hamburger.IsPacked());
        assert(!o.hamburger.HasOnion());
    }
    {
        // Проверяем заказ с луком
        const auto& o = orders.at(id2);
        assert(!o.ec);
        assert(o.hamburger.IsCutletRoasted());
        assert(o.hamburger.IsPacked());
        assert(o.hamburger.HasOnion());
    }
}
