#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <iostream>
namespace net = boost::asio;
int main(){ net::io_context ioc; std::cout << "asio ok" << std::endl; (void)ioc; return 0; }
