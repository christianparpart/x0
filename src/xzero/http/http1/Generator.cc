// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http1/Generator.h>
#include <xzero/http/HttpDateGenerator.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/http/HttpStatus.h>
#include <xzero/net/EndPointWriter.h>
#include <xzero/io/FileView.h>
#include <xzero/HugeBuffer.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>

namespace xzero {
namespace http {
namespace http1 {

bool isContentForbidden(HttpMethod method) {
  switch (method) {
    case HttpMethod::UNKNOWN_METHOD:
       return false;
    case HttpMethod::OPTIONS:
       return true;
    case HttpMethod::GET:
       return true;
    case HttpMethod::HEAD:
       return true;
    case HttpMethod::POST:
       return false;
    case HttpMethod::PUT:
       return false;
    case HttpMethod::DELETE:
       return true;
    case HttpMethod::TRACE:
       return true;
    case HttpMethod::CONNECT:
    case HttpMethod::PROPFIND:
    case HttpMethod::PROPPATCH:
    case HttpMethod::MKCOL:
    case HttpMethod::COPY:
    case HttpMethod::MOVE:
    case HttpMethod::LOCK:
    case HttpMethod::UNLOCK:
      // a.k.a. TODO: find out for sure (100%)
      return false;
    default:
      logFatal("Unhandled HTTP method");
  }
}

Generator::Generator(EndPointWriter* output)
    : bytesTransmitted_(0),
      contentLength_(Buffer::npos),
      actualContentLength_(0),
      chunked_(false),
      buffer_(),
      writer_(output) {
}

void Generator::reset() {
  bytesTransmitted_ = 0;
  contentLength_ = Buffer::npos;
  actualContentLength_ = 0;
  chunked_ = false;
  buffer_.clear();
}

void Generator::generateRequest(const HttpRequestInfo& info,
                                Buffer&& chunk) {
  generateRequestLine(info);
  generateHeaders(info, isContentForbidden(info.method()));
  flushBuffer();
  generateBody(std::move(chunk));
}

void Generator::generateRequest(const HttpRequestInfo& info,
                                const BufferRef& chunk) {
  generateRequestLine(info);
  generateHeaders(info, isContentForbidden(info.method()));
  flushBuffer();
  generateBody(chunk);
}

void Generator::generateRequest(const HttpRequestInfo& info,
                                FileView&& chunk) {
  generateRequestLine(info);
  generateHeaders(info, isContentForbidden(info.method()));
  flushBuffer();
  generateBody(std::move(chunk));
}

void Generator::generateRequest(const HttpRequestInfo& info,
                                HugeBuffer&& chunk) {
  generateRequestLine(info);
  generateHeaders(info, isContentForbidden(info.method()));
  flushBuffer();
  generateBody(std::move(chunk));
}

void Generator::generateRequest(const HttpRequestInfo& info) {
  generateRequestLine(info);
  generateHeaders(info, isContentForbidden(info.method()));
  flushBuffer();
}

void Generator::generateResponse(const HttpResponseInfo& info,
                                 const BufferRef& chunk) {
  generateResponseInfo(info);
  generateBody(chunk);
  flushBuffer();
}

void Generator::generateResponse(const HttpResponseInfo& info,
                                 Buffer&& chunk) {
  generateResponseInfo(info);
  generateBody(std::move(chunk));
  flushBuffer();
}

void Generator::generateResponse(const HttpResponseInfo& info,
                                 FileView&& chunk) {
  generateResponseInfo(info);
  generateBody(std::move(chunk));
}

void Generator::generateResponse(const HttpResponseInfo& info,
                                 HugeBuffer&& chunk) {
  generateResponseInfo(info);
  generateBody(std::move(chunk));
}

void Generator::generateResponseInfo(const HttpResponseInfo& info) {
  generateResponseLine(info);

  if (static_cast<int>(info.status()) >= 200) {
    generateHeaders(info, isContentForbidden(info.status()));
  } else if (info.status() == HttpStatus::SwitchingProtocols) {
    generateHeaders(info, isContentForbidden(info.status()));
  } else {
    buffer_.push_back("\r\n");
  }

  flushBuffer();
}

void Generator::generateBody(const BufferRef& chunk) {
  if (chunked_) {
    if (chunk.size() > 0) {
      Buffer buf(12);
      buf.printf("%zx\r\n", chunk.size());
      writer_->write(std::move(buf));
      writer_->write(chunk);
      writer_->write(BufferRef("\r\n"));
    }
  } else {
    if (chunk.size() <= remainingContentLength()) {
      actualContentLength_ += chunk.size();
      writer_->write(chunk);
    } else {
      throw std::invalid_argument("chunk");
      // ("HTTP body exceeds the expected content length.");
    }
  }
}

void Generator::generateTrailer(const HeaderFieldList& trailers) {
  if (chunked_) {
    buffer_.push_back("0\r\n");
    for (const HeaderField& header: trailers) {
      buffer_.push_back(header.name());
      buffer_.push_back(": ");
      buffer_.push_back(header.value());
      buffer_.push_back("\r\n");
    }
    buffer_.push_back("\r\n");
  }
  flushBuffer();
}

void Generator::generateBody(Buffer&& chunk) {
  if (chunked_) {
    if (chunk.size() > 0) {
      Buffer buf(12);
      buf.printf("%zx\r\n", chunk.size());
      writer_->write(std::move(buf));
      writer_->write(std::move(chunk));
      writer_->write(BufferRef("\r\n"));
    }
  } else {
    if (chunk.size() <= remainingContentLength()) {
      actualContentLength_ += chunk.size();
      writer_->write(std::move(chunk));
    } else {
      throw std::invalid_argument{"chunk"};
      // "HTTP body exceeds the expected content length.");
    }
  }
}

void Generator::generateBody(FileView&& chunk) {
  if (chunked_) {
    int n;
    char buf[12];

    if (chunk.size() > 0) {
      n = snprintf(buf, sizeof(buf), "%zx\r\n", chunk.size());
      bytesTransmitted_ += n + chunk.size() + 2;
      writer_->write(BufferRef(buf, static_cast<size_t>(n)));
      writer_->write(std::move(chunk));
      writer_->write(BufferRef("\r\n"));
    }
  } else {
    if (chunk.size() <= remainingContentLength()) {
      bytesTransmitted_ += chunk.size();
      actualContentLength_ += chunk.size();
      writer_->write(std::move(chunk));
    } else {
      throw std::invalid_argument{"chunk"};
      // "HTTP body chunk exceeds content length.");
    }
  }
}

void Generator::generateBody(HugeBuffer&& chunk) {
  if (chunk.isBuffered()) {
    generateBody(chunk.getBuffer());
  } else {
    generateBody(chunk.getFileView());
  }
}

void Generator::generateRequestLine(const HttpRequestInfo& info) {
  buffer_.push_back(info.unparsedMethod());
  buffer_.push_back(' ');
  buffer_.push_back(info.unparsedUri());

  switch (info.version()) {
    case HttpVersion::VERSION_0_9:
      buffer_.push_back(" HTTP/0.9\r\n");
      break;
    case HttpVersion::VERSION_1_0:
      buffer_.push_back(" HTTP/1.0\r\n");
      break;
    case HttpVersion::VERSION_1_1:
      buffer_.push_back(" HTTP/1.1\r\n");
      break;
    default:
      throw std::invalid_argument{"info.version"};
  }
}

void Generator::generateResponseLine(const HttpResponseInfo& info) {
  switch (info.version()) {
    case HttpVersion::VERSION_0_9:
      buffer_.push_back("HTTP/0.9 ");
      break;
    case HttpVersion::VERSION_1_0:
      buffer_.push_back("HTTP/1.0 ");
      break;
    case HttpVersion::VERSION_1_1:
      buffer_.push_back("HTTP/1.1 ");
      break;
    default:
      throw std::invalid_argument{"info.version"};
  }

  buffer_.push_back(static_cast<int>(info.status()));

  buffer_.push_back(' ');
  if (info.reason().size() > 0)
    buffer_.push_back(info.reason());
  else
    buffer_.push_back(fmt::format("{}", info.status()));

  buffer_.push_back("\r\n");
}

void Generator::generateHeaders(const HttpInfo& info, bool bodyForbidden) {
  chunked_ = info.hasContentLength() == false || info.hasTrailers();
  contentLength_ = info.contentLength();

  for (const HeaderField& header: info.headers()) {
    // skip pseudo headers (that might have come via HTTP/2)
    if (header.name()[0] != ':') {
      buffer_.push_back(header.name());
      buffer_.push_back(": ");
      buffer_.push_back(header.value());
      buffer_.push_back("\r\n");
    }
  }

  if (!info.trailers().empty()) {
    buffer_.push_back("Trailer: ");
    size_t count = 0;
    for (const HeaderField& trailer: info.trailers()) {
      if (count)
        buffer_.push_back(", ");

      buffer_.push_back(trailer.name());
      count++;
    }
    buffer_.push_back("\r\n");
  }

  if (!bodyForbidden) {
    if (chunked_) {
      buffer_.push_back("Transfer-Encoding: chunked\r\n");
    } else {
      buffer_.push_back("Content-Length: ");
      buffer_.push_back(info.contentLength());
      buffer_.push_back("\r\n");
    }
  }

  buffer_.push_back("\r\n");
}

void Generator::flushBuffer() {
  if (!buffer_.empty()) {
    bytesTransmitted_ += buffer_.size();
    writer_->write(std::move(buffer_));
    buffer_.clear();
  }
}

}  // namespace http1
}  // namespace http
}  // namespace xzero
