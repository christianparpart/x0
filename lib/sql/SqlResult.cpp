/* <x0/sql/SqlResult.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2010-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/sql/SqlResult.h>
#include <x0/sql/SqlConnection.h>

#include <mysql.h>
#include <errmsg.h>

namespace x0 {

SqlResult::SqlResult(MYSQL *h) :
	handle_(h),
	result_(mysql_store_result(handle_)),
	currentRow_(0),
	numRows_(0),
	fields_(),
	errorCode_(mysql_errno(handle_)),
	errorText_(mysql_error(handle_))
{
	//DEBUG("SqlResult()");
	if (result_) {
		unsigned fieldCount = mysql_num_fields(result_);
		fields_.reserve(fieldCount);
		data_.resize(fieldCount);

		numRows_ = mysql_num_rows(result_);

		for (unsigned i = 0; i != fieldCount; ++i) {
			fields_.push_back(mysql_fetch_field_direct(result_, i));
		}
	}
}

SqlResult::SqlResult() :
	handle_(NULL),
	result_(NULL),
	currentRow_(0),
	numRows_(0),
	fields_(),
	errorCode_(0),
	errorText_(NULL)
{
}

SqlResult::SqlResult(SqlResult&& r) :
	handle_(std::move(r.handle_)),
	result_(std::move(r.result_)),
	currentRow_(std::move(r.currentRow_)),
	numRows_(std::move(r.numRows_)),
	fields_(std::move(r.fields_)),
	data_(std::move(r.data_)),
	errorCode_(std::move(r.errorCode_)),
	errorText_(std::move(r.errorText_))
{
	//DEBUG("SqlResult(SqlResult&&)");
	assert(r.data_.empty());
	assert(r.fields_.empty());
	r.result_ = NULL;
	r.handle_ = NULL;
}

SqlResult::~SqlResult()
{
	//DEBUG("~SqlResult()");
	if (result_)
		mysql_free_result(result_);
}

bool SqlResult::isError() const
{
	return !errorText_.empty();
}

unsigned SqlResult::currentRow() const
{
	return currentRow_;
}

unsigned SqlResult::numRows() const
{
	return numRows_;
}

unsigned SqlResult::numFields() const
{
	return mysql_num_fields(result_);
}

bool SqlResult::fetch()
{
	MYSQL_ROW row = mysql_fetch_row(result_);
	if (!row)
		return false;

	++currentRow_;

	unsigned long *lengths = mysql_fetch_lengths(result_);

	for (unsigned i = 0, e = fields_.size(); i != e; ++i) {
		if (lengths[i])
			data_[i] = std::string(row[i], 0, lengths[i]);
		else
			data_[i].clear();
	}

	return true;
}

const char *SqlResult::nameAt(int index) const
{
	return fields_[index]->name;
}

const std::string& SqlResult::valueOf(const char *name) const
{
	for (int i = 0, e = fields_.size(); i != e; ++i)
		if (strcmp(fields_[i]->name, name) == 0)
			return data_[i];

	static const std::string none;
	return none;
}

const std::string& SqlResult::valueAt(int index) const
{
	return data_[index];
}

const std::string& SqlResult::operator[](int index) const
{
	return valueAt(index);
}

const std::string& SqlResult::operator[](const char *name) const
{
	return valueOf(name);
}

SqlResult::operator bool() const
{
	if (isError())
		return false;

	return currentRow_ < numRows_;
}

SqlResult& SqlResult::operator++()
{
	fetch();
	return *this;
}

} // namespace x0
