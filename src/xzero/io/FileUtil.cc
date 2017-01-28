// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/FileUtil.h>
#include <xzero/io/FileView.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/io/File.h>
#include <xzero/RuntimeError.h>
#include <xzero/Buffer.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>

#include <system_error>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>

namespace xzero {

static const char PathSeperator = '/';

char FileUtil::pathSeperator() noexcept {
  return PathSeperator;
}

std::string FileUtil::currentWorkingDirectory() {
  char buf[PATH_MAX];
  if (getcwd(buf, sizeof(buf)))
    return buf;

  RAISE_ERRNO(errno);
}

std::string FileUtil::absolutePath(const std::string& relpath) {
  if (relpath.empty())
    return currentWorkingDirectory();

  if (relpath[0] == PathSeperator)
    return relpath; // absolute already

  return joinPaths(currentWorkingDirectory(), relpath);
}

std::string FileUtil::realpath(const std::string& relpath) {
  char result[PATH_MAX];
  if (::realpath(relpath.c_str(), result) == nullptr)
    RAISE_SYSERR(errno, "Cannot resolve %s", relpath.c_str());

  return result;
}

bool FileUtil::exists(const std::string& path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0;
}

bool FileUtil::isDirectory(const std::string& path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool FileUtil::isRegular(const std::string& path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

size_t FileUtil::size(const std::string& path) {
  struct stat st;

  if (stat(path.c_str(), &st) == 0)
    return st.st_size;

  RAISE_ERRNO(errno);
}

size_t FileUtil::sizeRecursive(const std::string& path) {
  struct stat st;
  if (stat(path.c_str(), &st) < 0)
    RAISE_ERRNO(errno);

  if (S_ISREG(st.st_mode))
    return st.st_size;

  if (!S_ISDIR(st.st_mode))
    return 0;

  size_t totalSize = 0;
  FileUtil::ls(path, [&totalSize](const std::string& childpath) -> bool {
    totalSize += sizeRecursive(childpath);
    return true;
  });
  return totalSize;
}

void FileUtil::ls(const std::string& path,
                  std::function<bool(const std::string&)> callback) {
  DIR* dir = opendir(path.c_str());
  if (!dir)
    RAISE_ERRNO(errno);

  int len = offsetof(dirent, d_name) + pathconf(path.c_str(), _PC_NAME_MAX);
  dirent* dep = (dirent*) new unsigned char[len + 1];
  dirent* res = nullptr;
  Buffer buf;

  std::string filename;
  filename = joinPaths(path, "/");
  std::size_t baseFileNameLength = filename.size();

  while (readdir_r(dir, dep, &res) == 0 && res) {
    // skip "."
    if (dep->d_name[0] == '.' && dep->d_name[1] == 0)
      continue;

    // skip ".."
    if (dep->d_name[0] == '.' && dep->d_name[1] == '.' && dep->d_name[2] == 0)
      continue;

    // prepare filename
    filename += static_cast<char*>(dep->d_name);

    // callback
    if (!callback(filename))
      break;

    // reset filename to its base-length
    filename.resize(baseFileNameLength);
  }

  // free resources
  delete[] dep;
  closedir(dir);
}

std::string FileUtil::joinPaths(const std::string& base,
                                const std::string& append) {
  if (base.empty())
    return append;

  if (append.empty())
    return base;

  if (base.back() == PathSeperator) {
    if (append.front() == PathSeperator) {
      return base + append.substr(1);
    } else {
      return base + append;
    }
  } else {
    if (append.front() == PathSeperator) {
      return base + append;
    } else {
      return base + '/' + append;
    }
  }
}

void FileUtil::seek(int fd, off_t offset) {
  off_t rv = ::lseek(fd, offset, SEEK_SET);
  if (rv == (off_t) -1)
    RAISE_ERRNO(errno);
}

size_t FileUtil::read(int fd, Buffer* output) {
  struct stat st;
  if (fstat(fd, &st) < 0)
    RAISE_ERRNO(errno);

  if (st.st_size > 0) {
    size_t beg = output->size();
    output->reserve(beg + st.st_size + 1);
    ssize_t nread = ::pread(fd, output->data() + beg, st.st_size, 0);
    if (nread < 0)
      RAISE_ERRNO(errno);

    output->data()[beg + nread] = '\0';
    output->resize(beg + nread);
    return nread;
  }

  // XXX some files do not yield informations via stat, such as files in /proc.
  // So fallback to standard read() until EOF is reached.
  output->reserve(output->size() + 4096);
  ssize_t nread = 0;
  for (;;) {
    ssize_t rv = ::read(fd, output->end(), output->capacity() - output->size());
    if (rv > 0) {
      output->resize(output->size() + rv);
      nread += rv;
    } else if (rv == 0) {
      break;
    } else if (errno == EINTR) {
      continue;
    } else {
      RAISE_ERRNO(errno);
    }
  }
  return nread;
}

size_t FileUtil::read(File& file, Buffer* output) {
  FileDescriptor fd = file.createPosixChannel(File::Read);
  return read(fd, output);
}

size_t FileUtil::read(const std::string& path, Buffer* output) {
  FileDescriptor fd = open(path.c_str(), O_RDONLY);
  if (fd < 0)
    RAISE_ERRNO(errno);

  return read(fd, output);
}

size_t FileUtil::read(const FileView& file, Buffer* output) {
  output->reserve(file.size() + 1);
  size_t nread = 0;

  do {
    ssize_t rv = ::pread(file.handle(), output->data(),
                         file.size() - nread, file.offset() + nread);
    if (rv < 0) {
      switch (errno) {
        case EINTR:
        case EAGAIN:
          break;
        default:
          RAISE_ERRNO(errno);
      }
    } else if (rv == 0) {
      // end of file reached
      break;
    } else {
      nread += rv;
    }
  } while (nread < file.size());

  output->data()[nread] = '\0';
  output->resize(nread);

  return nread;
}

Buffer FileUtil::read(int fd) {
  Buffer output;
  read(fd, &output);
  return output;
}

Buffer FileUtil::read(File& file) {
  Buffer output;
  read(file, &output);
  return output;
}

Buffer FileUtil::read(const FileView& file) {
  Buffer output;
  read(file, &output);
  return output;
}

Buffer FileUtil::read(const std::string& path) {
  Buffer output;
  read(path, &output);
  return output;
}

void FileUtil::write(const std::string& path, const Buffer& buffer) {
  int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0660);
  if (fd < 0)
    RAISE_ERRNO(errno);

  ssize_t nwritten = 0;
  do {
    ssize_t rv = ::write(fd, buffer.data(), buffer.size());
    if (rv < 0) {
      close(fd);
      RAISE_ERRNO(errno);
    }
    nwritten += rv;
  } while (static_cast<size_t>(nwritten) < buffer.size());

  close(fd);
}

void FileUtil::write(int fd, const BufferRef& buffer) {
  size_t nwritten = 0;
  do {
    ssize_t rv = ::write(fd, buffer.data() + nwritten, buffer.size() - nwritten);
    if (rv < 0) {
      switch (errno) {
        case EINTR:
        case EAGAIN:
          break;
        default:
          RAISE_ERRNO(errno);
      }
    } else {
      nwritten += rv;
    }
  } while (nwritten < buffer.size());
}

void FileUtil::write(int fd, const FileView& fileView) {
  write(fd, read(fileView));
}

void FileUtil::copy(const std::string& from, const std::string& to) {
  RAISE_STATUS(NotImplementedError);
}

void FileUtil::truncate(const std::string& path, size_t size) {
  if (::truncate(path.c_str(), size) < 0)
    RAISE_ERRNO(errno);
}

std::string FileUtil::dirname(const std::string& path) {
  size_t n = path.rfind(PathSeperator);
  return n != std::string::npos
         ? path.substr(0, n)
         : std::string(".");
}

std::string FileUtil::basename(const std::string& path) {
  size_t n = path.rfind(PathSeperator);
  return n != std::string::npos
         ? path.substr(n)
         : path;
}

void FileUtil::mkdir(const std::string& path, int mode) {
  if (::mkdir(path.c_str(), mode) < 0)
    RAISE_ERRNO(errno);
}

void FileUtil::mkdir_p(const std::string& dirname, int mode) {
  char const* begin = dirname.c_str();
  char const* cur = begin;

  if (exists(dirname)) {
    if (isDirectory(dirname)) {
      return;
    } else {
      RAISE(
          IOError,
          "file '%s' exists but is not a directory",
          dirname.c_str());
    }
  }

  for (cur = begin; *cur == '/'; ++cur);

  while ((cur = strchr(cur, '/'))) {
    std::string path(begin, cur);
    cur++;

    if (exists(path)) {
      if (isDirectory(path)) {
        continue;
      } else {
        RAISE(
            IOError,
            "file '%s' exists but is not a directory",
            path.c_str());
      }
    }

    mkdir(path);
  }

  mkdir(dirname);
}

void FileUtil::rm(const std::string& path) {
  if (::unlink(path.c_str()) < 0)
    RAISE_ERRNO(errno);
}

void FileUtil::mv(const std::string& path, const std::string& target) {
  if (::rename(path.c_str(), target.c_str()) < 0)
    RAISE_ERRNO(errno);
}

void FileUtil::chown(const std::string& path, int uid, int gid) {
  if (::chown(path.c_str(), uid, gid) < 0)
    RAISE_ERRNO(errno);
}

void FileUtil::chown(const std::string& path,
                     const std::string& user,
                     const std::string& group) {
  errno = 0;
  struct passwd* pw = getpwnam(user.c_str());
  if (!pw) {
    if (errno != 0) {
      RAISE_ERRNO(errno);
    } else {
      RAISE(RuntimeError, "Unknown user name.");
    }
  }
  int uid = pw->pw_uid;

  struct group* gr = getgrnam(group.c_str());
  if (!gr) {
    if (errno != 0) {
      RAISE_ERRNO(errno);
    } else {
      RAISE(RuntimeError, "Unknown group name.");
    }
  }
  int gid = gr->gr_gid;

  FileUtil::chown(path, uid, gid);
}

int FileUtil::createTempFile() {
  return createTempFileAt(tempDirectory());
}

int FileUtil::createTempFileAt(const std::string& basedir, std::string* result) {
#if defined(ENABLE_O_TMPFILE) && defined(O_TMPFILE)
  int flags = O_TMPFILE | O_CLOEXEC | O_RDWR;
  int mode = S_IRUSR | S_IWUSR;
  int fd = ::open(basedir.c_str(), flags, mode);

  if (fd < 0)
    RAISE_ERRNO(errno);

  if (result)
    result->clear();
#elif defined(HAVE_MKOSTEMPS)
  std::string pattern = joinPaths(basedir, "XXXXXXXX.tmp");
  int flags = O_CLOEXEC;
  int fd = mkostemps(const_cast<char*>(pattern.c_str()), 4, flags);

  if (fd < 0)
    RAISE_ERRNO(errno);

  if (result)
    *result = std::move(pattern);
  else
    FileUtil::rm(pattern);
#else
  std::string pattern = joinPaths(basedir, "XXXXXXXX.tmp");
  int fd = mkstemps(const_cast<char*>(pattern.c_str()), 4);

  if (fd < 0)
    RAISE_ERRNO(errno);

  if (result)
    *result = std::move(pattern);
  else
    FileUtil::rm(pattern);
#endif

  return fd;
}

std::string FileUtil::createTempDirectory() {
  std::string path;
  path += tempDirectory();
  path += PathSeperator;
  path += "xzero.XXXXXXXX";

  if (!mkdtemp(const_cast<char*>(path.c_str())))
    RAISE_ERRNO(errno);

  return path;
}

std::string FileUtil::tempDirectory() {
  if (const char* s = getenv("TMPDIR"))
    return s;

  if (const char* s = getenv("TEMPDIR"))
    return s;

  if (const char* s = getenv("TEMP"))
    return s;

  return "/tmp";
}

void FileUtil::allocate(int fd, size_t length) {
  if (ftruncate(fd, length) < 0)
    RAISE_ERRNO(errno);
}

void FileUtil::preallocate(int fd, off_t offset, size_t length) {
#if defined(FALLOC_FL_KEEP_SIZE)
  const int mode = FALLOC_FL_KEEP_SIZE;
  if (fallocate(fd, mode, offset, length) < 0)
    RAISE_ERRNO(errno);
#else
  // ignoring is fine
#endif
}

void FileUtil::deallocate(int fd, off_t offset, size_t length) {
#if defined(FALLOC_FL_PUNCH_HOLE) && defined(FALLOC_FL_KEEP_SIZE)
  const int mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;
  if (fallocate(fd, mode, offset, length) < 0)
    RAISE_ERRNO(errno);
#else
  // we cannot release the underlying pages, but we can at least conform to
  // filling with zeroes
  const size_t pagesize = 4 * 4096;
  Buffer zeroes;
  zeroes.push_back('\0', pagesize);

  while (length > pagesize) {
    if (::pwrite(fd, zeroes.data(), pagesize, offset) < 0)
      RAISE_ERRNO(errno);

    offset += pagesize;
    length -= pagesize;
  }

  if (::pwrite(fd, zeroes.data(), length, offset) < 0)
    RAISE_ERRNO(errno);
#endif
}

void FileUtil::collapse(int fd, off_t offset, size_t length) {
#if defined(FALLOC_FL_COLLAPSE_RANGE)
  const int mode = FALLOC_FL_COLLAPSE_RANGE;
  if (fallocate(fd, mode, offset, length) < 0)
    RAISE_ERRNO(errno);
#else
  // TODO: implement in userspace by copy+truncate
  RAISE_STATUS(NotImplementedError);
#endif
}

void FileUtil::truncate(int fd, size_t length) {
  if (ftruncate(fd, length) < 0)
    RAISE_ERRNO(errno);
}

void FileUtil::close(int fd) {
  for (;;) {
    int rv = ::close(fd);
    switch (rv) {
      case 0:
        return;
      case EINTR:
        break;
      default:
        RAISE_ERRNO(errno);
    }
  }
}

bool FileUtil::isBlocking(int fd) {
  return !(fcntl(fd, F_GETFL) & O_NONBLOCK);
}

void FileUtil::setBlocking(int fd, bool enable) {
  unsigned flags = enable ? fcntl(fd, F_GETFL) & ~O_NONBLOCK
                          : fcntl(fd, F_GETFL) | O_NONBLOCK;

  if (fcntl(fd, F_SETFL, flags) < 0) {
    RAISE_ERRNO(errno);
  }
}

}  // namespace xzero
