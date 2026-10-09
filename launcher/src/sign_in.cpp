// TuxBlox - Linux Compatibility Layer for the Roblox Engine
// Copyright (C) 2026 TuxBlox Developers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "sign_in.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

bool isUnreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' ||
           c == '_' || c == '~';
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// write() may stop short of the whole buffer, and is interrupted by signals.
bool writeAll(int fd, const std::string& text) {
    size_t done = 0;
    while (done < text.size()) {
        const ssize_t n = ::write(fd, text.data() + done, text.size() - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        done += static_cast<size_t>(n);
    }
    return true;
}

} // namespace

std::string signInFilePath(const std::string& installDir) {
    return installDir + "/sign-in";
}

std::string percentEncode(const std::string& text) {
    static const char kHex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(text.size() * 3);
    for (const char ch : text) {
        const auto c = static_cast<unsigned char>(ch);
        if (isUnreserved(c)) {
            encoded += ch;
        } else {
            encoded += '%';
            encoded += kHex[c >> 4];
            encoded += kHex[c & 0xF];
        }
    }
    return encoded;
}

std::optional<std::string> percentDecode(const std::string& text) {
    std::string decoded;
    decoded.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '%') {
            decoded += text[i];
            continue;
        }
        if (i + 2 >= text.size()) return std::nullopt;
        const int high = hexValue(text[i + 1]);
        const int low = hexValue(text[i + 2]);
        if (high < 0 || low < 0) return std::nullopt;
        decoded += static_cast<char>((high << 4) | low);
        i += 2;
    }
    return decoded;
}

std::string renderSignInFile(const SignInCredentials& credentials) {
    return "# The Roblox account TuxBlox signs Studio in with. Readable only by you.\n"
           "# Turn off \"Sign in automatically\" in the launcher, or delete this file, to forget it.\n"
           "username=" +
           percentEncode(credentials.username) + "\n" + "password=" + percentEncode(credentials.password) + "\n";
}

SignInCredentials loadSignIn(const std::string& installDir) {
    SignInCredentials credentials;
    try {
        std::ifstream file(signInFilePath(installDir), std::ios::binary);
        if (!file) return credentials;

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;
            const size_t equals = line.find('=');
            if (equals == std::string::npos) continue;
            const std::string key = line.substr(0, equals);
            const std::optional<std::string> value = percentDecode(line.substr(equals + 1));
            // One bad field makes the whole file untrustworthy -- signing in
            // with half an account would only earn a failed login.
            if (!value) return SignInCredentials{};
            if (key == "username") credentials.username = *value;
            if (key == "password") credentials.password = *value;
        }
        return credentials;
    } catch (...) {
        return SignInCredentials{};
    }
}

bool saveSignIn(const std::string& installDir, const SignInCredentials& credentials) {
    if (credentials.username.empty() && credentials.password.empty()) return forgetSignIn(installDir);

    std::error_code ec;
    fs::create_directories(installDir, ec);

    const std::string path = signInFilePath(installDir);
    const std::string temporary = path + ".tmp";
    // A leftover from an interrupted save could have been created with any
    // mode by anything; O_EXCL below refuses to write into one, so it goes.
    ::unlink(temporary.c_str());

    // The mode is set at creation rather than tightened afterwards, so the
    // password is never on disk in a file anyone else could open, not even
    // for the moment between the two calls.
    const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR);
    if (fd < 0) return false;

    // No fsync: the launcher saves on every keystroke, from the UI thread.
    // The rename is what keeps a reader from seeing half a file, and that
    // holds without one.
    const bool written = writeAll(fd, renderSignInFile(credentials));
    const bool closed = ::close(fd) == 0;
    if (!written || !closed || std::rename(temporary.c_str(), path.c_str()) != 0) {
        ::unlink(temporary.c_str());
        return false;
    }
    return true;
}

bool forgetSignIn(const std::string& installDir) {
    std::error_code ec;
    fs::remove(signInFilePath(installDir), ec);
    return !ec;
}

} // namespace tuxblox
