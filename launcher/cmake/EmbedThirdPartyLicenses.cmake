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

# Embeds the license texts copyright_file.cpp needs to write proper
# attribution into the installed product's COPYRIGHT.txt. Mirrors
# installer/cmake/EmbedThirdPartyLicenses.cmake.
#
# The set of texts is deliberately the *product's*, not this binary's: the
# launcher and the installer both write the same ~/.tuxblox/COPYRIGHT.txt and
# whichever ran last wins, so both must be able to emit identical content --
# see the comment at the top of copyright_file.cpp.
#
# stb_image.h's dual-license text is hardcoded in copyright_file.cpp rather than
# fetched here: stb keeps its text inline in the (large, mostly-code) header.

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
