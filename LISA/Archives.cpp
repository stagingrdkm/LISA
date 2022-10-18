/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 Liberty Global Service B.V.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Archives.h"
#include "Debug.h"

#include <archive.h>
#include <archive_entry.h>

#include <memory>

namespace WPEFramework {
namespace Plugin {
namespace LISA {
namespace Archive {

namespace { // anonymous

class Archive
{
public:
    Archive(const std::string& filePath) :
        theArchive{archive_read_new()}
    {
        assert(theArchive);
        archive_read_support_format_tar(theArchive);
        archive_read_support_filter_gzip(theArchive);

        static constexpr int BLOCK_SIZE = 10240;
        if(archive_read_open_filename(theArchive, filePath.c_str(), BLOCK_SIZE) != ARCHIVE_OK) {
            std::string message = std::string{} + "error opening file " + archive_error_string(theArchive);
            throw ArchiveError(message);
        }
        INFO("archive opened ", filePath);
    }

    Archive(const Archive& other) = delete;
    Archive& operator=(const Archive& other) = delete;

    ~Archive()
    {
        if (theArchive) {
            archive_read_close(theArchive);
            archive_read_free(theArchive);
        }
    }

    void extractTo(const std::string& destination)
    {
        struct archive_entry *entry{};

        while (true)
        {
            auto readHeaderResult = archive_read_next_header(theArchive, &entry);

            if (readHeaderResult == ARCHIVE_EOF) {
                INFO("archive read successfully");
                break;
            } else if (readHeaderResult != ARCHIVE_OK && readHeaderResult != ARCHIVE_WARN) {
                std::string message = std::string{} + "error while reading entry " + archive_error_string(theArchive);
                throw ArchiveError(message);
            } else if (readHeaderResult == ARCHIVE_WARN) {
                INFO("Warning while reading entry ", archive_error_string(theArchive));
            }

            std::string destPath{destination + archive_entry_pathname(entry)};
            archive_entry_set_pathname(entry, destPath.c_str());

            const char *origHardlink = archive_entry_hardlink(entry);
            if (origHardlink) {
                std::string destPathHardLink{destination + '/' + origHardlink};
                archive_entry_set_hardlink(entry, destPathHardLink.c_str());
            }

            auto extractStatus = archive_read_extract(theArchive, entry, flags);
            if (extractStatus == ARCHIVE_OK || extractStatus == ARCHIVE_WARN) {
                INFO("extracted: ", archive_entry_pathname(entry));
                if (extractStatus == ARCHIVE_WARN) {
                    INFO("Warning while extracting ", archive_error_string(theArchive));
                }
            } else {
                std::string message = std::string{} + "error while extracting " + archive_error_string(theArchive);
                throw ArchiveError(message);
            }
        }
    }

private:
    static constexpr int flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL
            | ARCHIVE_EXTRACT_FFLAGS;

    struct archive* theArchive{};
};

} // namespace anonymous

void unpack(const std::string& filePath, const std::string& destinationDir)
{
    Archive archive{filePath};
    archive.extractTo(destinationDir);
}

} // namespace Archive
} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework


