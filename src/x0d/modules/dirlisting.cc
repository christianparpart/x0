// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "dirlisting.h"
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/File.h>
#include <xzero/logging.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::flow;

namespace x0d {

DirlistingModule::DirlistingModule(x0d::XzeroDaemon* d)
    : XzeroModule(d, "dirlisting") {

  mainHandler("dirlisting", &DirlistingModule::dirlisting);
}

bool DirlistingModule::dirlisting(XzeroContext* cx, Params& args) {
  if (!cx->verifyDirectoryDepth())
    return true;

  if (!cx->file()) {
    logError("dirlisting", "Requesth path not mapped to a physical location yet.");
    return false;
  }

  if (!cx->file()->isDirectory())
    return false;

  Buffer sstr;

  appendHeader(sstr, cx->request()->path());

  if (cx->request()->path() != "/")
    appendDirectory(sstr, "..");

  FileUtil::ls(cx->file()->path(), [&](const std::string& path) -> bool {
    std::shared_ptr<File> file = daemon().vfs().getFile(path, "/");
    if (file->isDirectory()) {
      appendDirectory(sstr, file->filename());
    } else {
      appendFile(sstr, file);
    }
    return true;
  });
  appendTrailer(sstr);

  cx->response()->setStatus(HttpStatus::Ok);
  cx->response()->setContentLength(sstr.size());
  cx->response()->setHeader("Content-Type", "text/html");
  cx->response()->write(std::move(sstr));
  cx->response()->completed();

  return true;
}

void DirlistingModule::appendHeader(Buffer& sstr, const std::string& path) {
  sstr << "<html><head>"
       << "<title>Directory: " << path << "</title>"
       << "<style>\n"
          "\tthead { font-weight: bold; }\n"
          "\ttd.name { width: 200px; }\n"
          "\ttd.size { width: 80px; }\n"
          "\ttd.subdir { width: 280px; }\n"
          "\ttd.mimetype { }\n"
          "\ttr:hover { background-color: #EEE; }\n"
          "</style>\n";
  sstr << "</head>\n";
  sstr << "<body>\n";

  sstr << "<h2 style='font-family: Courier New, monospace;'>Index of "
       << path << "</h2>\n";
  sstr << "<br/>";
  sstr << "<table>\n";

  sstr << "<thead>"
          "<td class='name'>Name</td>"
          "<td class='size'>Size</td>"
          "<td class='mimetype'>Mime type</td>"
          "</thead>\n";
}

void DirlistingModule::appendDirectory(Buffer& sstr, const std::string& filename) {
  sstr << "\t<tr>\n";
  sstr << "\t\t<td class='subdir' colspan='2'><a href='"
       << filename << "/'>" << filename << "/</a></td>\n"
       << "\t\t<td class='mimetype'>directory</td>"
       << "</td>\n";
  sstr << "\t</tr>\n";
}

void DirlistingModule::appendFile(Buffer& sstr, std::shared_ptr<File> file) {
  sstr << "\t<tr>\n";
  sstr << "\t\t<td class='name'><a href='" << file->filename() << "'>"
       << file->filename() << "</a></td>\n";
  sstr << "\t\t<td class='size'>" << file->size() << "</td>\n";
  sstr << "\t\t<td class='mimetype'>" << file->mimetype() << "</td>\n";
  sstr << "\t</tr>\n";
}

void DirlistingModule::appendTrailer(Buffer& sstr) {
  sstr << "</table>\n";
  sstr << "<hr/>\n";
  sstr << "</body></html>\n";
}

} // namespace x0d
