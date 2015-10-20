// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/logging.h>
#include <xzero/logging/LogAggregator.h>
#include <xzero/logging/LogSource.h>
#include <xzero/RuntimeError.h>

namespace xzero {

void logError(const char* component, const std::exception& e) {
  LogSource* logger = LogAggregator::get().getSource(component);
  logger->error(e);
}

} // namespace xzero
