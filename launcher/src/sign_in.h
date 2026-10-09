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

#pragma once
#include <optional>
#include <string>

namespace tuxblox {

// The Roblox account Studio's login page is filled in with when
// Settings::autoSignIn is on.
//
// Kept in a file of its own rather than in settings.json: settings.json is
// the file people paste into bug reports, and this one holds a password. It
// is written readable by its owner only, and nothing else in TuxBlox ever
// prints what is in it.
//
// The compatibility layer reads the same file, from inside Studio's process
// (compat/wine/dlls/webview2loader/unixlib.c), so its format is a contract
// between the two:
//
//     # comment lines and blank lines are skipped
//     username=<percent-encoded UTF-8>
//     password=<percent-encoded UTF-8>
//
// Percent-encoded so that what the layer reads is nothing but letters,
// digits and "-._~%", which it can place inside a JavaScript string as-is
// and leave to decodeURIComponent -- no quoting or Unicode handling on that
// side at all. It is not encryption and does not pretend to be.
struct SignInCredentials {
    std::string username;
    std::string password;
};

// What the launcher sets so the compatibility layer can find the file. Only
// the path travels through the environment, never the account itself: the
// environment is inherited by everything Roblox starts.
inline constexpr const char* kSignInFileEnvVar = "TUXBLOX_SIGN_IN_FILE";

// installDir + "/sign-in".
std::string signInFilePath(const std::string& installDir);

// Every byte outside A-Z a-z 0-9 - . _ ~ (RFC 3986's unreserved set) as %XX,
// upper-case hex. decodeURIComponent reads it back exactly.
std::string percentEncode(const std::string& text);
// The reverse. Empty when `text` holds a '%' not followed by two hex digits.
std::optional<std::string> percentDecode(const std::string& text);

// The file's text for `credentials`.
std::string renderSignInFile(const SignInCredentials& credentials);

// Never throws: a missing, unreadable or malformed file gives back empty
// credentials, which is the same as none being saved.
SignInCredentials loadSignIn(const std::string& installDir);

// Writes the file, readable and writable by its owner only, replacing any
// earlier one in a single rename so the layer never reads half of it.
// Saving two empty fields deletes the file instead. False if it could not
// be written.
bool saveSignIn(const std::string& installDir, const SignInCredentials& credentials);

// Deletes the file. True when no file is left, including when there was none.
bool forgetSignIn(const std::string& installDir);

// A made-up account, so a fresh install shows automatic sign-in working with
// nothing to set up first: Studio's login page fills itself in, Log In is
// pressed, and Roblox answers that the details are wrong. It cannot be anyone's
// real account -- Roblox usernames may not contain a hyphen -- so the wrong
// password it sends never counts against a real person. Replaced by typing a
// real account in Settings > Roblox account.
SignInCredentials demoSignIn();

// Saves demoSignIn() when no account is saved yet, and leaves a saved one
// alone. True when a sign-in file is there afterwards.
bool seedDemoSignIn(const std::string& installDir);

} // namespace tuxblox
