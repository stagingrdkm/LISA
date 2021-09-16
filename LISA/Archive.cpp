/*****************************************************************************
* Copyright © 2020 Liberty Global B.V. and its Licensors.
* All rights reserved.
* Licensed by RDK Management, LLC under the terms of the RDK license.
* ============================================================================
* Liberty Global B.V. CONFIDENTIAL AND PROPRIETARY
* ============================================================================
* This file (and its contents) are the intellectual property of Liberty Global B.V.
* It may not be used, copied, distributed or otherwise disclosed in whole or in
* part without the express written permission of Liberty Global B.V.
* The RDK License agreement constitutes express written consent by Liberty Global.
* ============================================================================
* This software is the confidential and proprietary information of Liberty Global B.V.
* ("Confidential Information"). You shall not disclose this source code or
* such Confidential Information and shall use it only in accordance with the
* terms of the license agreement you entered into.
*
* LIBERTY GLOBAL B.V. MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE
* SUITABILITY OF THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT
* LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
* PARTICULAR PURPOSE, OR NON-INFRINGEMENT. LIBERTY GLOBAL B.V. SHALL NOT BE LIABLE FOR
* ANY DAMAGES SUFFERED BY LICENSEE NOR SHALL THEY BE RESPONSIBLE AS A RESULT
* OF USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.
******************************************************************************/

#include "Archive.h"
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
        archive_read_support_compression_gzip(theArchive);

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
            } else if (readHeaderResult != ARCHIVE_OK) {
                std::string message = std::string{} + "error while reading entry " + archive_error_string(theArchive);
                throw ArchiveError(message);
            }

            std::string destPath{destination + archive_entry_pathname(entry)};
            archive_entry_set_pathname(entry, destPath.c_str());

            const char *origHardlink = archive_entry_hardlink(entry);
            if (origHardlink) {
                std::string destPathHardLink{destination + '/' + origHardlink};
                archive_entry_set_hardlink(entry, destPathHardLink.c_str());
            }

            auto extractStatus = archive_read_extract(theArchive, entry, flags);
            if (extractStatus == ARCHIVE_OK) {
                INFO("extracted: ", archive_entry_pathname(entry));
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


