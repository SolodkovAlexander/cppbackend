#include <boost/log/trivial.hpp>

#include <string_view>
#include <thread>
#include <iostream>
#include<stdio.h> 

using namespace std::literals;

void fff() {
    BOOST_LOG_TRIVIAL(trace) << "xxxxx";
}

int main() {
    fff();
    printf("address of function main() is :%p\n", fff);    

    void (*pf)();
    pf = fff;
    std::cout << "cout << pf is " << pf << std::endl;
    auto dd = &fff;
    std::cout << *dd;
    std::cout << std::hex << std::this_thread::get_id() << std::endl; 
    BOOST_LOG_TRIVIAL(trace) << std::this_thread::get_id();

    auto ss = new int(13);
    int bbb = 13;
    auto ddd = &::boost::log::trivial::logger::get();
    
    BOOST_LOG_TRIVIAL(trace) << ddd;

    BOOST_LOG_TRIVIAL(trace) << "Сообщение уровня trace"sv;
    BOOST_LOG_TRIVIAL(debug) << "Сообщение уровня debug"sv;
    BOOST_LOG_TRIVIAL(info) << "Сообщение уровня info"sv;
    BOOST_LOG_TRIVIAL(warning) << "Сообщение уровня warning"sv;
    BOOST_LOG_TRIVIAL(error) << "Сообщение уровня error"sv;
    BOOST_LOG_TRIVIAL(fatal) << "Сообщение уровня fatal"sv;
}