/* <x0/sql/SqlStatement.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010-2011 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_sql_SqlStatement_h
#define sw_x0_sql_SqlStatement_h

#include <mysql/mysql.h>
#include <vector>
#include <string>
#include <string.h> // strcmp(), memset()
#include <stdio.h> // snprintf(), fprintf()

namespace x0 {

class SqlStatement
{
private:
	MYSQL *conn_;
	MYSQL_STMT *stmt_;
	MYSQL_RES *meta_;
	unsigned bindOffset_;
	std::vector<MYSQL_BIND> params_;
	std::vector<MYSQL_FIELD *> fields_;
	std::vector<MYSQL_BIND> data_;
	unsigned long *fixedLengths_;
	unsigned long *varLengths_;
	my_bool *nulls_;
	const char *query_;
	const char *error_;

	unsigned currentRow_;

public:
	SqlStatement();
	SqlStatement(MYSQL *c, const char *s);
	~SqlStatement();

	bool isError() const;
	const char *error() const;

	bool reset();

	bool prepare(MYSQL *c, const char *s);
	template<typename... Args> bool execute(const Args&... args);
	bool fetch();

	unsigned currentRow() const;
	unsigned numRows() const;
	unsigned numFields() const;

	unsigned long long affectedRows() const;
	unsigned long long lastInsertId() const;

	const char *nameAt(unsigned index) const;

	template<typename T> T valueAt(unsigned index) const;
	template<typename T> T valueOf(const char *name) const;

	bool isNullAt(unsigned index) const;
	bool isNullAt(const char *name) const;

	template<typename... Args> bool operator()(const Args&... args);
	SqlStatement& operator++();
	operator bool() const;

private:
	MYSQL_BIND *getParam();

	bool bindParam();
	template<typename Arg1> bool bindParam(const Arg1& a1);
	template<typename Arg1, typename... Args> bool bindParam(const Arg1& a1, const Args&... args);

	bool run();

	static const char *mysql_type_str(int type);
};

// {{{ template impls / inlines
template<>
bool SqlStatement::bindParam<bool>(const bool& value)
{
	//DEBUG("bind bool");

	MYSQL_BIND *b = getParam();
	memset(b, 0, sizeof(*b));
	b->buffer_type = MYSQL_TYPE_TINY;
	b->buffer_length = sizeof(bool);
	b->length = &b->buffer_length;
	b->is_unsigned = false;
	b->is_null = false;
	b->buffer = (char *)&value;

	return true;
}

template<>
bool SqlStatement::bindParam<int>(const int& value)
{
	//debug("bind int");
	MYSQL_BIND *b = getParam();
	memset(b, 0, sizeof(*b));
	b->buffer_type = MYSQL_TYPE_LONG;
	b->buffer_length = sizeof(int);
	b->length = &b->buffer_length;
	b->is_unsigned = false;
	b->is_null = false;
	b->buffer = (char *)&value;

	return true;
}

template<>
bool SqlStatement::bindParam<std::string>(const std::string& value)
{
	//DEBUG("bind string");
	MYSQL_BIND *b = getParam();
	memset(b, 0, sizeof(*b));
	b->buffer_type = MYSQL_TYPE_STRING;
	b->buffer_length = value.size();
	b->length = &b->buffer_length;
	b->is_unsigned = false;
	b->is_null = false;
	b->buffer = (char *)value.data();

	return true;
}

template<typename Arg1, typename... Args>
bool SqlStatement::bindParam(const Arg1& a1, const Args&... args)
{
	return bindParam(a1) && bindParam(args...);
}

template<typename... Args>
bool SqlStatement::execute(const Args&... args)
{
	bindOffset_ = 0;

	if (bindParam(args...))
		return run();
	else
		return false;
}

template<typename... Args>
bool SqlStatement::operator()(const Args&... args)
{
	return execute(args...);
}

template<>
bool SqlStatement::valueAt<bool>(unsigned index) const
{
	if (nulls_[index])
		return false;

	const MYSQL_BIND *d = &data_[index];
	switch (fields_[index]->type)
	{
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_VARCHAR:
			return d->buffer_length > 0;
		case MYSQL_TYPE_LONG:
			return *(int32_t *)d->buffer != 0;
		case MYSQL_TYPE_TINY:
			return *(uint8_t *)d->buffer != 0;
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATETIME:
			return true;
		default:
#if !defined(NDEBUG)
			fprintf(stderr, "Unknown type case from bool to %s\n", mysql_type_str(fields_[index]->type));
#endif
			return true;
	}
}

template<>
int SqlStatement::valueAt<int>(unsigned index) const
{
	if (nulls_[index])
		return 0;

	const MYSQL_BIND *d = &data_[index];
	switch (fields_[index]->type)
	{
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_VARCHAR:
			return d->buffer_length;
		case MYSQL_TYPE_TINY:
			return *(uint8_t *)d->buffer;
		case MYSQL_TYPE_SHORT:
			return *(uint16_t *)d->buffer;
		case MYSQL_TYPE_LONG:
			return *(int32_t *)d->buffer;
		case MYSQL_TYPE_LONGLONG:
			return *(int64_t *)d->buffer;
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATETIME:
			return 0; // TODO return time_t
		default:
#if !defined(NDEBUG)
			fprintf(stderr, "Unknown type case from INT to %s\n", mysql_type_str(fields_[index]->type));
#endif
			return 0;
	}
}

template<>
std::string SqlStatement::valueAt<std::string>(unsigned index) const
{
	if (nulls_[index])
		return std::string();

	const MYSQL_BIND *d = &data_[index];
	switch (fields_[index]->type)
	{
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_VARCHAR:
			if (d->buffer_length) {
				return std::string((char*)d->buffer, 0, d->buffer_length);
			} else
				return std::string();
		case MYSQL_TYPE_LONG:
		{
			char buf[64];
			snprintf(buf, sizeof(buf), "%d", *(int32_t *)d->buffer);
			return buf;
			//return std::string((char*)d->buffer, 0, d->buffer_length);
		}
		case MYSQL_TYPE_TINY: {
			static std::string boolstr[] = { "false", "true" };
			return boolstr[*(char*)d->buffer == '1' ? 1 : 0];
		}
		case MYSQL_TYPE_DATE: {
			MYSQL_TIME *ts = (MYSQL_TIME *)d->buffer;
			char buf[11];
			snprintf(buf, sizeof(buf), "%04d-%02d-%02d", ts->year, ts->month, ts->day);
			return buf;
		}
		case MYSQL_TYPE_TIME: {
			MYSQL_TIME *ts = (MYSQL_TIME *)d->buffer;
			char buf[9];
			snprintf(buf, sizeof(buf), "%02d:%02d:%02d", ts->hour, ts->minute, ts->second);
			return buf;
		}
		case MYSQL_TYPE_DATETIME: {
			MYSQL_TIME *ts = (MYSQL_TIME *)d->buffer;
			char buf[20];
			snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
				ts->year, ts->month, ts->day, ts->hour, ts->minute, ts->second);
			return buf;
		}
		default:
			char buf[128];
			snprintf(buf, sizeof(buf), "unknown:<%s>", mysql_type_str(fields_[index]->type));
			return buf;
	}
}

template<typename T>
T SqlStatement::valueOf(const char *name) const
{
	for (unsigned i = 0, e = fields_.size(); i != e; ++i)
		if (strcmp(fields_[i]->name, name) == 0)
			return valueAt<T>(i);

	// field not found
	return T();
}
// }}}

} // namespace x0

#endif
