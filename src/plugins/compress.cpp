// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/* plugin type: filter
 *
 * description:
 *     Dynamically compresses response content stream.
 *     Supported algorithms: deflate, gzip, bzip2.
 *
 * setup API:
 *     string[] compress.types = ['text/html', 'texxt/css',
 *                                'text/plain', 'application/xml',
 *                                'application/xhtml+xml'];
 *     int compress.level = 9;
 *     int compress.min = 64 bytes;
 *     int compress.max = 128 mbyte;
 *
 * request processing API:
 *     none
 */

#include <x0d/XzeroPlugin.h>
#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <xzero/HttpRangeDef.h>
#include <base/io/CompressFilter.h>
#include <base/Tokenizer.h>
#include <base/strutils.h>
#include <base/Types.h>
#include <x0d/sysconfig.h>

#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <cstdlib>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

using namespace xzero;
using namespace flow;
using namespace base;

/**
 * \ingroup plugins
 * \brief serves static files from server's local filesystem to client.
 */
class compress_plugin : public x0d::XzeroPlugin {
 private:
  std::unordered_map<std::string, int> contentTypes_;
  int level_;
  long long minSize_;
  long long maxSize_;

  bool containsMime(const std::string& value) const {
    return contentTypes_.find(value) != contentTypes_.end();
  }

 public:
  compress_plugin(x0d::XzeroDaemon* d, const std::string& name)
      : x0d::XzeroPlugin(d, name),
        contentTypes_(),             // no types
        level_(9),                   // best compression
        minSize_(256),               // 256 byte
        maxSize_(128 * 1024 * 1024)  // 128 MB
  {
    contentTypes_["text/html"] = 0;
    contentTypes_["text/css"] = 0;
    contentTypes_["text/plain"] = 0;
    contentTypes_["application/xml"] = 0;
    contentTypes_["application/xhtml+xml"] = 0;

    onPostProcess(
        std::bind(&compress_plugin::postProcess, this, std::placeholders::_1));

    setupFunction("compress.types", &compress_plugin::setup_types,
                  flow::FlowType::StringArray);
    setupFunction("compress.level", &compress_plugin::setup_level,
                  flow::FlowType::Number);
    setupFunction("compress.min", &compress_plugin::setup_minsize,
                  flow::FlowType::Number);
    setupFunction("compress.max", &compress_plugin::setup_maxsize,
                  flow::FlowType::Number);
  }

 private:
  void setup_types(flow::vm::Params& args) {
    contentTypes_.clear();

    const auto& x = args.getStringArray(1);

    for (int i = 0, e = args.size(); i != e; ++i) {
      contentTypes_[x[i].str()] = 0;
    }
  }

  void setup_level(flow::vm::Params& args) {
    level_ = args.getInt(1);
    level_ = std::min(std::max(level_, 0), 10);
  }

  void setup_minsize(flow::vm::Params& args) { minSize_ = args.getInt(1); }

  void setup_maxsize(flow::vm::Params& args) { maxSize_ = args.getInt(1); }

 private:
  void postProcess(HttpRequest* in) {
    if (in->responseHeaders.contains("Content-Encoding"))
      return;  // do not double-encode content

    long long size = 0;
    if (in->responseHeaders.contains("Content-Length"))
      size = std::atoll(in->responseHeaders["Content-Length"].c_str());

    bool chunked = in->responseHeaders["Transfer->Encoding"] == "chunked";

    if (size < minSize_ && !(size <= 0 && chunked)) return;

    if (size > maxSize_) return;

    if (!containsMime(in->responseHeaders["Content-Type"])) return;

    if (BufferRef r = in->requestHeader("Accept-Encoding")) {
      auto items =
          Tokenizer<BufferRef, BufferRef>::tokenize(r, ", ");

      if (std::find(items.begin(), items.end(), "bzip2") != items.end()) {
        in->responseHeaders.push_back("Content-Encoding", "bzip2");
        in->outputFilters.push_back(std::make_shared<BZip2Filter>(level_));
      } else if (std::find(items.begin(), items.end(), "gzip") != items.end()) {
        in->responseHeaders.push_back("Content-Encoding", "gzip");
        in->outputFilters.push_back(std::make_shared<GZipFilter>(level_));
      } else if (std::find(items.begin(), items.end(), "deflate") !=
                 items.end()) {
        in->responseHeaders.push_back("Content-Encoding", "deflate");
        in->outputFilters.push_back(
            std::make_shared<DeflateFilter>(level_));
      } else {
        return;
      }

      // response might change according to Accept-Encoding
      if (!in->responseHeaders.contains("Vary"))
        in->responseHeaders.push_back("Vary", "Accept-Encoding");
      else
        in->responseHeaders.append("Vary", ",Accept-Encoding");

      // removing content-length implicitely enables chunked encoding
      in->responseHeaders.remove("Content-Length");
    }
  }
};

X0D_EXPORT_PLUGIN(compress)
