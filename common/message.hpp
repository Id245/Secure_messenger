/**
 * @file message.hpp
 * @brief Defines the message structure and types for chat communication.
 */
#pragma once
#include <string>
#include <vector>
#include "json.hpp"

namespace chat {

/**
 * @brief Defines the type of a chat message.
 */
enum class MessageType {
    REGISTER,  /**< Message to register a username with the server. */
    LIST,      /**< Message to request the list of connected users. */
    SELECT,    /**< Message to select a user to chat with. */
    MESSAGE,   /**< A standard chat message. */
    SYSTEM     /**< A system notification or error message. */
};

/**
 * @brief Represents a chat message.
 *
 * This structure is used for communication between the client and server.
 * It can be serialized to and deserialized from JSON.
 */
struct Message {
    MessageType type;                   /**< The type of the message. */
    std::string sender;                 /**< The username of the message sender. */
    std::string recipient;              /**< The username of the message recipient (if applicable). */
    std::string content;                /**< The content of the message. */
    std::vector<std::string> users;     /**< A list of usernames (used for LIST responses). */

    /**
     * @brief Serializes the Message object to a JSON string.
     * @return A JSON string representation of the message.
     */
    std::string serialize() const {
        nlohmann::json j;
        j["type"] = static_cast<int>(type);
        j["sender"] = sender;
        j["recipient"] = recipient;
        j["content"] = content;
        j["users"] = users;
        return j.dump();
    }

    /**
     * @brief Deserializes a Message object from a JSON string.
     * @param json_str The JSON string to parse.
     * @return A Message object.
     */
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
