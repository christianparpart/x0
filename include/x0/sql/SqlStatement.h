/* <x0/sql/SqlStatement.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2010-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_sql_SqlStatement_h
#define sw_x0_sql_SqlStatement_h

#include <x0/Api.h>
#include <x0/Logging.h>
#include <vector>
#include <string>
#include <string.h> // strcmp(), memset()
#include <stdio.h> // snprintf(), fprintf()
#include <mysql.h>

namespace x0 {

//! \addtogroup sql
//@{

class X0_API SqlStatement
#ifndef XZERO_NDEBUG
	: public Logging
#endif
{
private:
	MYSQL *conn_;
	MYSQL_STMT *stmt_;
	MYSQL_RES *meta_;
	unsigned bindOffset_;
	MYSQL_BIND* params_;
	unsigned paramCount_;
	std::vector<MYSQL_FIELD *> fields_;
	std::vector<MYSQL_BIND> data_;
	unsigned long *fixedLengths_;
	unsigned long *varLengths_;
	my_bool *nulls_;
	char *query_;
	const char *error_;

	unsigned currentRow_;

public:
	class Iterator // {{{
	{
	private:
		SqlStatement* stmt_;

	public:
		Iterator(SqlStatement* stmt) :
			stmt_(stmt)
		{
		}

		SqlStatement& operator*() const
		{
			return *stmt_;
		}

		Iterator& operator++()
		{
			if (!stmt_->fetch())
				stmt_ = nullptr;

			return *this;
		}

		friend bool operator==(const Iterator& a, const Iterator& b)
		{
			return &a == &b || a.stmt_ == b.stmt_;
		}

		friend bool operator!=(const Iterator& a, const Iterator& b)
		{
			return !(a == b);
		}
	}; // }}}

	Iterator begin()
	{
		return ++Iterator(this);
	}

	Iterator end()
	{
		return Iterator(nullptr);
	}

public:
	SqlStatement();
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

	template<typename... Args> SqlStatement& operator()(const Args&... args);
	SqlStatement& operator++();
	operator bool() const;
	bool operator !() const;

private:
	MYSQL_BIND *getParam();

	bool bindParam();
	template<typename Arg1> bool bindParam(const Arg1& a1);
	template<typename Arg1, typename... Args> bool bindParam(const Arg1& a1, const Args&... args);

	bool run();

	static const char *mysql_type_str(int type);
};

//@}

// {{{ template impls / inlines
template<typename Arg1, typename... Args>
inline bool SqlStatement::bindParam(const Arg1& a1, const Args&... args)
{
	return bindParam(a1) && bindParam(args...);
}

template<typename... Args>
inline bool SqlStatement::execute(const Args&... args)
{
	bindOffset_ = 0;

	if (bindParam(args...))
		return run();
	else
		return false;
}

template<typename... Args>
inline SqlStatement& SqlStatement::operator()(const Args&... args)
{
	execute(args...);
	return *this;
}

template<typename T>
inline T SqlStatement::valueOf(const char *name) const
{
	for (unsigned i = 0, e = fields_.size(); i != e; ++i)
		if (strcmp(fields_[i]->name, name) == 0)
			return valueAt<T>(i);

	// field not found
	return T();
}

inline SqlStatement::operator bool() const
{
	return !isError();
}

inline bool SqlStatement::operator !() const
{
	return isError();
}
// }}}

} // namespace x0

#endif
