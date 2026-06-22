// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Unit tests for the shared TOML credentials-file loader
// (exchange::rest::read_toml_credentials). All file I/O targets a unique temp
// directory written by the test itself — no network, no real key files.

#include "exchange/common/credentials_file.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>  // getpid

using exchange::rest::read_toml_credentials;
namespace fs = std::filesystem;

namespace {

// RAII unique temp directory under the system temp path; removed on teardown.
class TempDir {
public:
    TempDir() {
        static std::atomic<int> counter{0};
        path_ = fs::temp_directory_path() /
                ("cryptocogs_cred_" + std::to_string(::getpid()) + "_" +
                 std::to_string(counter.fetch_add(1)));
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    TempDir(const TempDir&)            = delete;
    TempDir& operator=(const TempDir&) = delete;

    std::string str() const { return path_.string(); }
    const fs::path& path() const { return path_; }

    // Write `content` to <dir>/<name>.
    void write(const std::string& name, const std::string& content) const {
        std::ofstream f(path_ / name);
        f << content;
    }

private:
    fs::path path_;
};

} // namespace

TEST(ReadTomlCredentials, ReturnsRequestedKeysInOrder) {
    TempDir dir;
    dir.write("default",
              "api_key = \"KEY123\"\n"
              "api_secret = \"SECRET456\"\n"
              "passphrase = \"PASS789\"\n");

    // default_dir is ignored when an explicit location is given.
    const auto v = read_toml_credentials("default", ".ignored", dir.str(),
                                         {"api_key", "passphrase", "api_secret"});
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], "KEY123");
    EXPECT_EQ(v[1], "PASS789");
    EXPECT_EQ(v[2], "SECRET456");
}

TEST(ReadTomlCredentials, ResolvesUnderHomeWhenLocationEmpty) {
    TempDir home;
    fs::create_directories(home.path() / ".myexch");
    {
        std::ofstream f(home.path() / ".myexch" / "default");
        f << "api_key = \"K\"\napi_secret = \"S\"\n";
    }

    const char* saved = ::getenv("HOME");
    ::setenv("HOME", home.str().c_str(), 1);
    const auto v = read_toml_credentials("default", ".myexch", "",
                                         {"api_key", "api_secret"});
    if (saved) ::setenv("HOME", saved, 1); else ::unsetenv("HOME");

    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], "K");
    EXPECT_EQ(v[1], "S");
}

TEST(ReadTomlCredentials, ThrowsWhenFileNotFound) {
    TempDir dir;  // exists, but the file does not
    EXPECT_THROW(read_toml_credentials("nonexistent", ".x", dir.str(), {"api_key"}),
                 std::runtime_error);
}

TEST(ReadTomlCredentials, ThrowsOnMalformedToml) {
    TempDir dir;
    dir.write("bad", "api_key = \"unterminated\n");  // missing closing quote
    EXPECT_THROW(read_toml_credentials("bad", ".x", dir.str(), {"api_key"}),
                 std::runtime_error);
}

TEST(ReadTomlCredentials, ThrowsOnMissingKey) {
    TempDir dir;
    dir.write("partial", "api_key = \"K\"\n");
    EXPECT_THROW(
        read_toml_credentials("partial", ".x", dir.str(), {"api_key", "api_secret"}),
        std::runtime_error);
}

TEST(ReadTomlCredentials, ThrowsOnNonStringValue) {
    TempDir dir;
    dir.write("typed", "api_key = 12345\n");  // integer, not a string
    EXPECT_THROW(read_toml_credentials("typed", ".x", dir.str(), {"api_key"}),
                 std::runtime_error);
}

TEST(ReadTomlCredentials, ThrowsOnEmptyStringValue) {
    TempDir dir;
    dir.write("empty", "api_key = \"\"\n");
    EXPECT_THROW(read_toml_credentials("empty", ".x", dir.str(), {"api_key"}),
                 std::runtime_error);
}

TEST(ReadTomlCredentials, ThrowsWhenHomeUnsetAndLocationEmpty) {
    const char* saved = ::getenv("HOME");
    ::unsetenv("HOME");
    EXPECT_THROW(read_toml_credentials("default", ".x", "", {"api_key"}),
                 std::runtime_error);
    if (saved) ::setenv("HOME", saved, 1);
}
