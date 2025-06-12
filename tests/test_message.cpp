#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "../common/message.hpp"
#include "../common/utils.hpp"        // новая утилита

using namespace chat;

/* ─────── Message ─────── */
TEST_SUITE("Message") {
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

    TEST_CASE("deserialize handles invalid json") {
        std::string garbage = "{ invalid json }";
        CHECK_NOTHROW(Message::deserialize(garbage));
    }

    TEST_CASE("enum value stored as int") {
        Message m;
        m.type = MessageType::LIST;
        auto j = nlohmann::json::parse(m.serialize());
        CHECK(j["type"].get<int>() == static_cast<int>(MessageType::LIST));
    }
}

/* ─────── Utils ─────── */
TEST_SUITE("Utils") {
    // helper
    auto create_temp_file = [] {
        std::string path = "tmp_test_file.txt";
        std::ofstream(path) << "dummy";
        return path;
    };

    TEST_CASE("file_exists positive") {
        auto p = create_temp_file();
        CHECK(file_exists(p) == true);
        std::remove(p.c_str());
    }

    TEST_CASE("file_exists negative") {
        CHECK(file_exists("definitely_no_file_42.tmp") == false);
    }
}
