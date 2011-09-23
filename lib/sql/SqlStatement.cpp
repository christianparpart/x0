/* <x0/sql/SqlStatement.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010-2011 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/sql/SqlStatement.h>
#include <mysql/errmsg.h>
#include <assert.h>

#if !defined(NDEBUG)
#	define TRACE(msg...) const_cast<SqlStatement*>(this)->debug(msg)
#else
#	define TRACE(msg...) do { } while (0)
#endif

namespace x0 {

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

bool SqlStatement::reset()
{
	if (mysql_stmt_reset(stmt_) == 0) {
		currentRow_ = 0;
		error_ = NULL;
		return true;
	} else {
		error_ = mysql_stmt_error(stmt_);
		return false;
	}
}

const char *SqlStatement::mysql_type_str(int type)
{
	switch (type) {
		case MYSQL_TYPE_DECIMAL: return "DECIMAL";
		case MYSQL_TYPE_TINY: return "TINYINT";
		case MYSQL_TYPE_SHORT: return "SHORT";
		case MYSQL_TYPE_LONG: return "LONG";
		case MYSQL_TYPE_FLOAT: return "FLOAT";
		case MYSQL_TYPE_DOUBLE: return "DOUBLE";
		case MYSQL_TYPE_NULL: return "NULL";
		case MYSQL_TYPE_TIMESTAMP: return "TIMESTAMP";
		case MYSQL_TYPE_LONGLONG: return "LONGLONG";
		case MYSQL_TYPE_INT24: return "INT24";
		case MYSQL_TYPE_DATE: return "DATE";
		case MYSQL_TYPE_TIME: return "TIME";
		case MYSQL_TYPE_DATETIME: return "DATETIME";
		case MYSQL_TYPE_YEAR: return "YEAR";
		case MYSQL_TYPE_NEWDATE: return "NEWDATE";
		case MYSQL_TYPE_VARCHAR: return "VARCHAR";
		case MYSQL_TYPE_BIT: return "BIT";
		case MYSQL_TYPE_NEWDECIMAL: return "NEWDECIMAL";
		case MYSQL_TYPE_ENUM: return "ENUM";
		case MYSQL_TYPE_SET: return "SET";
		case MYSQL_TYPE_TINY_BLOB: return "TINY_BLOB";
		case MYSQL_TYPE_MEDIUM_BLOB: return "MEDIUM_BLOB";
		case MYSQL_TYPE_LONG_BLOB: return "LONG_BLOB";
		case MYSQL_TYPE_BLOB: return "BLOB";
		case MYSQL_TYPE_VAR_STRING: return "VAR_STRING";
		case MYSQL_TYPE_STRING: return "STRING";
		case MYSQL_TYPE_GEOMETRY: return "GEOMETRY";
		default: return "UNKNOWN";
	}
}

static int getFixedSizeLength(enum_field_types type)
{
	switch (type) {
		case MYSQL_TYPE_TINY:
			return 1;
		case MYSQL_TYPE_SHORT:
			return 2;
		case MYSQL_TYPE_LONG:
			return 4;
		case MYSQL_TYPE_LONGLONG:
			return 8;
		case MYSQL_TYPE_FLOAT:
			return 4;
		case MYSQL_TYPE_DOUBLE:
			return 8;
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP:
			return sizeof(MYSQL_TIME);
		case MYSQL_TYPE_VARCHAR:
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_VAR_STRING:
			return 0; // variable size length
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_YEAR:
		case MYSQL_TYPE_NEWDATE:
		case MYSQL_TYPE_BIT:
		case MYSQL_TYPE_NEWDECIMAL:
		case MYSQL_TYPE_ENUM:
		case MYSQL_TYPE_SET:
		case MYSQL_TYPE_GEOMETRY:
		default:
			assert(0 == "Invalid Argument!");
			return 0; // unknown
	}
}

bool SqlStatement::prepare(MYSQL *c, const char *s)
{
	if (stmt_)
		mysql_stmt_close(stmt_);

	if (query_)
		free(query_);

	conn_ = c;
	stmt_ = mysql_stmt_init(conn_);
	query_ = strdup(s);

	TRACE("prepare(\"%s\")", s);

	int rc = mysql_stmt_prepare(stmt_, s, strlen(s));

	paramCount_ = mysql_stmt_param_count(stmt_);
	params_ = paramCount_ ? new MYSQL_BIND[paramCount_] : nullptr;

	if (rc != 0) {
		error_ = mysql_stmt_error(stmt_);
		return false;
	}

	fields_.resize(mysql_stmt_field_count(stmt_));

	if (fields_.empty())
		return true;

	meta_ = mysql_stmt_result_metadata(stmt_);
	fixedLengths_ = new unsigned long[fields_.size()];
	varLengths_ = new unsigned long[fields_.size()];
	nulls_ = new my_bool[fields_.size()];
	data_.resize(fields_.size());

	memset(&data_.front(), 0, &data_.back() - &data_.front());
	memset(fixedLengths_, 0, fields_.size() * sizeof(*fixedLengths_));
	memset(varLengths_, 0, fields_.size() * sizeof(*varLengths_));
	memset(nulls_, 0, fields_.size() * sizeof(*nulls_));

	if (meta_) {
		for (unsigned i = 0, e = fields_.size(); i != e; ++i) {
			fields_[i] = mysql_fetch_field_direct(meta_, i);
			fixedLengths_[i] = getFixedSizeLength(fields_[i]->type);
			data_[i].is_null = &nulls_[i];
			data_[i].buffer_type = fields_[i]->type;
			data_[i].buffer_length = fixedLengths_[i];
			data_[i].length = &varLengths_[i];

			/*TRACE("field[%2d] name: %-24s type:%-16s datalen:%-8lu",
				i,
				fields_[i]->name,
				mysql_type_str(fields_[i]->type),
				fixedLengths_[i]
			);*/

			if (data_[i].buffer_length)
				data_[i].buffer = malloc(data_[i].buffer_length);
		}
	}

	return true;
}

