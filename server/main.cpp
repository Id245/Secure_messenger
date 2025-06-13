#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <mutex>
#include <algorithm>
#include "../common/message.hpp"
#include "../common/utils.hpp"          // ← новая строка

namespace asio = boost::asio;
using asio::ip::tcp;
namespace ssl = asio::ssl;

class ChatServer {
private:
    asio::io_context& io_context_;
    ssl::context ssl_context_;
    tcp::acceptor acceptor_;
    unsigned short port_;

    // Maps usernames to SSL sockets
    std::map<std::string, std::shared_ptr<ssl::stream<tcp::socket>>> user_connections_;
    std::mutex users_mutex_;

public:
    ChatServer(asio::io_context& io_context, unsigned short port)
        : io_context_(io_context),
          ssl_context_(ssl::context::tlsv12_server),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
          port_(port) {

        // Set up SSL context
        ssl_context_.set_options(
            ssl::context::default_workarounds |
            ssl::context::no_sslv2 |
            ssl::context::no_sslv3 |
            ssl::context::single_dh_use
        );

        // Load certificate and private key
        ssl_context_.use_certificate_chain_file("server.crt");
        ssl_context_.use_private_key_file("server.key", ssl::context::pem);
    }

    void start() {
        std::cout << "Secure chat server running on port " << port_ << "\n";
        accept_connection();
    }

private:
    void accept_connection() {
        auto socket = std::make_shared<tcp::socket>(io_context_);

        acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& error) {
            if (!error) {
                std::cout << "New connection from " << socket->remote_endpoint().address().to_string() << "\n";

                auto ssl_socket = std::make_shared<ssl::stream<tcp::socket>>(std::move(*socket), ssl_context_);

                // Perform SSL handshake
                ssl_socket->async_handshake(ssl::stream_base::server,
                    [this, ssl_socket](const boost::system::error_code& error) {
                        if (!error) {
                            std::cout << "SSL handshake successful\n";
                            handle_register(ssl_socket);
                        } else {
                            std::cerr << "SSL handshake failed: " << error.message() << "\n";
                        }
                    });
            } else {
                std::cerr << "Accept error: " << error.message() << "\n";
            }

            // Accept next connection
            accept_connection();
        });
    }

    void handle_register(std::shared_ptr<ssl::stream<tcp::socket>> ssl_socket) {
        auto buffer = std::make_shared<std::array<char, 1024>>();

        asio::async_read(*ssl_socket, asio::buffer(*buffer),
            asio::transfer_at_least(1),
            [this, ssl_socket, buffer](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (!error) {
                    std::string message_str(buffer->data(), bytes_transferred);
                    auto message = chat::Message::deserialize(message_str);

                    if (message.type == chat::MessageType::REGISTER) {
                        std::string username = message.sender;

                        // Check if username is already taken
                        std::lock_guard<std::mutex> lock(users_mutex_);
                        if (user_connections_.find(username) != user_connections_.end()) {
                            // Send error
                            chat::Message response;
                            response.type = chat::MessageType::SYSTEM;
                            response.content = "Username already taken. Please reconnect and choose another name.";

                            asio::async_write(*ssl_socket, asio::buffer(response.serialize()),
                                [](const boost::system::error_code&, std::size_t) {});

                            return;
                        }

                        // Register the new user
                        user_connections_[username] = ssl_socket;
                        std::cout << "User registered: " << username << "\n";

                        // Send confirmation and user list
                        chat::Message response;
                        response.type = chat::MessageType::LIST;
                        response.content = "Welcome " + username + "! You are now registered.";

                        for (const auto& user : user_connections_) {
                            response.users.push_back(user.first);
                        }

                        asio::async_write(*ssl_socket, asio::buffer(response.serialize()),
                            [this, ssl_socket, username](const boost::system::error_code& error, std::size_t) {
                                if (!error) {
                                    // Start listening for messages from this user
                                    listen_for_messages(ssl_socket, username);

                                    // Broadcast updated user list to all users
                                    broadcast_user_list();
                                }
                            });
                    }
                } else if (error != asio::error::eof) {
                    std::cerr << "Read error: " << error.message() << "\n";
                }
            });
    }

    void listen_for_messages(std::shared_ptr<ssl::stream<tcp::socket>> ssl_socket, const std::string& username) {
        auto buffer = std::make_shared<std::array<char, 4096>>();

        asio::async_read(*ssl_socket, asio::buffer(*buffer),
            asio::transfer_at_least(1),
            [this, ssl_socket, buffer, username](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (!error) {
                    std::string message_str(buffer->data(), bytes_transferred);
                    auto message = chat::Message::deserialize(message_str);

                    if (message.type == chat::MessageType::LIST) {
                        // Send updated user list
                        chat::Message response;
                        response.type = chat::MessageType::LIST;

                        std::lock_guard<std::mutex> lock(users_mutex_);
                        for (const auto& user : user_connections_) {
                            response.users.push_back(user.first);
                        }

                        asio::async_write(*ssl_socket, asio::buffer(response.serialize()),
                            [this, ssl_socket, username](const boost::system::error_code& error, std::size_t) {
                                if (!error) {
                                    // Continue listening
                                    listen_for_messages(ssl_socket, username);
                                }
                            });
                    }
                    else if (message.type == chat::MessageType::MESSAGE) {
                        std::cout << "Message from " << username << " to " << message.recipient << ": " << message.content << "\n";

                        // Forward the message to the recipient
                        std::lock_guard<std::mutex> lock(users_mutex_);
                        auto it = user_connections_.find(message.recipient);

                        if (it != user_connections_.end()) {
                            asio::async_write(*(it->second), asio::buffer(message.serialize()),
                                [](const boost::system::error_code& error, std::size_t) {
                                    if (error) {
                                        std::cerr << "Failed to deliver message: " << error.message() << "\n";
                                    }
                                });
                        }

                        // Continue listening for more messages from this user
                        listen_for_messages(ssl_socket, username);
                    }
                }
                else {
                    // Client disconnected or error
                    std::cout << "User " << username << " disconnected: " << error.message() << "\n";

                    {
                        std::lock_guard<std::mutex> lock(users_mutex_);
                        user_connections_.erase(username);
                    }

                    // Broadcast updated user list
                    broadcast_user_list();
                }
            });
    }

    void broadcast_user_list() {
        chat::Message user_list;
        user_list.type = chat::MessageType::LIST;
        user_list.sender = "SERVER";

        {
            std::lock_guard<std::mutex> lock(users_mutex_);
            for (const auto& user : user_connections_) {
                user_list.users.push_back(user.first);
            }
        }

        std::string serialized = user_list.serialize();

        std::lock_guard<std::mutex> lock(users_mutex_);
        for (const auto& user : user_connections_) {
            asio::async_write(*(user.second), asio::buffer(serialized),
                [username = user.first](const boost::system::error_code& error, std::size_t) {
                    if (error) {
                        std::cerr << "Failed to send user list to " << username << ": " << error.message() << "\n";
                    }
                });
        }
    }
};

int main(int argc, char* argv[]) {
    unsigned short port = 8443; // Default SSL port

    if (argc >= 2) {
        try {
            port = static_cast<unsigned short>(std::stoi(argv[1]));
        } catch (const std::exception& e) {
            std::cerr << "Invalid port number: " << e.what() << "\n";
            std::cerr << "Using default port " << port << "\n";
        }
    }

    try {
        if (!file_exists("server.crt") || !file_exists("server.key")) {
            std::cerr << "Certificate files not found. Please generate them first.\n";
            std::cerr << "Run: openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes -subj '/CN=localhost'\n";
            return 1;
        }

        asio::io_context io_context;

        ChatServer server(io_context, port);
        server.start();

        io_context.run();

    } catch (std::exception& e) {
        std::cerr << "Server exception: " << e.what() << "\n";
    }

    return 0;
}
