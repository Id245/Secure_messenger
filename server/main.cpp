#include <boost/asio.hpp>
#include <iostream>

using boost::asio::ip::tcp;

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 1234));

        std::cout << "Server is running on port 1234...\n";

        for (;;) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            std::cout << "Client connected\n";

            while (true) {
                char data[1024];
                boost::system::error_code error;

                size_t length = socket.read_some(boost::asio::buffer(data), error);

                if (error == boost::asio::error::eof || error == boost::asio::error::connection_reset) {
                    std::cout << "Client disconnected\n";
                    break;
                } else if (error) {
                    throw boost::system::system_error(error);
                }

                std::string received(data, length);
                std::cout << "Client: " << received << "\n";

                std::string response = "Server received: " + received;
                boost::asio::write(socket, boost::asio::buffer(response), error);
            }
        }

    } catch (std::exception& e) {
        std::cerr << "Server exception: " << e.what() << "\n";
    }

    return 0;
}
