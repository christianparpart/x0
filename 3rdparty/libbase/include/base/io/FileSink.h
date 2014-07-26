// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_FileSink_hpp
#define sw_x0_io_FileSink_hpp 1

#include <base/io/Sink.h>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace base {

//! \addtogroup io
//@{

/** file sink.
 */
class BASE_API FileSink : public Sink {
 private:
  std::string path_;
  int flags_;
  int mode_;
  int handle_;
  bool autoClose_;

 public:
  explicit FileSink(const std::string& filename, int flags = O_WRONLY | O_CREAT,
                    int mode = 0666);
  FileSink(int fd, bool autoClose);
  ~FileSink();

  int handle() const;

  virtual void accept(SinkVisitor& v);
  virtual ssize_t write(const void* buffer, size_t size);

  bool cycle();
};

//@}

// {{{ inlines
inline int FileSink::handle() const { return handle_; }
// }}}

}  // namespace base

#endif
