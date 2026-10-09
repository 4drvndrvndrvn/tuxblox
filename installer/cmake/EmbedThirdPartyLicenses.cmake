# TuxBlox - Linux Compatibility Layer for the Roblox Engine
# Copyright (C) 2026 TuxBlox Developers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

# Embeds license texts for third-party dependencies compiled directly into
# the installer binary (nlohmann/json) and by the product as a whole, so copyright_file.cpp
# can write proper attribution into the installed product's COPYRIGHT.txt.
# Mirrors FetchFont.cmake's fetch-or-locate + embed pattern.
#
# nlohmann/json is vendored as a single header with no accompanying LICENSE
# file, so its license is fetched separately, pinned to the exact same v3.11.3
# tag vendor.sh already pins json.hpp to.
#
# stb_image.h's dual-license text lives inline in that (large, mostly-code)
# header rather than in a separate file, so it isn't handled here -- it's
# hardcoded directly in copyright_file.cpp instead, with a comment noting
# which pinned commit it was copied from.

set(JSON_LICENSE_URL "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/LICENSE.MIT")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(JSON_LICENSE_TXT_PATH "${GENERATED_DIR}/json-LICENSE.MIT")
# Kept beside this file rather than fetched: www.gnu.org refuses connections from
# many hosted build machines, GitHub's among them. A copy of the text Wine ships
# (third_party_licenses/wine/COPYING.LIB), since this build only sees its own folder.
set(LGPL21_LICENSE_TXT_PATH "${CMAKE_SOURCE_DIR}/cmake/lgpl-2.1.txt")
set(JSON_LICENSE_HEADER_PATH "${GENERATED_DIR}/json_license_txt.h")
set(LGPL21_LICENSE_HEADER_PATH "${GENERATED_DIR}/lgpl21_license_txt.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${JSON_LICENSE_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${JSON_LICENSE_URL} -DDEST=${JSON_LICENSE_TXT_PATH}
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${JSON_LICENSE_TXT_PATH} -DOUTPUT=${JSON_LICENSE_HEADER_PATH}
            -DSYMBOL=kJsonLicenseTxt
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding nlohmann/json license text"
    VERBATIM
)

add_custom_command(
    OUTPUT ${LGPL21_LICENSE_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${LGPL21_LICENSE_TXT_PATH} -DOUTPUT=${LGPL21_LICENSE_HEADER_PATH}
            -DSYMBOL=kLgpl21LicenseTxt
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    DEPENDS ${LGPL21_LICENSE_TXT_PATH}
    COMMENT "Embedding LGPLv2.1 license text (bundled GTK stack)"
    VERBATIM
)

add_custom_target(generate_thirdparty_license_headers DEPENDS
    ${JSON_LICENSE_HEADER_PATH} ${LGPL21_LICENSE_HEADER_PATH})
