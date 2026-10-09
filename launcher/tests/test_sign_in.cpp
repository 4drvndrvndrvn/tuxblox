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
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace fs = std::filesystem;

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void writeFile(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << text;
}

int main() {
    using namespace tuxblox;

    const std::string dir = (fs::temp_directory_path() / "tuxblox_test_sign_in_dir").string();
    fs::remove_all(dir);

    // Nothing saved is the same as empty credentials, not an error.
    {
        SignInCredentials c = loadSignIn(dir);
        assert(c.username.empty());
        assert(c.password.empty());
        assert(signInFilePath(dir) == dir + "/sign-in");
    }

    // The unreserved set passes through; everything else, including every
    // byte of a multi-byte character, becomes %XX in upper case.
    {
        assert(percentEncode("Az09-._~") == "Az09-._~");
        assert(percentEncode("a b'\"%\\") == "a%20b%27%22%25%5C");
        assert(percentEncode("\xC3\xA4") == "%C3%A4");
        assert(percentEncode("") == "");
    }

    // Decoding is the exact reverse, and refuses a '%' that is not followed
    // by two hex digits rather than guessing.
    {
        assert(percentDecode("a%20b%27%22%25%5C") == std::optional<std::string>("a b'\"%\\"));
        assert(percentDecode("%c3%a4") == std::optional<std::string>("\xC3\xA4"));
        assert(!percentDecode("%"));
        assert(!percentDecode("abc%2"));
        assert(!percentDecode("%zz"));
    }

    // What reaches the file is only what the compatibility layer accepts:
    // letters, digits and -._~% after each "=", whatever the password holds.
    {
        const std::string text = renderSignInFile({"user", "</script>'\"\n\\"});
        const size_t at = text.find("password=");
        assert(at != std::string::npos);
        const std::string value = text.substr(at + 9, text.find('\n', at) - at - 9);
        assert(value.find_first_not_of(
                   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~%") == std::string::npos);
    }

    // Round trip, with the characters most likely to break a hand-rolled
    // format: spaces, both quotes, '=', '%', '#', a backslash and non-ASCII.
    {
        const SignInCredentials saved{"someone@example.com", "p\xC3\xA4ss = w0rd #1 '\"%20\\"};
        assert(saveSignIn(dir, saved));

        const SignInCredentials loaded = loadSignIn(dir);
        assert(loaded.username == saved.username);
        assert(loaded.password == saved.password);
    }

    // Readable and writable by the owner only, whatever the umask allows.
    {
        struct stat st {};
        assert(stat(signInFilePath(dir).c_str(), &st) == 0);
        assert((st.st_mode & 0777) == 0600);
        assert(!fs::exists(signInFilePath(dir) + ".tmp"));
    }

    // A save replaces a file another program left world-readable with a new
    // one, rather than writing the password into it.
    {
        writeFile(signInFilePath(dir), "username=x\npassword=y\n");
        chmod(signInFilePath(dir).c_str(), 0644);
        assert(saveSignIn(dir, {"a", "b"}));

        struct stat st {};
        assert(stat(signInFilePath(dir).c_str(), &st) == 0);
        assert((st.st_mode & 0777) == 0600);
        assert(loadSignIn(dir).password == "b");
    }

    // A temporary file left by an interrupted save does not stop the next one.
    {
        writeFile(signInFilePath(dir) + ".tmp", "partial");
        assert(saveSignIn(dir, {"c", "d"}));
        assert(loadSignIn(dir).username == "c");
        assert(!fs::exists(signInFilePath(dir) + ".tmp"));
    }

    // Comments, blank lines, CRLF endings and unknown keys are all tolerated.
    {
        writeFile(signInFilePath(dir), "# note\r\n\r\nversion=2\r\nusername=me\r\npassword=a%20b\r\n");
        const SignInCredentials c = loadSignIn(dir);
        assert(c.username == "me");
        assert(c.password == "a b");
    }

    // One malformed field makes the whole file count as nothing saved --
    // half an account would only earn a failed login.
    {
        writeFile(signInFilePath(dir), "username=me\npassword=bad%zz\n");
        const SignInCredentials c = loadSignIn(dir);
        assert(c.username.empty());
        assert(c.password.empty());
    }

    // Saving two empty fields is how the page clears the account: no file
    // is left with nothing in it.
    {
        assert(saveSignIn(dir, {"e", "f"}));
        assert(saveSignIn(dir, {"", ""}));
        assert(!fs::exists(signInFilePath(dir)));
    }

    // Forgetting deletes the file, and forgetting nothing is still success.
    {
        assert(saveSignIn(dir, {"g", "h"}));
        assert(forgetSignIn(dir));
        assert(!fs::exists(signInFilePath(dir)));
        assert(forgetSignIn(dir));
        assert(loadSignIn(dir).username.empty());
    }

    // The demo account is seeded only where nothing is saved: it never
    // replaces an account the user typed in.
    {
        fs::remove_all(dir);
        assert(seedDemoSignIn(dir));
        SignInCredentials c = loadSignIn(dir);
        assert(c.username == demoSignIn().username);
        assert(c.password == demoSignIn().password);

        assert(saveSignIn(dir, {"mine", "secret"}));
        assert(seedDemoSignIn(dir));
        assert(loadSignIn(dir).username == "mine");
    }

    // The demo username must never be a real account, or every install would
    // send a wrong password for someone. Roblox usernames are letters, digits
    // and at most one underscore -- a hyphen rules it out, and it is not an
    // email address or a phone number either.
    {
        const std::string username = demoSignIn().username;
        assert(username.find('-') != std::string::npos);
        assert(username.find('@') == std::string::npos);
        assert(!demoSignIn().password.empty());
    }

    // The file is written into a directory that does not exist yet, the way a
    // fresh install's would be.
    {
        fs::remove_all(dir);
        assert(saveSignIn(dir, {"i", "j"}));
        assert(readFile(signInFilePath(dir)).find("username=i\n") != std::string::npos);
    }

    fs::remove_all(dir);

    printf("sign_in: all tests passed\n");
    return 0;
}