const char *SqlStatement::nameAt(unsigned index) const
{
	return fields_[index]->name;
}

bool SqlStatement::isError() const
{
	return error_ != NULL;
}

const char *SqlStatement::error() const
{
	return error_;
}

bool SqlStatement::bindParam()
{
	return true;
}

MYSQL_BIND *SqlStatement::getParam()
{
	assert(bindOffset_ < paramCount_);
	return &params_[bindOffset_++];
}

bool SqlStatement::run()
{
	TRACE("%s", query_);
	if (bindOffset_ != mysql_stmt_param_count(stmt_)) {
		error_ = "Invalid parameter count";
		fprintf(stderr, "Cannot run argument with invalid parameter count.\n");
		return false;
	}

	if (paramCount_ != 0 && mysql_stmt_bind_param(stmt_, params_) != 0) {
		error_ = mysql_stmt_error(stmt_);
		return false;
	}

	unsigned i = 0;
	for (bool done = false; !done; i = 1 + i % 10) {
		switch (mysql_stmt_execute(stmt_)) {
			case CR_SERVER_GONE_ERROR:
			case CR_SERVER_LOST_EXTENDED:
			case CR_SERVER_LOST:
				if (i)
					sleep(i); 

				break;
			case 0:
				done = true;
				break;
			default:
				error_ = mysql_stmt_error(stmt_);
				return false;
		}
	}

	mysql_stmt_store_result(stmt_);
	error_ = nullptr;
	return true;
}

