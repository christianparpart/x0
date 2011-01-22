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

namespace x0 {

SqlStatement::SqlStatement() :
	conn_(NULL),
	stmt_(NULL),
	meta_(NULL),
	bindOffset_(0),
	params_(),
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

	delete[] fixedLengths_;
	delete[] varLengths_;
	delete[] nulls_;
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

	conn_ = c;
	stmt_ = mysql_stmt_init(conn_);

	//DEBUG("SqlStatement.prepare(\"%s\")", s);

	int rc = mysql_stmt_prepare(stmt_, s, strlen(s));

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

			/*DEBUG("field[%2d] name: %-24s type:%-16s datalen:%-8lu",
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
	//DEBUG("getParam: %d", bindOffset_);
	if (bindOffset_ < params_.size())
		return &params_[bindOffset_++];

	params_.push_back(MYSQL_BIND());
	++bindOffset_;
	return &params_.back();
}

bool SqlStatement::run()
{
	if (bindOffset_ != mysql_stmt_param_count(stmt_)) {
		error_ = "Invalid parameter count";
		fprintf(stderr, "Cannot run argument with invalid parameter count.\n");
		return false;
	}

	if (params_.size() > 0 && mysql_stmt_bind_param(stmt_, &params_.front()) != 0) {
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

	// optional: mysql_stmt_store_result(stmt_);
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

		/*DEBUG("[%2u] length: result(%lu) given(%lu) %s",
				i, *d->length, d->buffer_length,
				mysql_type_str(d->buffer_type))*/;

		if (fixedLengths_[i]) {
			//DEBUG("fixed-length value[%d]: %s", i, this->valueAt<std::string>(i).c_str());
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

} // namespace x0
