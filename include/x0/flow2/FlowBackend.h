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

typedef std::function<void(FlowContext* context, FlowParams& args)> FlowCallback;

class X0_API FlowBackend {
private:
	struct Callback { // {{{
		std::string name;
		FlowCallback function;
		std::vector<FlowType> signature;

		Callback(const std::string& _name, const FlowCallback& _callback, FlowType _returnType) :
			name(_name),
			function(_callback),
			signature()
		{
			signature.push_back(_returnType);
		}

		void invoke(FlowContext* cx, FlowParams& args) {
			function(cx, args);
		}

		template<typename... ArgTypes>
		static Callback makeFunction(const std::string& name, const FlowCallback& function, FlowType rt, ArgTypes... args) {
			Callback cb(name, function, rt);
			cb.push_back(args...);
			return cb;
		}

		static Callback makeHandler(const std::string& name, const FlowCallback& function) {
			return Callback(name, function, FlowType::Boolean);
		}

		template<typename Arg1, typename... Args>
		void push_back(Arg1 a1, Args... args) {
			signature.push_back(a1);
			push_back(args...);
		}
	}; // }}}
	std::vector<Callback> callbacks_;

public:
	FlowBackend();
	virtual ~FlowBackend();

	virtual bool import(const std::string& name, const std::string& path) = 0;

	bool contains(const std::string& name) const;
	bool registerHandler(const std::string& name, const FlowCallback& fn);
	template<typename... Args>
	bool registerFunction(const std::string& name, const FlowCallback& fn, FlowType returnType, Args... args);
};

} // namespace x0
