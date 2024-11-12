#pragma once
#include <cassert>
#include <optional>

using namespace std::literals;

class TV {
public:
    constexpr static int MIN_CHANNEL = 1;
    constexpr static int MAX_CHANNEL = 99;

    /*
     * Возвращает информацию о том, включен телевизор или нет.
     */
    [[nodiscard]] bool IsTurnedOn() const noexcept {
        return is_turned_on_;
    }

    /*
     * Возвращает номер выбранного канала или std::nullopt, если телевизор выключен.
     */
    [[nodiscard]] std::optional<int> GetChannel() const noexcept {
        return is_turned_on_ ? std::optional{channel_} : std::nullopt;
    }

    /*
     * Включает телевизор, если он был выключен. Если телевизор уже включен, ничего не делает.
     * При включении выбирается тот номер канала, который был выбран перед последним выключением.
     * При самом первом включении выбирает канал #1.
     */
    void TurnOn() noexcept {
        if (!is_turned_on_) {
            is_turned_on_ = true;
        }
    }

    /*
     * Выключает телевизор, если он был включен. Если телевизор уже выключен, ничего не делает.
     */
    void TurnOff() noexcept {
        if (is_turned_on_) {
            is_turned_on_ = false;
        }
    }

    /*
     * Выбирает канал channel.
     * Ранее выбранный канал запоминается и может быть восстановлен методом SelectLastViewedChannel.
     * Если номер канала совпадает с ранее выбранным каналом, метод ничего не делает.
     * Если телевизор выключен, выбрасывает исключение std::logic_error.
     * Если номер канала за пределами диапазона MIN_CHANNEL, MAX_CHANNEL, выбрасывает out_of_range.
     */
    void SelectChannel(int channel) {
        if (!is_turned_on_) {
            throw std::logic_error("TV is off"s);
        }
        if (channel < MIN_CHANNEL || channel > MAX_CHANNEL) {
            throw std::out_of_range("Invalid channel"s);
        }
        if (channel_ == channel) {
            return;
        }

        last_viewed_channel_ = channel;
        channel_ = channel;
    }

    /*
     * Выбирает номер канала, который был выбран перед последним вызовом SelectChannel.
     * Многократный последовательный вызов SelectLastViewedChannel переключает два последних выбранных канала.
     * Если телевизор выключен, выбрасывает исключение std::logic_error.
     */
    void SelectLastViewedChannel() {
        if (!is_turned_on_) {
            throw std::logic_error("TV is off"s);
        }
        std::swap(channel_, last_viewed_channel_);
    }

private:
    bool is_turned_on_ = false;
    int channel_ = 1;
    int last_viewed_channel_ = 1;
};