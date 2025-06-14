/**
 * @file test_all.cpp
 * @brief Contains all unit tests for the chat application.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "../common/message.hpp"
#include "../common/utils.hpp"        // новая утилита

using namespace chat;

/* ─────── Message ─────── */
/**
 * @brief Test suite for the Message class.
 */
TEST_SUITE("Message") {
    /**
     * @brief Tests the serialization and deserialization round-trip of a Message object.
     */
    TEST_CASE("serialize / deserialize round-trip") {
        Message m;
        m.type      = MessageType::MESSAGE;
        m.sender    = "alice";
        m.recipient = "bob";
        m.content   = "hello";
        m.users     = {"alice", "bob"};

        auto json = m.serialize();
        auto r    = Message::deserialize(json);

        CHECK(r.type      == m.type);
        CHECK(r.sender    == m.sender);
        CHECK(r.recipient == m.recipient);
        CHECK(r.content   == m.content);
        CHECK(r.users     == m.users);
    }

    /**
     * @brief Tests that deserialization handles invalid JSON gracefully.
     */
    TEST_CASE("deserialize handles invalid json") {
        std::string garbage = "{ invalid json }";
        CHECK_NOTHROW(Message::deserialize(garbage));
    }

    /**
     * @brief Tests that the MessageType enum is correctly stored as an integer in JSON.
     */
    TEST_CASE("enum value stored as int") {
        Message m;
        m.type = MessageType::LIST;
        auto j = nlohmann::json::parse(m.serialize());
        CHECK(j["type"].get<int>() == static_cast<int>(MessageType::LIST));
    }
}

/* ─────── Utils ─────── */
/**
 * @brief Test suite for utility functions.
 */
TEST_SUITE("Utils") {
    // helper
    /**
     * @brief Helper function to create a temporary file for testing.
     * @return The path to the created temporary file.
     */
    auto create_temp_file = [] {
        std::string path = "tmp_test_file.txt";
        std::ofstream(path) << "dummy";
        return path;
    };

    /**
     * @brief Tests the file_exists function with an existing file.
     */
    TEST_CASE("file_exists positive") {
        auto p = create_temp_file();
        CHECK(file_exists(p) == true);
        std::remove(p.c_str());
    }

    /**
     * @brief Tests the file_exists function with a non-existent file.
     */
    TEST_CASE("file_exists negative") {
        CHECK(file_exists("definitely_no_file_42.tmp") == false);
    }
}
