#pragma once

#include <x0/Api.h>
#include <x0/flow2/FlowValue.h>

#include <utility>
#include <memory>
#include <vector>
#include <string>
#include <x0/Api.h>

namespace x0 {

class FlowContext;

typedef std::function<void(FlowParams& args, FlowContext* cx)> FlowCallback;

class X0_API FlowBackend {
public:
	struct Callback { // {{{
		std::string name_;
		FlowCallback function_;
		std::vector<FlowType> signature_;

		const std::string name() const { return name_; }
		const std::vector<FlowType>& signature() const { return signature_; }

		Callback(const std::string& _name, FlowType _returnType) :
			name_(_name),
			function_(),
			signature_()
		{
			signature_.push_back(_returnType);
		}

		Callback(const std::string& _name, const FlowCallback& _builtin, FlowType _returnType) :
			name_(_name),
			function_(_builtin),
			signature_()
		{
			signature_.push_back(_returnType);
		}

		void invoke(FlowParams& args, FlowContext* cx) {
			function_(args, cx);
		}

		template<typename Arg1>
		Callback& signature(Arg1 a1) {
			signature_.push_back(a1);
			return *this;
		}

		template<typename Arg1, typename... Args>
		Callback& signature(Arg1 a1, Args... more) {
			signature_.push_back(a1);
			return signature(more...);
		}

		Callback& callback(const FlowCallback& cb) {
			function_ = cb;
			return *this;
		}

		Callback& operator()(const FlowCallback& cb) {
			function_ = cb;
			return *this;
		}

		// static factory

		template<typename... ArgTypes>
		static Callback makeFunction(const std::string& name, const FlowCallback& function, FlowType rt, ArgTypes... args) {
			Callback cb(name, function, rt);
			cb.signature(args...);
			return cb;
		}

		static Callback makeHandler(const std::string& name) {
			return Callback(name, FlowType::Boolean);
		}
	}; // }}}

private:
	std::vector<Callback> builtins_;

public:
	FlowBackend();
	virtual ~FlowBackend();

	virtual bool import(const std::string& name, const std::string& path) = 0;

	bool contains(const std::string& name) const;
	int find(const std::string& name) const;
	const std::vector<Callback>& builtins() const { return builtins_; }

	Callback& registerHandler(const std::string& name);

//	template<typename... Args>
//	Callback& registerFunction(const std::string& name, const FlowCallback& fn, FlowType returnType, Args... args);

	void invoke(int id, int argc, FlowValue* argv, FlowContext* cx);
};

//inline void FlowBackend::invoke(int id, int argc, FlowValue* argv, FlowContext* cx)
//{
//	FlowArray args(argc, argv);
//	return builtins_[id].invoke(args, cx);
//}

} // namespace x0
