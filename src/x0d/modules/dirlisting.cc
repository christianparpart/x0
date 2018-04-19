// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "dirlisting.h"
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/MediaRange.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/File.h>
#include <xzero/JsonWriter.h>
#include <xzero/logging.h>

using namespace xzero;
using namespace xzero::http;

namespace x0d {

class OutputFormatter {
 public:
  virtual ~OutputFormatter() {}

  virtual void generateHeader(const std::string& path) = 0;
  virtual void generateEntry(std::shared_ptr<File> file) = 0;
  virtual void generateTrailer() = 0;
};

class CsvFormatter : public OutputFormatter { // {{{
 public:
  HttpResponse* response_;
  Buffer buffer_;

  explicit CsvFormatter(HttpResponse* resp)
      : response_(resp), buffer_() {}

  void generateHeader(const std::string& path) override {
    buffer_.push_back("mtime,size,mimetype,filename\n");
  }

  void generateEntry(std::shared_ptr<File> file) override {
    buffer_.push_back(file->mtime().unixtime());
    buffer_.push_back(',');
    buffer_.push_back(file->isDirectory() ? 0 : file->size());
    buffer_.push_back(",\"");
    buffer_.push_back(file->mimetype());
    buffer_.push_back("\",\"");
    buffer_.push_back(file->filename());
    buffer_.push_back("\"\n");
  }

  void generateTrailer() override {
    response_->setContentLength(buffer_.size());
    response_->setHeader("Content-Type", "text/csv");
    response_->write(std::move(buffer_));
  }
};
// }}}
class JsonFormatter : public OutputFormatter { // {{{
 public:
  HttpResponse* response_;
  Buffer buffer_;
  JsonWriter writer_;

  explicit JsonFormatter(HttpResponse* resp)
      : response_(resp), buffer_(), writer_(&buffer_) {}

  void generateHeader(const std::string& path) override {
    writer_.beginArray("");
  }

  void generateEntry(std::shared_ptr<File> file) override {
    writer_.beginObject();

    writer_.name("filename").value(file->filename());
    writer_.name("type").value(file->isDirectory() ? "directory"
                                                   : "file");
    writer_.name("mimetype").value(file->mimetype());
    writer_.name("last-modified").value(file->lastModified());
    writer_.name("mtime").value(file->mtime().unixtime());
    writer_.name("size").value(file->isDirectory() ? 0 : file->size());
    writer_.endObject();
  }

  void generateTrailer() override {
    writer_.endArray();
    buffer_.push_back('\n');

    response_->setContentLength(buffer_.size());
    response_->setHeader("Content-Type", "application/json");
    response_->write(std::move(buffer_));
  }
};
// }}}
class HtmlFormatter : public OutputFormatter { // {{{
 public:
  Buffer buffer_;
  HttpResponse* response_;

  explicit HtmlFormatter(HttpResponse* resp) : response_(resp) {}

  void generateEntry(std::shared_ptr<File> file) override {
    if (file->isDirectory()) {
      appendDirectory(file->filename());
    } else if (file->isRegular()) {
      appendFile(file);
    }
  }

  void generateHeader(const std::string& path) override {
    buffer_ << "<html><head>"
            << "<title>Directory: " << path << "</title>"
            << "<style>\n"
               "\tthead { font-weight: bold; }\n"
               "\ttd.name { width: 200px; }\n"
               "\ttd.size { width: 80px; }\n"
               "\ttd.subdir { width: 280px; }\n"
               "\ttd.mimetype { }\n"
               "\ttr:hover { background-color: #EEE; }\n"
               "</style>\n";
    buffer_ << "</head>\n";
    buffer_ << "<body>\n";

    buffer_ << "<h2 style='font-family: Courier New, monospace;'>Index of "
            << path << "</h2>\n";
    buffer_ << "<br/>";
    buffer_ << "<table>\n";

    buffer_ << "<thead>"
               "<td class='name'>Name</td>"
               "<td class='size'>Size</td>"
               "<td class='mimetype'>Mime type</td>"
               "</thead>\n";

    if (path != "/") {
      appendDirectory("..");
    }
  }

  void appendDirectory(const std::string& filename) {
    buffer_ << "\t<tr>\n";
    buffer_ << "\t\t<td class='subdir' colspan='2'><a href='"
            << filename << "/'>" << filename << "/</a></td>\n"
            << "\t\t<td class='mimetype'>directory</td>"
            << "</td>\n";
    buffer_ << "\t</tr>\n";
  }

  void appendFile(std::shared_ptr<File> file) {
    buffer_ << "\t<tr>\n";
    buffer_ << "\t\t<td class='name'><a href='" << file->filename() << "'>"
            << file->filename() << "</a></td>\n";
    buffer_ << "\t\t<td class='size'>" << file->size() << "</td>\n";
    buffer_ << "\t\t<td class='mimetype'>" << file->mimetype() << "</td>\n";
    buffer_ << "\t</tr>\n";
  }

  void generateTrailer() override {
    buffer_ << "</table>\n";
    buffer_ << "<hr/>\n";
    buffer_ << "</body></html>\n";

    response_->setContentLength(buffer_.size());
    response_->setHeader("Content-Type", "text/html");
    response_->write(std::move(buffer_));
  }
}; // }}}

DirlistingModule::DirlistingModule(x0d::Daemon* d)
    : Module(d, "dirlisting") {

  mainHandler("dirlisting", &DirlistingModule::dirlisting);
}

bool DirlistingModule::dirlisting(Context* cx, Params& args) {
  if (cx->tryServeTraceOrigin())
    return true;

  if (cx->request()->directoryDepth() < 0) {
    cx->logError("Directory traversal detected: {}", cx->request()->path());
    return cx->sendErrorPage(HttpStatus::BadRequest);
  }

  if (!cx->file()) {
    cx->logError("dirlisting: Requesth path not mapped to a physical location yet.");
    return cx->sendErrorPage(HttpStatus::InternalServerError);
  }

  if (!cx->file()->isDirectory())
    return false;

  std::unique_ptr<OutputFormatter> formatter;

  std::string accept = MediaRange::match(cx->request()->getHeader("Accept"),
                                         {"text/html", "application/json",
                                          "text/csv"});

  if (accept == "text/csv") {
    formatter = std::make_unique<CsvFormatter>(cx->response());
  } else if (accept == "application/json") {
    formatter = std::make_unique<JsonFormatter>(cx->response());
  } else {
    // default to text/html
    formatter = std::make_unique<HtmlFormatter>(cx->response());
  }

  formatter->generateHeader(cx->request()->path());

  FileUtil::ls(cx->file()->path(), [&](const std::string& path) -> bool {
    formatter->generateEntry(daemon().vfs().getFile(path));
    return true;
  });

  cx->response()->setStatus(HttpStatus::Ok);
  formatter->generateTrailer();
  cx->response()->completed();

  return true;
}

} // namespace x0d
