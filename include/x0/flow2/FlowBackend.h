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

typedef std::function<bool(FlowContext* context)> FlowHandlerCB;
typedef std::function<void(FlowContext* context, FlowParams& args)> FlowFunctionCB;

class FlowBackend {
private:
	std::vector<FlowHandlerCB> handlers_;
	std::vector<FlowFunctionCB> functions_;

public:
	virtual bool import(const std::string& name, const std::string& path);

	void registerHandler(const std::string& name, const FlowHandlerCB& fn);
	void registerFunction(const std::string& name, const FlowFunctionCB& fn);

//	template<typename Result, typename... Args>
//	void registerFunction(const std::string& name, const FlowFunctionCB& fn);

//	template<Result(typename... Args)>
//	void registerFunction(const std::string& name, const FlowFunctionCB& fn);
};

} // namespace x0
