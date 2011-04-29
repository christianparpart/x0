/* <x0/sql/SqlConnection.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010-2011 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/sql/SqlConnection.h>
#include <x0/sql/SqlResult.h>

#include <stdio.h>
#include <mysql/mysql.h>
#include <mysql/errmsg.h>

namespace x0 {

SqlConnection::SqlConnection() :
	handle_(NULL)
{
	handle_ = mysql_init(NULL);
}

SqlConnection::~SqlConnection()
{
	if (handle_)
		mysql_close(handle_);
}

bool SqlConnection::open(const char *hostname, const char *username, const char *passwd, const char *database, int port)
{
	if (mysql_real_connect(handle_, hostname, username, passwd, database, port, 0, 0) == NULL)
		return false;

	username_ = username;
	passwd_ = passwd;
	database_ = database;
	hostname_ = hostname;
	port_ = port;
	return true;
}

bool SqlConnection::isOpen() const
{
	return mysql_ping(handle_) == 0;
}

bool SqlConnection::ping()
{
	return mysql_ping(handle_) == 0;
}

MYSQL *SqlConnection::handle()
{
	return handle_;
}

SqlConnection::operator MYSQL* () const
{
	return handle_;
}

std::string SqlConnection::makeQuery(const char *s)
{
#if !defined(NDEBUG)
	const char *ps = s;

	while (*s) {
		if (*s == '?' && *(++s) != '?') {
			fprintf(stderr, "invalid format string: missing args\n");
		}
		++s;
	}

	return ps;
#else
	return s;
#endif
}

template<>
std::string SqlConnection::queryField<std::string>(const char *tableName,
	const char *keyName, const char *keyValue, const char *fieldName)
{
	SqlResult result(query("SELECT `?` FROM `?` WHERE `?` = '?'", fieldName, tableName, keyName, keyValue));

	if (result.fetch())
		return result.valueOf(fieldName);

	return std::string();
}

unsigned long long SqlConnection::affectedRows() const
{
	return mysql_affected_rows(handle_);
}

} // namespace x0
