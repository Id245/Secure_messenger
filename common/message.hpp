#pragma once
#include <string>
#include <vector>
#include "json.hpp"

namespace chat {

enum class MessageType {
    REGISTER,  // Register username
    LIST,      // Get user list
    SELECT,    // Select user to chat with
    MESSAGE,   // Send a message
    SYSTEM     // System notification
};

struct Message {
    MessageType type;
    std::string sender;
    std::string recipient;
    std::string content;
    std::vector<std::string> users;  // For LIST responses

    // Convert to JSON string
    std::string serialize() const {
        nlohmann::json j;
        j["type"] = static_cast<int>(type);
        j["sender"] = sender;
        j["recipient"] = recipient;
        j["content"] = content;
        j["users"] = users;
        return j.dump();
    }

    // Parse from JSON string
    static Message deserialize(const std::string& json_str) {
        Message msg;
        try {
            auto j = nlohmann::json::parse(json_str);
            msg.type = static_cast<MessageType>(j["type"].get<int>());
            msg.sender = j["sender"].get<std::string>();
            msg.recipient = j["recipient"].get<std::string>();
            msg.content = j["content"].get<std::string>();
            msg.users = j["users"].get<std::vector<std::string>>();
        }
        catch (std::exception& e) {
            // Handle parsing error
        }
        return msg;
    }
};

}  // namespace chat
