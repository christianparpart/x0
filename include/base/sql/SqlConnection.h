// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_sql_Connection_h
#define sw_x0_sql_Connection_h

#include <base/Api.h>
#include <base/Buffer.h>
#include <base/sql/SqlResult.h>
#include <ev++.h>
#include <mysql.h>
#include <errmsg.h>
#include <unistd.h>

namespace base {

//! \addtogroup sql
//@{

class X0_API SqlConnection {
 private:
  MYSQL handle_;

  std::string username_;
  std::string passwd_;
  std::string database_;
  std::string hostname_;
  int port_;

 public:
  SqlConnection();
  ~SqlConnection();

  MYSQL *handle();

  bool open(const char *hostname, const char *username, const char *passwd,
            const char *database, int port = 3306);
  bool isOpen() const;
  bool ping();
  void close();

  template <typename... Args>
  SqlResult query(const char *queryStr, Args &&... args);

  template <typename T, typename... Args>
  T queryScalar(const char *queryStr, Args... args);

  operator MYSQL *() const;

  template <typename T>
  T queryField(const char *table, const char *keyName, const char *keyValue,
               const char *fieldName);

  unsigned long long affectedRows() const;

 private:
  std::string makeQuery(const char *s);
  template <typename Arg1, typename... Args>
  std::string makeQuery(const char *s, Arg1 &&a1, Args &&... args);
};

//@}

// {{{ inlines / template impl
template <typename... Args>
SqlResult SqlConnection::query(const char *queryStr, Args &&... args) {
  std::string q(makeQuery(queryStr, args...));
  unsigned i = 0;
  int rc;

  do {
    if (i) sleep(i);
    rc = mysql_real_query(&handle_, q.c_str(), q.size());
  } while (rc != 0 && mysql_errno(&handle_) != CR_SERVER_GONE_ERROR);

  return SqlResult(&handle_);
}

template <typename T, typename... Args>
T SqlConnection::queryScalar(const char *queryStr, Args... args) {
  SqlResult result(query(queryStr, args...));
  if (result && result.fetch()) return result.at<T>(0);

  return T();
}

template <typename Arg1, typename... Args>
std::string SqlConnection::makeQuery(const char *s, Arg1 &&a1,
                                     Args &&... args) {
  Buffer result;
  while (*s) {
    if (*s == '?' && *(++s) != '?') {
      result.push_back(a1);
      result.push_back(makeQuery(s, args...));
      return result.c_str();
    }
    result.push_back(*s++);
  }
  fprintf(stderr, "internal error: extra args provided to query\n");
  return result.c_str();
}
// }}}

}  // namespace base

#endif
