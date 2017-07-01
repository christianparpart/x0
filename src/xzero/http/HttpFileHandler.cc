// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpFileHandler.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpRangeDef.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/io/File.h>
#include <xzero/io/FileView.h>
#include <xzero/UnixTime.h>
#include <xzero/Tokenizer.h>
#include <xzero/Buffer.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if 1 //!defined(NDEBUG)
#define TRACE(msg...) logTrace("http.FileHandler", msg)
#else
#define TRACE(level, msg...) do {} while (0)
#endif

namespace xzero {
namespace http {

// {{{ helper methods
/**
 * converts a range-spec into real offsets.
 *
 * \todo Mark this fn as \c constexpr as soon Ubuntu's LTS default compiler
 * supports it (14.04)
 */
inline /*constexpr*/ std::pair<size_t, size_t> makeOffsets(
    const std::pair<size_t, size_t>& p, size_t actualSize) {
  return p.first == HttpRangeDef::npos
             ? std::make_pair(actualSize - p.second,
                              actualSize - 1)  // last N bytes
             : p.second == HttpRangeDef::npos && p.second > actualSize
                   ? std::make_pair(p.first, actualSize - 1)  // from fixed N to
                                                              // the end of file
                   : std::make_pair(p.first, p.second);       // fixed range
}

/**
 * generates a boundary tag.
 *
 * \return a value usable as boundary tag.
 */
static std::string generateDefaultBoundaryID() {
  static const char* map = "0123456789abcdef";
  char buf[16 + 1];

  for (size_t i = 0; i < sizeof(buf) - 1; ++i)
    buf[i] = map[random() % (sizeof(buf) - 1)];

  buf[sizeof(buf) - 1] = '\0';

  return std::string(buf);
}
// }}}

HttpFileHandler::HttpFileHandler()
    : HttpFileHandler(std::bind(&generateDefaultBoundaryID)) {
}

HttpFileHandler::HttpFileHandler(std::function<std::string()> generateBoundaryID)
    : generateBoundaryID_(generateBoundaryID) {
}

HttpFileHandler::~HttpFileHandler() {
}

HttpStatus HttpFileHandler::handle(HttpRequest* request,
                                   HttpResponse* response,
                                   std::shared_ptr<File> transferFile) {
  if (!transferFile->isRegular())
    return HttpStatus::NotFound;

  HttpStatus status = handleClientCache(*transferFile, request, response);
  switch (status) {
    case HttpStatus::Undefined:
      break;
    case HttpStatus::NotModified:
      // 304
      response->setStatus(status);
      response->completed();
      return status;
    case HttpStatus::PreconditionFailed:
      // 412
      return status;
    default:
      RAISE(InternalError); // internal bug; unhandled status code
  }

  switch (transferFile->errorCode()) {
    case 0:
      break;
    case ENOENT:
      return HttpStatus::NotFound;
    case EACCES:
    case EPERM:
      return HttpStatus::Forbidden;
    default:
      RAISE_ERRNO(transferFile->errorCode());
  }

  int fd = -1;
  if (request->method() == HttpMethod::GET) {
    fd = transferFile->createPosixChannel(File::Read | File::NonBlocking);
    if (fd < 0) {
      if (errno != EPERM && errno != EACCES)
        RAISE_ERRNO(errno);

      return HttpStatus::Forbidden;
    }
  } else if (request->method() != HttpMethod::HEAD) {
    return HttpStatus::MethodNotAllowed;
  }

  response->addHeader("Allow", "GET, HEAD");
  response->addHeader("Last-Modified", transferFile->lastModified());
  response->addHeader("ETag", transferFile->etag());

  if (handleRangeRequest(*transferFile, fd, request, response))
    return HttpStatus::PartialContent;

  // XXX Only set status code to 200 (Ok) when the status code hasn't been set
  // previously yet. This may occur due to an internal redirect.
  if (!isError(response->status()))
    response->setStatus(HttpStatus::Ok);

  response->addHeader("Accept-Ranges", "bytes");
  response->addHeader("Content-Type", transferFile->mimetype());

  response->setContentLength(transferFile->size());

  if (fd >= 0) {  // GET request
#if defined(HAVE_POSIX_FADVISE)
    posix_fadvise(fd, 0, transferFile->size(), POSIX_FADV_SEQUENTIAL);
#endif
    response->write(FileView(fd, 0, transferFile->size(), true));
    response->completed();
  } else {
    response->completed();
  }

  // This is the expected response status, even though it may have
  // been overridden (due to internal redirect by setting failure code
  // before invoking this handler).
  return HttpStatus::Ok;
}

HttpStatus HttpFileHandler::handleClientCache(const File& transferFile,
                                              HttpRequest* request,
                                              HttpResponse* response) {
  static const char* timeFormat = "%a, %d %b %Y %H:%M:%S GMT";

  // If-None-Match
  do {
    const std::string& value = request->headers().get("If-None-Match");
    if (value.empty()) continue;

    // XXX: on static files we probably don't need the token-list support
    if (value != transferFile.etag()) continue;

    return HttpStatus::NotModified;
  } while (0);

  // If-Modified-Since
  do {
    const std::string& value = request->headers().get("If-Modified-Since");
    if (value.empty()) continue;

    UnixTime dt(UnixTime::parseString(value, timeFormat).get());

    if (transferFile.mtime() > dt.unixtime()) continue;

    return HttpStatus::NotModified;
  } while (0);

  // If-Match
  do {
    const std::string& value = request->headers().get("If-Match");
    if (value.empty()) continue;

    if (value == "*") continue;

    // XXX: on static files we probably don't need the token-list support
    if (value == transferFile.etag()) continue;

    return HttpStatus::PreconditionFailed;
  } while (0);

  // If-Unmodified-Since
  do {
    const std::string& value = request->headers().get("If-Unmodified-Since");
    if (value.empty()) continue;

    UnixTime dt(UnixTime::parseString(value, timeFormat).get());

    if (transferFile.mtime() <= dt.unixtime()) continue;

    return HttpStatus::PreconditionFailed;
  } while (0);

  return HttpStatus::Undefined;
}

/**
 * Retrieves the number of digits of a positive number.
 */
static size_t numdigits(size_t number) {
  size_t result = 0;

  do {
    result++;
    number /= 10;
  } while (number != 0);

  return result;
}

bool HttpFileHandler::handleRangeRequest(const File& transferFile, int fd,
                                         HttpRequest* request,
                                         HttpResponse* response) {
  const bool isHeadReq = fd < 0;
  BufferRef range_value(request->headers().get("Range"));
  HttpRangeDef range;

  // if no range request or range request was invalid (by syntax) we fall back
  // to a full response
  if (range_value.empty() || !range.parse(range_value))
    return false;

  std::string ifRangeCond = request->headers().get("If-Range");
  if (!ifRangeCond.empty()
        && !equals(ifRangeCond, transferFile.etag())
        && !equals(ifRangeCond, transferFile.lastModified()))
    return false;

  response->setStatus(HttpStatus::PartialContent);

  size_t numRanges = range.size();
  if (numRanges > 1) {
    // generate a multipart/byteranged response, as we've more than one range to
    // serve

    std::string boundary(generateBoundaryID_());
    size_t contentLength = 0;

    // precompute final content-length
    for (size_t i = 0; i < numRanges; ++i) {
      // add ranged chunk length
      const auto offsets = makeOffsets(range[i], transferFile.size());
      const size_t partLength = 1 + offsets.second - offsets.first;

      const size_t headerLen = sizeof("\r\n--") - 1
                             + boundary.size()
                             + sizeof("\r\nContent-Type: ") - 1
                             + transferFile.mimetype().size()
                             + sizeof("\r\nContent-Range: bytes ") - 1
                             + numdigits(offsets.first)
                             + sizeof("-") - 1
                             + numdigits(offsets.second)
                             + sizeof("/") - 1
                             + numdigits(transferFile.size())
                             + sizeof("\r\n\r\n") - 1;

      contentLength += headerLen + partLength;
    }

    // add trailer length
    const size_t trailerLen = sizeof("\r\n--") - 1
                            + boundary.size()
                            + sizeof("--\r\n") - 1;
    contentLength += trailerLen;

    // populate response info
    response->setContentLength(contentLength);
    response->addHeader("Content-Type",
                        "multipart/byteranges; boundary=" + boundary);

    // populate body
    for (size_t i = 0; i != numRanges; ++i) {
      const std::pair<size_t, size_t> offsets(
          makeOffsets(range[i], transferFile.size()));

      if (offsets.second < offsets.first) { // FIXME why did I do this here?
        response->setStatus(HttpStatus::PartialContent);
        response->completed();
        return true;
      }

      const size_t partLength = 1 + offsets.second - offsets.first;

      Buffer buf;
      buf.push_back("\r\n--");
      buf.push_back(boundary);
      buf.push_back("\r\nContent-Type: ");
      buf.push_back(transferFile.mimetype());

      buf.push_back("\r\nContent-Range: bytes ");
      buf.push_back(offsets.first);
      buf.push_back("-");
      buf.push_back(offsets.second);
      buf.push_back("/");
      buf.push_back(transferFile.size());
      buf.push_back("\r\n\r\n");

      if (!isHeadReq) {
        bool last = i + 1 == numRanges;
        response->write(std::move(buf));
        response->write(FileView(fd, offsets.first, partLength, last));
      }
    }

    Buffer buf;
    buf.push_back("\r\n--");
    buf.push_back(boundary);
    buf.push_back("--\r\n");
    response->write(std::move(buf));
  } else {  // generate a simple (single) partial response
    std::pair<size_t, size_t> offsets(
        makeOffsets(range[0], transferFile.size()));

    if (offsets.second < offsets.first) {
      response->sendError(HttpStatus::RequestedRangeNotSatisfiable);
      return true;
    }

    response->addHeader("Content-Type", transferFile.mimetype());

    size_t length = 1 + offsets.second - offsets.first;
    response->setContentLength(length);

    char cr[128];
    snprintf(cr, sizeof(cr), "bytes %zu-%zu/%zu", offsets.first, offsets.second,
             transferFile.size());
    response->addHeader("Content-Range", cr);

    if (fd >= 0) {
#if defined(HAVE_POSIX_FADVISE)
      posix_fadvise(fd, offsets.first, length, POSIX_FADV_SEQUENTIAL);
#endif
      response->write(FileView(fd, offsets.first, length, true));
    }
  }

  response->completed();
  return true;
}

} // namespace http
} // namespace xzero
