/**
 * @file main.cpp
 * @brief Client application for the secure chat.
 */

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
    /** @brief Reset all attributes. */
    const std::string RESET   = "\\033[0m";
    /** @brief Red color. */
    const std::string RED     = "\\033[31m";
    /** @brief Green color. */
    const std::string GREEN   = "\\033[32m";
    /** @brief Yellow color. */
    const std::string YELLOW  = "\\033[33m";
    /** @brief Blue color. */
    const std::string BLUE    = "\\033[34m";
    /** @brief Magenta color. */
    const std::string MAGENTA = "\\033[35m";
    /** @brief Cyan color. */
    const std::string CYAN    = "\\033[36m";
    /** @brief Bold text. */
    const std::string BOLD    = "\\033[1m";
}

/**
 * @brief Main class for the chat client.
 *
 * Handles connection to the server, sending and receiving messages,
 * and user interface.
 */
class ChatClient {
private:
    // Connection related
    asio::io_context& io_context_;          ///< Boost.Asio I/O context.
    ssl::context ssl_context_;              ///< Boost.Asio SSL context.
    std::shared_ptr<ssl::stream<tcp::socket>> ssl_socket_; ///< SSL socket for communication.
    std::string server_ip_;                 ///< IP address of the server.
    std::string port_;                      ///< Port number of the server.
    std::string username_;                  ///< Username of the client.

    // State
    /**
     * @brief Represents the current state of the client.
     */
    enum class ClientState {
        DISCONNECTED, ///< Client is not connected to the server.
        CONNECTED,    ///< Client is connected but not yet registered.
        REGISTERED,   ///< Client is registered with a username.
        CHATTING      ///< Client is actively chatting with another user.
    };

    std::atomic<ClientState> state_{ClientState::DISCONNECTED}; ///< Current state of the client.
    std::string selected_user_;                                 ///< Username of the currently selected chat partner.
    std::vector<std::string> user_list_;                        ///< List of available users.
    std::mutex user_list_mutex_;                                ///< Mutex to protect access to the user list.

    // Message history for each user
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> chat_history_; ///< Stores chat history with other users.
    std::mutex chat_history_mutex_;                                                      ///< Mutex to protect access to chat history.

    // Input/output mutex to prevent garbled console
    std::mutex console_mutex_;                                  ///< Mutex to synchronize console output.

    // Flag to indicate if we should quit
    std::atomic<bool> quit_{false};                             ///< Flag to signal application termination.

    // Threads
    std::thread io_thread_;                                     ///< Thread for Boost.Asio I/O operations.
    std::thread read_thread_;                                   ///< Thread for reading messages from the server.
    std::thread input_thread_;                                  ///< Thread for handling user input.

public:
    /**
     * @brief Constructs a ChatClient object.
     * @param io_context The Boost.Asio I/O context.
     * @param server_ip The IP address of the server.
     * @param port The port number of the server.
     */
    ChatClient(asio::io_context& io_context, const std::string& server_ip, const std::string& port)
        : io_context_(io_context),
          ssl_context_(ssl::context::tlsv12_client),
          server_ip_(server_ip),
          port_(port) {

        // Set SSL options
        ssl_context_.set_verify_mode(ssl::verify_none);
    }

    /**
     * @brief Destroys the ChatClient object.
     *
     * Ensures all threads are joined and resources are cleaned up.
     */
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

    /**
     * @brief Starts the chat client.
     *
     * Establishes a connection to the server and starts the necessary threads.
     * @return True if the client started successfully, false otherwise.
     */
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

    /**
     * @brief Runs the main loop of the client.
     *
     * Initializes the connection if needed, clears the screen, and starts the input loop.
     * This function will block until the client quits.
     */
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
    /**
     * @brief Clears the terminal screen.
     *
     * Uses ANSI escape codes to clear the screen and move the cursor to the top-left.
     */
    void clear_screen() {
        std::cout << "\033[2J\033[1;1H";  // ANSI escape code to clear screen
    }

    /**
     * @brief Prints the header for the chat application to the console.
     *
     * Displays the application title and current logged-in user if applicable.
     * Uses console mutex for thread-safe output.
     */
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

    /**
     * @brief Displays the login screen and prompts the user for a username.
     *
     * After getting the username, it calls `register_user()` to register with the server.
     */
    void show_login_screen() {
        print_header();
        std::cout << Color::YELLOW << "Enter your username: " << Color::RESET;
        std::cin >> username_;

        register_user();

        // Wait a bit for registration to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    /**
     * @brief Displays the user selection screen.
     *
     * Shows a list of available users and allows the client to select one to chat with
     * or refresh the user list. This loop continues as long as the client is in the
     * `REGISTERED` state and has not quit.
     */
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

    /**
     * @brief Displays the chat screen for the selected user.
     *
     * Shows the chat history with the selected user and allows the client to send messages
     * or go back to the user selection screen. This loop continues as long as the client
     * is in the `CHATTING` state with the `selected_user_` and has not quit.
     */
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

    /**
     * @brief Main input loop for the client.
     *
     * Manages the client's UI state by calling the appropriate screen display function
     * (`show_login_screen`, `show_user_selection_screen`, `show_chat_screen`) based on the
     * current `state_`. This loop continues until `quit_` is true.
     */
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

    /**
     * @brief Registers the client's username with the server.
     *
     * Sends a `REGISTER` message to the server with the current `username_`.
     */
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

    /**
     * @brief Requests the current list of users from the server.
     *
     * Sends a `LIST` message to the server.
     */
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

    /**
     * @brief Sends a chat message to the `selected_user_`.
     * @param content The text content of the message to send.
     *
     * If the content or `selected_user_` is empty, the function does nothing.
     * Otherwise, it creates a `MESSAGE` type `chat::Message`, adds it to the local
     * `chat_history_`, and sends it to the server.
     */
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

    /**
     * @brief Main loop for reading messages from the server.
     *
     * Continuously tries to read data from the `ssl_socket_`. If a message is received,
     * it's deserialized and processed by `process_message()`.
     * Handles disconnection and read errors. This loop runs until the client state
     * is `DISCONNECTED` or `quit_` is true.
     */
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

    /**
     * @brief Processes a received message from the server.
     * @param message The `chat::Message` object received from the server.
     *
     * Handles different message types:
     * - `LIST`: Updates the local `user_list_`, changes state to `REGISTERED` if needed,
     *           and displays any system message content.
     * - `MESSAGE`: Adds the message to `chat_history_`. If currently chatting with the
     *              sender, refreshes the chat screen. Otherwise, displays a notification.
     * - `SYSTEM`: Displays the system message content.
     */
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

/**
 * @brief Main function for the chat client.
 * @param argc Argument count.
 * @param argv Argument vector. Expects server IP and port as arguments.
 * @return 0 on successful execution, 1 on error (e.g., incorrect arguments).
 */
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