enum_field_types makeResultType(enum_field_types type)
{
	switch (type) {
		case MYSQL_TYPE_LONG: //return MYSQL_TYPE_LONG;

		case MYSQL_TYPE_DECIMAL: //return MYSQL_TYPE_LONG;
		case MYSQL_TYPE_TINY:    //return MYSQL_TYPE_TINY;
		case MYSQL_TYPE_SHORT:   //return MYSQL_TYPE_SHORT;
		case MYSQL_TYPE_FLOAT:   //return MYSQL_TYPE_FLOAT;
		case MYSQL_TYPE_DOUBLE:  //return MYSQL_TYPE_DOUBLE;
		case MYSQL_TYPE_TIMESTAMP://return MYSQL_TYPE_STRING;
		case MYSQL_TYPE_LONGLONG://return MYSQL_TYPE_LONGLONG;
		case MYSQL_TYPE_INT24:   //return MYSQL_TYPE_LONG;
		case MYSQL_TYPE_DATE:    //return MYSQL_TYPE_STRING;
		case MYSQL_TYPE_TIME:    //return MYSQL_TYPE_STRING;
		case MYSQL_TYPE_DATETIME://return MYSQL_TYPE_STRING;
		case MYSQL_TYPE_YEAR:    //return MYSQL_TYPE_LONG;
		case MYSQL_TYPE_NEWDATE: //return MYSQL_TYPE_STRING;
		case MYSQL_TYPE_BIT: 	 //return MYSQL_TYPE_TINY;
		case MYSQL_TYPE_NEWDECIMAL://return MYSQL_TYPE_LONG;
		case MYSQL_TYPE_ENUM:
		case MYSQL_TYPE_SET: //return MYSQL_TYPE_LONG;
		case MYSQL_TYPE_VARCHAR:
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_GEOMETRY:
		default: return MYSQL_TYPE_VAR_STRING;
	}
}

bool SqlStatement::fetch()
{
	for (unsigned i = 0, e = data_.size(); i != e; ++i) {
		MYSQL_BIND *d = &data_[i];

		if (!fixedLengths_[i]) {
			// var-size field
			if (d->buffer)
				free(d->buffer);

			d->buffer = NULL;
			d->buffer_length = 0;
		}

		nulls_[i] = 0;
	}

	if (mysql_stmt_bind_result(stmt_, &data_.front()) != 0) {
		error_ = mysql_stmt_error(stmt_);
		fprintf(stderr, "mysql_stmt_bind_result step 1 failed: %s\n", error_);
		return false;
	}

	int rc = mysql_stmt_fetch(stmt_);
	switch (rc) {
		case 1:
			error_ = mysql_stmt_error(stmt_);
			fprintf(stderr, "mysql_stmt_fetch failed (%d): %s\n", rc, error_);
			return false;
		case MYSQL_NO_DATA:
			//DEBUG("end of result set.");
			return false;
		case 0:
		case MYSQL_DATA_TRUNCATED:
		default:
			break;
	}

	for (unsigned i = 0, e = data_.size(); i != e; ++i) {
		MYSQL_BIND *d = &data_[i];

		/*TRACE("[%2u] length: result(%lu) given(%lu) %s",
				i, *d->length, d->buffer_length,
				mysql_type_str(d->buffer_type));*/

		if (fixedLengths_[i]) {
			//TRACE("fixed-length value[%d]: %s", i, this->valueAt<std::string>(i).c_str());
			continue;
		}

		d->buffer_length = *d->length;
		d->buffer = malloc(d->buffer_length + 1);
		((char *)d->buffer)[d->buffer_length] = '\0';

		if (mysql_stmt_fetch_column(stmt_, d, i, 0) != 0) {
			error_ = mysql_stmt_error(stmt_);
			fprintf(stderr, "mysql_stmt_fetch step 1 failed: %s\n", error_);
			return false;
		}
	}

	return true;
}

bool SqlStatement::isNullAt(unsigned index) const
{
	return nulls_[index];
}

unsigned SqlStatement::numFields() const
{
	return fields_.size();
}

unsigned SqlStatement::currentRow() const
{
	return currentRow_;
}

unsigned long long SqlStatement::affectedRows() const
{
	return mysql_stmt_affected_rows(stmt_);
}

