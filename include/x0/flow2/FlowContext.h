/* 
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/flow2/FlowValue.h>
#include <unordered_map>
#include <deque>

namespace x0 {

class ASTNode;
class Symbol;
class FlowEngine;

// XXX we might want to add debug/runtime info here, that aids some diagnostics

class FlowContext {
private:
	// RegExp::Match* regexpMatch_;

public:
	FlowContext() {}
	virtual ~FlowContext() {}
};

} // namespace x0 
