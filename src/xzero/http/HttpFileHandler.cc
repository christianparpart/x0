// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpFileHandler.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpRangeDef.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/Random.h>
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

namespace xzero {
namespace http {

// {{{ helper methods
/**
 * Converts a range-spec into real offsets.
 *
 */
inline constexpr std::pair<size_t, size_t> makeOffsets(
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
  static Random rng;
  static const char* map = "0123456789abcdef";
  char buf[16 + 1];

  for (size_t i = 0; i < sizeof(buf) - 1; ++i)
    buf[i] = map[rng.random64() % (sizeof(buf) - 1)];

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
      logFatal("internal bug; unhandled status code");
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

  FileHandle fd;
  if (request->method() == HttpMethod::GET) {
    fd = transferFile->createPosixChannel(FileOpenFlags::Read | FileOpenFlags::NonBlocking);
    if (fd < 0) {
      switch (errno) {
        case EPERM:
        case EACCES:
          return HttpStatus::Forbidden;
        case ENOSYS:
          // TODO: in that case we're prjobably on MemoryFile and we need to read differently
        default:
          RAISE_ERRNO(errno);
      }
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

  if (fd.isOpen()) {  // GET request
#if defined(HAVE_POSIX_FADVISE)
    posix_fadvise(fd, 0, transferFile->size(), POSIX_FADV_SEQUENTIAL);
#endif
    response->write(FileView(std::move(fd), 0, transferFile->size()));
    response->completed();
  } else {
    response->completed();
  }

  // This is the expected response status, even though it may have
  // been overridden (due to internal redirect by setting failure code
  // before invoking this handler).
  return HttpStatus::Ok;
}

static inline uint64_t getUnixMicros(const struct tm& tm) {
  static const auto isLeapYear = [](uint16_t year) {
    if (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0)) {
      return true;
    } else {
      return false;
    }
  };

  static const auto daysInMonth = [=](uint16_t year, uint8_t month) {
    if (month == 2) {
      return (28 + (isLeapYear(year) ? 1 : 0));
    } else {
      return (31 - (month - 1) % 7 % 2);
    }
  };

  uint64_t days = tm.tm_mday - 1;

  for (auto i = 1970; i < tm.tm_year; ++i)
    days += 365 + isLeapYear(i);

  for (auto i = 1; i < tm.tm_mon; ++i)
    days += daysInMonth(tm.tm_year, i);

  uint64_t micros = days * kMicrosPerDay +
         tm.tm_hour * kMicrosPerHour +
         tm.tm_min * kMicrosPerMinute +
         tm.tm_sec * kMicrosPerSecond;

  fmt::print("secs = {}\n", micros / kMicrosPerSecond);

  return micros;
}

static std::optional<UnixTime> parseTime(const std::string& timeStr) {
#if defined(HAVE_STRPTIME)
  static const char* timeFormat = "%a, %d %b %Y %T GMT";

  struct tm tm;
  memset(&tm, 0, sizeof(tm));
  if (strptime(timeStr.c_str(), timeFormat, &tm) == nullptr)
    return std::nullopt;

  const time_t gmt = mktime(&tm);
  const time_t utc = gmt + tm.tm_gmtoff;
  if (tm.tm_isdst == 1)
    return UnixTime((utc - 3600) * kMicrosPerSecond);
  else
    return UnixTime(utc * kMicrosPerSecond);
#else
  return std::nullopt; // TODO: strptime() impl
#endif
}

HttpStatus HttpFileHandler::handleClientCache(const File& transferFile,
                                              HttpRequest* request,
                                              HttpResponse* response) {
  static const char* FMT = "%a, %d %b %Y %T GMT";
  // If-Modified-Since
  do {
    const std::string& value = request->headers().get("If-Modified-Since");
    if (value.empty())
      continue;

    std::optional<UnixTime> dt = parseTime(value);
    if (!dt)
      continue;

    if (transferFile.mtime() > dt.value())
      continue;

    return HttpStatus::NotModified;
  } while (0);

  // If-Unmodified-Since
  do {
    const std::string& value = request->headers().get("If-Unmodified-Since");
    if (value.empty())
      continue;

    std::optional<UnixTime> dt = parseTime(value);
    if (!dt)
      continue;

    if (transferFile.mtime() <= dt.value())
      continue;

    return HttpStatus::PreconditionFailed;
  } while (0);

  // If-Match
  do {
    const std::string& value = request->headers().get("If-Match");
    if (value.empty())
      continue;

    if (value == "*")
      continue;

    // XXX: on static files we probably don't need the token-list support
    if (value == transferFile.etag())
      continue;

    return HttpStatus::PreconditionFailed;
  } while (0);

  // If-None-Match
  do {
    const std::string& value = request->headers().get("If-None-Match");
    if (value.empty())
      continue;

    // XXX: on static files we probably don't need the token-list support
    if (value != transferFile.etag())
      continue;

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

bool HttpFileHandler::handleRangeRequest(const File& transferFile,
                                         FileHandle& fd,
                                         HttpRequest* request,
                                         HttpResponse* response) {
  const bool isHeadReq = fd.isClosed();
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
        if (!last) {
          response->write(FileView(fd, offsets.first, partLength));
        } else {
          response->write(FileView(std::move(fd), offsets.first, partLength));
        }
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

    if (fd.isOpen()) {
#if defined(HAVE_POSIX_FADVISE)
      posix_fadvise(fd, offsets.first, length, POSIX_FADV_SEQUENTIAL);
#endif
      response->write(FileView(std::move(fd), offsets.first, length));
    }
  }

  response->completed();
  return true;
}

} // namespace http
} // namespace xzero
