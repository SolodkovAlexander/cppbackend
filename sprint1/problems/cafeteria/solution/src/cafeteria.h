#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <atomic>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;
using namespace std::chrono;
using namespace std::literals;
using Timer = net::steady_timer;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class HotDogOrder : public std::enable_shared_from_this<HotDogOrder>
{
public:
    HotDogOrder(net::io_context &io,
                int id,
                std::shared_ptr<Bread> bread,
                std::shared_ptr<Sausage> sausage,
                std::shared_ptr<GasCooker> gas_cooker,
                HotDogHandler handler) : io_{io}, id_{id}, bread_{std::move(bread)}, sausage_{std::move(sausage)}, gas_cooker_{gas_cooker}, handler_{std::move(handler)}
    {
    }

    // Запускает асинхронное выполнение заказа
    void Execute()
    {
        net::post(io_, [self = shared_from_this()]
                  { self->StartBake(); });
        net::post(io_, [self = shared_from_this()]
                  { self->StartFry(); });
    }

    void StartFry()
    {
        sausage_->StartFry(*(gas_cooker_), [self = shared_from_this()]()
                           { self->FrySausage(); });
    }
    void FrySausage()
    {
        sausage_fry_timer_.expires_after(HotDog::MIN_SAUSAGE_COOK_DURATION);
        sausage_fry_timer_.async_wait([self = shared_from_this()](sys::error_code ec)
                                      { self->FinishFrySausage(); }); // асинхр.
    }
    void FinishFrySausage()
    {
        net::post(strand_, [self = shared_from_this()]
                  { 
            self->sausage_->StopFry();
            self->CheckReadiness(); });
    }

    void StartBake()
    {
        bread_->StartBake(*(gas_cooker_), [self = shared_from_this()]()
                          { self->BakeBread(); });
    }
    void BakeBread()
    {
        bread_bake_timer_.expires_after(HotDog::MIN_BREAD_COOK_DURATION);
        bread_bake_timer_.async_wait([self = shared_from_this()](sys::error_code ec)
                                     { self->FinishBakeBread(); }); // асинхр.
    }
    void FinishBakeBread()
    {
        net::post(strand_, [self = shared_from_this()]
                  { 
            self->bread_->StopBaking();
            self->CheckReadiness(); });
    }

private:
    void CheckReadiness()
    {
        if (delivered_ || !bread_->IsCooked() || !sausage_->IsCooked())
        {
            return;
        }

        delivered_ = true;
        try
        {
            handler_(Result<HotDog>(HotDog{id_, sausage_, bread_}));
        }
        catch (std::invalid_argument e)
        {
            handler_(Result<HotDog>(std::make_exception_ptr(e)));
        }
        catch (...)
        {
            handler_(Result<HotDog>(std::make_exception_ptr("Unknown error"s)));
        }
    }

private:
    net::io_context &io_;
    int id_;
    std::shared_ptr<Bread> bread_;
    std::shared_ptr<Sausage> sausage_;
    std::shared_ptr<GasCooker> gas_cooker_;
    HotDogHandler handler_;

private:
    Timer bread_bake_timer_{io_};
    net::strand<net::io_context::executor_type> strand_{net::make_strand(io_)};
    Timer sausage_fry_timer_{io_};
    bool delivered_ = false;
};

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria
{
public:
    explicit Cafeteria(net::io_context &io)
        : io_{io}
    {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler)
    {
        net::post(order_strand_, [this, handler = std::move(handler)]
                  {
            const int order_id = ++next_order_id_;
            std::make_shared<HotDogOrder>(io_, order_id, store_.GetBread(), store_.GetSausage(), gas_cooker_, handler)->Execute(); });
    }

private:
    std::atomic_int counter_ = 0; // для проверки, есть ли состояние гонки между потоками (без strand_ будет гонка!)
    net::io_context &io_;
    net::strand<net::io_context::executor_type> order_strand_{net::make_strand(io_)};
    // Используется для создания ингредиентов хот-дога
    Store store_;
    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    // Используйте её для приготовления ингредиентов хот-дога.
    // Плита создаётся с помощью make_shared, так как GasCooker унаследован от
    // enable_shared_from_this.
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
    int next_order_id_ = 0;
};
