/* <x0/function.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

// this api is project independant, however
#ifndef sw_x0_function_hpp
#define sw_x0_function_hpp (1)

#include <x0/api.hpp>

namespace x0 {

/** \addtogroup base */
/*@{*/

// {{{ function<Result(Args...)>
template<typename SignatureT> class function;

/**
 * \brief functor API
 * \see handler<bool(Args...)>
 */
template<typename _Result, typename... _Args>
class function<_Result(_Args...)> {
private:
	// {{{ safe_bool
	struct THiddenType { THiddenType *value; };
	typedef THiddenType *THiddenType::* safe_bool;
	// }}}

	// {{{ TFnBase, TUnaryFunctor, TBinaryFunctor
	class TFnBase {
	public:
		typedef _Result (*_Functor)(_Args...);

	public:
		virtual ~TFnBase() { }
		virtual _Result call(_Args... args) const = 0;
		virtual TFnBase *clone() const = 0;
		virtual bool equals(const TFnBase& v) const = 0;
	};

	class TUnaryFunctor : public TFnBase {
	public:
		typedef _Result (*_Functor)(_Args...);

	public:
		explicit TUnaryFunctor(_Functor func) : func(func) { }

		virtual _Result call(_Args... args) const { return func(args...); }
		virtual TUnaryFunctor *clone() const { return new TUnaryFunctor(func); }

		virtual bool equals(const TFnBase& f) const {
			if (const TUnaryFunctor *v = dynamic_cast<const TUnaryFunctor *>(&f))
				return func == v->func;

			return false;
		}

	private:
		_Functor func;
	};

	template<typename _Class>
	class TBinaryFunctor : public TFnBase {
	public:
		typedef _Result (_Class::*_Functor)(_Args...);

	public:
		explicit TBinaryFunctor(_Class *obj, _Functor func) : obj(obj), func(func) { }

		virtual _Result call(_Args... args) const { return (const_cast<_Class *>(obj)->*func)(args...); }
		virtual TBinaryFunctor *clone() const { return new TBinaryFunctor(obj, func); }

		virtual bool equals(const TFnBase& f) const {
			if (const TBinaryFunctor *v = dynamic_cast<const TBinaryFunctor<_Class> *>(&f))
				return this == v || (func == v->func && obj == v->obj);

			return false;
		}

	private:
		_Class *obj;
		_Functor func;
	};
	// }}}

	TFnBase *func;

public:
	function() :
		func(0) {
	}

	template<typename _Func> function(_Func func) :
		func(new TUnaryFunctor(func)) {
	}

	template<typename _Class, typename MemFn> function(_Class *obj, MemFn mf) :
		func(new TBinaryFunctor<_Class>(obj, mf)) {
	}

	function(const function& v) :
		func(v ? v.func->clone() : 0) {
	}

	~function() {
		delete func;
	}

	bool empty() const {
		return !func;
	}

	function *clone() const {
		return new function<_Result(_Args...)>(*this);
	}

	void clear() {
		if (TFnBase *fb = func) {
			func = 0;
			delete fb;
		}
	}

	function& operator=(const function& v) {
		TFnBase *old = func;
		func = &v && v.func ? v.func->clone() : 0;
		delete old;

		return *this;
	}

	bool operator!() const {
		return empty();
	}

	operator safe_bool() const {
		return func ? &THiddenType::value : 0;
	}

	_Result operator()(_Args... args) const {
		return func ? func->call(args...) : _Result();
	}

	bool equals(const function& v) const {
		if (this == &v)
			return true;

		if (func && v.func && func->equals(*v.func))
			return true;

		return false;
	}

	friend bool operator==(const function& a, const function& b) {
		return a.equals(b);
	}

	friend bool operator!=(const function& a, const function& b) {
		return !a.equals(b);
	}
};
// }}}

template<typename _Return, typename _Class, typename... _Args>
inline function<_Return(_Args...)> makeFunctor(_Return(*func)(_Args...)) {
	return function<_Return(_Args...)>(func);
}

template<typename _Return, typename _Class, typename... _Args>
inline function<_Return(_Args...)> makeFunctor(_Return(_Class::*func)(_Args...), const _Class *obj) {
	return function<_Return(_Args...)>(obj, func);
}

template<typename _Return, typename _Class, typename... _Args>
inline function<_Return(_Args...)> makeFunctor(_Return(_Class::*func)(_Args...), _Class *obj) {
	return function<_Return(_Args...)>(obj, func);
}

template<typename _Return, typename _Class, typename... _Args>
inline function<_Return(_Args...)> bindMember(_Return(_Class::*func)(_Args...), const _Class *obj) {
	return function<_Return(_Args...)>(obj, func);
}

template<typename _Return, typename _Class, typename... _Args>
inline function<_Return(_Args...)> bindMember(_Return(_Class::*func)(_Args...), _Class *obj) {
	return function<_Return(_Args...)>(obj, func);
}

/*@}*/

} // namespace x0

#endif
