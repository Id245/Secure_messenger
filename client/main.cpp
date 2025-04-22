#include <boost/asio.hpp>
#include <iostream>
#include <string>

using boost::asio::ip::tcp;

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", "1234");

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        std::cout << "Connected to server. Type 'exit' to quit.\n";

        while (true) {
            std::string msg;
            std::cout << "You: ";
            std::getline(std::cin, msg);

            if (msg == "exit")
                break;

            boost::asio::write(socket, boost::asio::buffer(msg));

            char reply[1024];
            boost::system::error_code error;
            size_t len = socket.read_some(boost::asio::buffer(reply), error);

            if (error) {
                std::cerr << "Receive error: " << error.message() << std::endl;
                break;
            }

            std::cout << "Server: " << std::string(reply, len) << "\n";
        }

    } catch (std::exception& e) {
        std::cerr << "Client exception: " << e.what() << "\n";
    }

    return 0;
}