unsigned long long SqlStatement::lastInsertId() const
{
	return mysql_stmt_insert_id(stmt_);
}

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
bool SqlStatement::bindParam<unsigned long long>(const unsigned long long& value)
{
	//debug("bind unsigned long long");
	MYSQL_BIND *b = getParam();
	memset(b, 0, sizeof(*b));
	b->buffer_type = MYSQL_TYPE_LONGLONG;
	b->buffer_length = sizeof(unsigned long long);
	b->length = &b->buffer_length;
	b->is_unsigned = true;
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
char SqlStatement::valueAt<char>(unsigned index) const
{
	if (nulls_[index])
		return 0;

	const MYSQL_BIND *d = &data_[index];
	switch (fields_[index]->type)
	{
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_VARCHAR:
			return d->buffer_length
				? static_cast<char*>(d->buffer)[0]
				: 0;
		case MYSQL_TYPE_TINY:
			return (char)*(uint8_t *)d->buffer;
		case MYSQL_TYPE_SHORT:
			return (char)*(uint16_t *)d->buffer;
		case MYSQL_TYPE_LONG:
			return (char)*(int32_t *)d->buffer;
		case MYSQL_TYPE_LONGLONG:
			return (char)*(int64_t *)d->buffer;
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATETIME:
			return 0; // invalid
		default:
#if !defined(NDEBUG)
			fprintf(stderr, "Unknown type case from CHAR to %s\n", mysql_type_str(fields_[index]->type));
#endif
			return 0;
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
long SqlStatement::valueAt<long>(unsigned index) const
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
			return 0;
		case MYSQL_TYPE_DATETIME: {
			MYSQL_TIME* tp = static_cast<MYSQL_TIME*>(d->buffer);

			struct tm tm;
			time_t ts = time(0);
			localtime_r(&ts, &tm);

			tm.tm_yday = 0;   // ignored
			tm.tm_wday = 0;   // ignored

			tm.tm_year = tp->year - 1900;
			tm.tm_mon = tp->month - 1;
			tm.tm_mday = tp->day;
			tm.tm_hour = tp->hour;
			tm.tm_min = tp->minute;
			tm.tm_sec = tp->second;
			ts = mktime(&tm);

			tzset();
			ts -= timezone;

			// XXX is this right? however, this seems to be the way to get the right timestamp in my case at least (as of 2011-04-30 CEST)
			if (tm.tm_isdst > 0)
				ts += 3600;

			return ts;
		}
		default:
#if !defined(NDEBUG)
			fprintf(stderr, "Unknown type case from LONG to %s\n", mysql_type_str(fields_[index]->type));
#endif
			return 0;
	}
}

template<>
unsigned long long SqlStatement::valueAt<unsigned long long>(unsigned index) const
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
			return *(uint32_t *)d->buffer;
		case MYSQL_TYPE_LONGLONG:
			return *(uint64_t *)d->buffer;
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATETIME:
			return 0; // TODO return time_t
		default:
			fprintf(stderr, "Unknown type case from INT to %s\n", mysql_type_str(fields_[index]->type));
			return 0;
	}
}

#ifndef NDEBUG
static unsigned int si_ = 0;
#endif

SqlStatement::SqlStatement() :
#ifndef NDEBUG
	Logging("SqlStatement/%d", ++si_),
#endif
	conn_(NULL),
	stmt_(NULL),
	meta_(NULL),
	bindOffset_(0),
	params_(),
	paramCount_(0),
	fields_(),
	data_(),
	fixedLengths_(NULL),
	varLengths_(NULL),
	nulls_(NULL),
	query_(NULL),
	error_(NULL),
	currentRow_(0)
{
}

SqlStatement::~SqlStatement()
{
	for (unsigned i = 0, e = fields_.size(); i != e; ++i) {
		MYSQL_BIND *d = &data_[i];
		if (d->buffer)
			free(d->buffer);
	}

	if (meta_) {
		mysql_free_result(meta_);
	}

	if (stmt_) {
		mysql_stmt_close(stmt_);
	}

	if (query_) {
		free(query_);
	}

	delete[] fixedLengths_;
	delete[] varLengths_;
	delete[] nulls_;
	delete[] params_;
}

} // namespace x0
