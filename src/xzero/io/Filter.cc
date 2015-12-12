// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/io/Filter.h>
#include <xzero/io/FileView.h>
#include <xzero/Buffer.h>
#include <xzero/sysconfig.h>
#include <stdexcept>
#include <system_error>

namespace xzero {

void Filter::applyFilters(
    const std::list<std::shared_ptr<Filter>>& filters,
    const BufferRef& input, Buffer* output, bool last) {
  auto i = filters.begin();
  auto e = filters.end();

  if (i == e) {
    *output = input;
    return;
  }

  (*i)->filter(input, output, last);
  i++;
  Buffer tmp;
  while (i != e) {
    tmp.swap(*output);
    (*i)->filter(tmp.ref(), output, last);
    i++;
  }
}

void Filter::applyFilters(
    const std::list<std::shared_ptr<Filter>>& filters,
    const FileView& file, Buffer* output, bool last) {

  Buffer input;
  file.fill(&input);

  if (input.size() != file.size())
    throw std::runtime_error("Could not read full input file.");

  Filter::applyFilters(filters, input, output, last);
}

} // namespace xzero
