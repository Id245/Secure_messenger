#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include <chrono>
#include "../common/message.hpp"

namespace asio = boost::asio;
using asio::ip::tcp;
namespace ssl = asio::ssl;

// ANSI color codes for terminal formatting
namespace Color {
    const std::string RESET   = "\033[0m";
    const std::string RED     = "\033[31m";
    const std::string GREEN   = "\033[32m";
    const std::string YELLOW  = "\033[33m";
    const std::string BLUE    = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN    = "\033[36m";
    const std::string BOLD    = "\033[1m";
}

class ChatClient {
private:
    // Connection related
    asio::io_context& io_context_;
    ssl::context ssl_context_;
    std::shared_ptr<ssl::stream<tcp::socket>> ssl_socket_;
    std::string server_ip_;
    std::string port_;
    std::string username_;

    // State
    enum class ClientState {
        DISCONNECTED,
        CONNECTED,
        REGISTERED,
        CHATTING
    };

    std::atomic<ClientState> state_{ClientState::DISCONNECTED};
    std::string selected_user_;
    std::vector<std::string> user_list_;
    std::mutex user_list_mutex_;

    // Message history for each user
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> chat_history_;
    std::mutex chat_history_mutex_;

    // Input/output mutex to prevent garbled console
    std::mutex console_mutex_;

    // Flag to indicate if we should quit
    std::atomic<bool> quit_{false};

    // Threads
    std::thread io_thread_;
    std::thread read_thread_;
    std::thread input_thread_;

public:
    ChatClient(asio::io_context& io_context, const std::string& server_ip, const std::string& port)
        : io_context_(io_context),
          ssl_context_(ssl::context::tlsv12_client),
          server_ip_(server_ip),
          port_(port) {

        // Set SSL options
        ssl_context_.set_verify_mode(ssl::verify_none);
    }

    ~ChatClient() {
        quit_ = true;

        if (input_thread_.joinable()) {
            input_thread_.join();
        }

        if (read_thread_.joinable()) {
            read_thread_.join();
        }

        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    }

    bool start() {
        try {
            // Connect to server
            tcp::resolver resolver(io_context_);
            auto endpoints = resolver.resolve(server_ip_, port_);

            // Create SSL socket
            ssl_socket_ = std::make_shared<ssl::stream<tcp::socket>>(io_context_, ssl_context_);
            asio::connect(ssl_socket_->lowest_layer(), endpoints);

            ssl_socket_->handshake(ssl::stream_base::client);
            state_ = ClientState::CONNECTED;

            // Start background I/O processing
            io_thread_ = std::thread([this]() { io_context_.run(); });
            read_thread_ = std::thread([this]() { read_loop(); });

            return true;
        }
        catch (std::exception& e) {
            std::cerr << "Connection error: " << e.what() << std::endl;
            return false;
        }
    }

    void run() {
        if (state_ == ClientState::DISCONNECTED) {
            if (!start()) {
                return;
            }
        }

        // Clear screen
        clear_screen();

        // Start user input thread
        input_thread_ = std::thread([this]() { input_loop(); });

        // Wait for quit signal
        input_thread_.join();
    }

private:
    void clear_screen() {
        std::cout << "\033[2J\033[1;1H";  // ANSI escape code to clear screen
    }

    void print_header() {
        std::lock_guard<std::mutex> lock(console_mutex_);
        clear_screen();
        std::cout << Color::CYAN << Color::BOLD << "══════════════════════════════════════════════════" << Color::RESET << std::endl;
        std::cout << Color::CYAN << Color::BOLD << "           SECURE MESSENGER - CHAT APP           " << Color::RESET << std::endl;
        std::cout << Color::CYAN << Color::BOLD << "══════════════════════════════════════════════════" << Color::RESET << std::endl;

        if (state_ == ClientState::REGISTERED || state_ == ClientState::CHATTING) {
            std::cout << Color::GREEN << "Logged in as: " << Color::BOLD << username_ << Color::RESET << std::endl;
        }

        std::cout << Color::CYAN << "──────────────────────────────────────────────────" << Color::RESET << std::endl;
    }

