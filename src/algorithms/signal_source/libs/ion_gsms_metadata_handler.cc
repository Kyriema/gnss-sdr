/*!
 * \file ion_gsms_metadata_handler.cc
 * \brief Build instances of IONGSMSFileSource as needed given a list of stream ids
 * \author Víctor Castillo Agüero, 2024. victorcastilloaguero(at)gmail.com
 *
 * -----------------------------------------------------------------------------
 *
 * GNSS-SDR is a Global Navigation Satellite System software-defined receiver.
 * This file is part of GNSS-SDR.
 *
 * Copyright (C) 2010-2024  (see AUTHORS file for a list of contributors)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * -----------------------------------------------------------------------------
 */

#include "ion_gsms.h"
#include <cstdlib>
#if USE_GLOG_AND_GFLAGS
#include <glog/logging.h>
#else
#include <absl/log/log.h>
#endif

IONGSMSMetadataHandler::IONGSMSMetadataHandler(const std::string& metadata_filepath)
    : metadata_filepath_(metadata_filepath)
{
    load_metadata();
}

const std::string& IONGSMSMetadataHandler::metadata_filepath() const
{
    return metadata_filepath_;
}

void IONGSMSMetadataHandler::load_metadata()
{
    try
        {
            GnssMetadata::XmlProcessor xml_proc;
            if (!xml_proc.Load(metadata_filepath_.c_str(), false, metadata_))
                {
                    LOG(WARNING) << "Could not load XML metadata file " << metadata_filepath_;
                    std::cerr << "Could not load XML metadata file " << metadata_filepath_ << std::endl;
                    std::cout << "GNSS-SDR program ended.\n";
                    exit(1);
                }
        }
    catch (GnssMetadata::ApiException& e)
        {
            LOG(WARNING) << "API Exception while loading XML metadata file: " << std::to_string(e.Error());
            std::cerr << "Could not load XML metadata file " << metadata_filepath_ << " : " << std::to_string(e.Error()) << std::endl;
            std::cout << "GNSS-SDR program ended.\n";
            exit(1);
        }
    catch (std::exception& e)
        {
            LOG(WARNING) << "Exception while loading XML metadata file: " << e.what();
            std::cerr << "Could not load XML metadata file " << metadata_filepath_ << " : " << e.what() << std::endl;
            std::cout << "GNSS-SDR program ended.\n";
            exit(1);
        }
}

std::vector<IONGSMSFileSource::sptr> IONGSMSMetadataHandler::make_stream_sources(const std::vector<std::string>& stream_ids) const
{
    std::vector<IONGSMSFileSource::sptr> sources{};
    for (const auto& file : metadata_.Files())
        {
            for (const auto& lane : metadata_.Lanes())
                {
                    if (lane.Id() == file.Lane().Id())
                        {
                            for (const auto& block : lane.Blocks())
                                {
                                    bool block_done = false;
                                    for (const auto& chunk : block.Chunks())
                                        {
                                            for (const auto& lump : chunk.Lumps())
                                                {
                                                    for (const auto& stream : lump.Streams())
                                                        {
                                                            bool found = false;
                                                            for (const auto& stream_id : stream_ids)
                                                                {
                                                                    if (stream_id == stream.Id())
                                                                        {
                                                                            found = true;
                                                                            break;
                                                                        }
                                                                }
                                                            if (found)
                                                                {
                                                                    auto source = gnss_make_shared<IONGSMSFileSource>(
                                                                        metadata_filepath_,
                                                                        file,
                                                                        block,
                                                                        stream_ids);

                                                                    sources.push_back(source);

                                                                    // This file source will take care of any other matching streams in this block
                                                                    // We can skip the rest of this block
                                                                    block_done = true;
                                                                    break;
                                                                }
                                                        }

                                                    if (block_done)
                                                        {
                                                            break;
                                                        }
                                                }
                                            if (block_done)
                                                {
                                                    break;
                                                }
                                        }
                                }
                            break;
                        }
                }
        }

    return sources;
}