    void show_login_screen() {
        print_header();
        std::cout << Color::YELLOW << "Enter your username: " << Color::RESET;
        std::cin >> username_;

        register_user();

        // Wait a bit for registration to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void show_user_selection_screen() {
        while (state_ == ClientState::REGISTERED && !quit_) {
            print_header();
            std::cout << Color::YELLOW << "Select a user to chat with:" << Color::RESET << std::endl;

            // Display user list
            {
                std::lock_guard<std::mutex> lock(user_list_mutex_);
                int idx = 1;
                for (const auto& user : user_list_) {
                    if (user != username_) { // Don't show self
                        std::cout << Color::CYAN << " " << idx << ": " << Color::RESET
                                  << Color::BOLD << user << Color::RESET << std::endl;
                        idx++;
                    }
                }

                if (idx == 1) {
                    std::cout << Color::RED << " No other users online." << Color::RESET << std::endl;
                }
            }

            std::cout << Color::CYAN << "──────────────────────────────────────────────────" << Color::RESET << std::endl;
            std::cout << Color::YELLOW << "Enter user number or 'r' to refresh: " << Color::RESET;

            std::string choice;
            std::cin >> choice;

            if (choice == "r" || choice == "R") {
                request_user_list();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            else {
                try {
                    int user_idx = std::stoi(choice) - 1;
                    std::lock_guard<std::mutex> lock(user_list_mutex_);

                    // Adjust index to account for skipping self in the display
                    int real_idx = 0;
                    for (const auto& user : user_list_) {
                        if (user != username_) {
                            if (real_idx == user_idx) {
                                selected_user_ = user;
                                state_ = ClientState::CHATTING;
                                break;
                            }
                            real_idx++;
                        }
                    }

                    if (state_ != ClientState::CHATTING) {
                        std::cout << Color::RED << "Invalid selection. Press Enter to try again." << Color::RESET << std::endl;
                        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                        std::cin.get();
                    }
                }
                catch (std::exception&) {
                    std::cout << Color::RED << "Invalid input. Press Enter to try again." << Color::RESET << std::endl;
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    std::cin.get();
                }
            }
        }
    }

    void show_chat_screen() {
        // Clear any existing input
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        while (state_ == ClientState::CHATTING && !quit_) {
            print_header();
            std::cout << Color::MAGENTA << Color::BOLD << "Chatting with: " << selected_user_ << Color::RESET << std::endl;
            std::cout << Color::CYAN << "──────────────────────────────────────────────────" << Color::RESET << std::endl;

            // Display chat history
            {
                std::lock_guard<std::mutex> lock(chat_history_mutex_);
                auto it = chat_history_.find(selected_user_);
                if (it != chat_history_.end()) {
                    for (const auto& msg : it->second) {
                        if (msg.first == username_) {
                            std::cout << Color::GREEN << "You: " << Color::RESET << msg.second << std::endl;
                        } else {
                            std::cout << Color::BLUE << selected_user_ << ": " << Color::RESET << msg.second << std::endl;
                        }
                    }
                }
            }

            std::cout << Color::CYAN << "──────────────────────────────────────────────────" << Color::RESET << std::endl;
            std::cout << "Type a message or '/back' to return to user selection: ";

            std::string message;
            std::getline(std::cin, message);

            if (message == "/back") {
                state_ = ClientState::REGISTERED;
                break;
            }
            else if (!message.empty()) {
                send_message(message);
            }
        }
    }

    void input_loop() {
        while (!quit_) {
            if (state_ == ClientState::CONNECTED) {
                show_login_screen();
            }
            else if (state_ == ClientState::REGISTERED) {
                show_user_selection_screen();
            }
            else if (state_ == ClientState::CHATTING) {
                show_chat_screen();
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    void register_user() {
        chat::Message reg_msg;
        reg_msg.type = chat::MessageType::REGISTER;
        reg_msg.sender = username_;

        try {
            std::string serialized = reg_msg.serialize();
            asio::write(*ssl_socket_, asio::buffer(serialized));
        } catch (std::exception& e) {
            std::cerr << "Failed to register: " << e.what() << std::endl;
        }
    }

    void request_user_list() {
        chat::Message list_msg;
        list_msg.type = chat::MessageType::LIST;
        list_msg.sender = username_;

        try {
            std::string serialized = list_msg.serialize();
            asio::write(*ssl_socket_, asio::buffer(serialized));
        } catch (std::exception& e) {
            std::cerr << "Failed to request user list: " << e.what() << std::endl;
        }
    }

    void send_message(const std::string& content) {
        if (content.empty() || selected_user_.empty()) {
            return;
        }

        chat::Message msg;
        msg.type = chat::MessageType::MESSAGE;
        msg.sender = username_;
        msg.recipient = selected_user_;
        msg.content = content;

        // Add to local chat history
        {
            std::lock_guard<std::mutex> lock(chat_history_mutex_);
            chat_history_[selected_user_].push_back({username_, content});
        }

        // Send to server
        try {
            std::string serialized = msg.serialize();
            asio::write(*ssl_socket_, asio::buffer(serialized));
        } catch (std::exception& e) {
            std::lock_guard<std::mutex> lock(console_mutex_);
            std::cerr << "Failed to send message: " << e.what() << std::endl;
        }
    }

    void read_loop() {
        while (state_ != ClientState::DISCONNECTED && !quit_) {
            try {
                std::array<char, 4096> buffer;
                boost::system::error_code error;

                size_t length = ssl_socket_->read_some(asio::buffer(buffer), error);

                if (error) {
                    if (error == asio::error::eof) {
                        std::lock_guard<std::mutex> lock(console_mutex_);
                        std::cout << "Server closed connection." << std::endl;
                    } else {
                        std::lock_guard<std::mutex> lock(console_mutex_);
                        std::cerr << "Read error: " << error.message() << std::endl;
                    }
                    state_ = ClientState::DISCONNECTED;
                    quit_ = true;
                    break;
                }

                std::string message_str(buffer.data(), length);
                auto message = chat::Message::deserialize(message_str);

                process_message(message);
            }
            catch (std::exception& e) {
                std::lock_guard<std::mutex> lock(console_mutex_);
                std::cerr << "Read error: " << e.what() << std::endl;
                state_ = ClientState::DISCONNECTED;
                quit_ = true;
                break;
            }
        }
    }

    void process_message(const chat::Message& message) {
        if (message.type == chat::MessageType::LIST) {
            // Update user list
            {
                std::lock_guard<std::mutex> lock(user_list_mutex_);
                user_list_ = message.users;
            }

            // Set state to registered if not already
            if (state_ == ClientState::CONNECTED) {
                state_ = ClientState::REGISTERED;
            }

            // Log system message if present
            if (!message.content.empty()) {
                std::lock_guard<std::mutex> lock(console_mutex_);
                std::cout << Color::YELLOW << "[System] " << message.content << Color::RESET << std::endl;
            }
        }
        else if (message.type == chat::MessageType::MESSAGE) {
            // Add message to chat history
            {
                std::lock_guard<std::mutex> lock(chat_history_mutex_);
                chat_history_[message.sender].push_back({message.sender, message.content});
            }

            // If we're currently chatting with this user, refresh the screen
            if (state_ == ClientState::CHATTING && selected_user_ == message.sender) {
                // Refresh the chat screen
                print_header();
                std::cout << Color::MAGENTA << Color::BOLD << "Chatting with: " << selected_user_ << Color::RESET << std::endl;
                std::cout << Color::CYAN << "──────────────────────────────────────────────────" << Color::RESET << std::endl;

                // Display chat history
                {
                    std::lock_guard<std::mutex> lock(chat_history_mutex_);
                    auto it = chat_history_.find(selected_user_);
                    if (it != chat_history_.end()) {
                        for (const auto& msg : it->second) {
                            if (msg.first == username_) {
                                std::cout << Color::GREEN << "You: " << Color::RESET << msg.second << std::endl;
                            } else {
                                std::cout << Color::BLUE << selected_user_ << ": " << Color::RESET << msg.second << std::endl;
                            }
                        }
                    }
                }

                std::cout << Color::CYAN << "──────────────────────────────────────────────────" << Color::RESET << std::endl;
                std::cout << "Type a message or '/back' to return to user selection: ";
            }
            // If we're not chatting with this user, show notification
            else {
                std::lock_guard<std::mutex> lock(console_mutex_);
                std::cout << std::endl << Color::YELLOW << "[New message from " << message.sender << "]" << Color::RESET << std::endl;
            }
        }
        else if (message.type == chat::MessageType::SYSTEM) {
            std::lock_guard<std::mutex> lock(console_mutex_);
            std::cout << Color::YELLOW << "[System] " << message.content << Color::RESET << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <port>\n";
        return 1;
    }

    std::string server_ip = argv[1];
    std::string port = argv[2];

    try {
        asio::io_context io_context;

        ChatClient client(io_context, server_ip, port);
        client.run();

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
