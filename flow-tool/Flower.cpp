/* <flow-tool/Flower.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://redmine.trapni.de/projects/x0
 *
 * (c) 2010-2014 Christian Parpart <trapni@gmail.com>
 */
#include "Flower.h"
#include <x0/flow/AST.h>
#include <x0/flow/ASTPrinter.h>
#include <x0/flow/FlowParser.h>
#include <x0/flow/FlowAssemblyBuilder.h>
#include <x0/flow/IRGenerator.h>
#include <x0/flow/TargetCodeGenerator.h>
#include <x0/flow/FlowCallVisitor.h>
#include <x0/flow/ir/IRProgram.h>
#include <x0/flow/ir/IRHandler.h>
#include <x0/flow/ir/BasicBlock.h>
#include <x0/flow/ir/Instr.h>
#include <x0/flow/ir/PassManager.h>
#include <x0/flow/transform/EmptyBlockElimination.h>
#include <x0/flow/transform/InstructionElimination.h>
#include <x0/flow/vm/Runtime.h>
#include <x0/flow/vm/NativeCallback.h>
#include <x0/flow/vm/Runner.h>
#include <fstream>
#include <memory>
#include <utility>
#include <cstdio>
#include <cassert>
#include <cstring>

using namespace x0;

/*
 * TODO
 * - After parsing, traverse throught the AST and translate every assert()
 *   to also pass the string representation of the first argument as last argument.
 *
 */

void reportError(const char *category, const std::string& msg)
{
	printf("%s error: %s\n", category, msg.c_str());
}

Flower::Flower() :
	Runtime(),
    filename_(),
    program_(nullptr),
	totalCases_(0),
	totalSuccess_(0),
	totalFailed_(0),
	dumpAST_(false),
    dumpIR_(false),
    dumpTarget_(false)
{
    // properties
    registerFunction("cwd", FlowType::String).bind(&Flower::flow_getcwd);

    // functions
    registerFunction("random", FlowType::Number).bind(&Flower::flow_random);

    registerFunction("__print", FlowType::Void)
        .params(FlowType::String)
        .bind(&Flower::flow_print);

    registerFunction("log", FlowType::Void)
        .param<FlowString>("message", "<whaaaaat!>")
        .param<FlowNumber>("severity", 42)
        .bind(&Flower::flow_log);

	// unit test aiding handlers
    registerHandler("error")
        .param<FlowString>("message", "")
        .bind(&Flower::flow_error);
    registerHandler("finish").bind(&Flower::flow_finish); // XXX rename to 'success'

    registerHandler("assert")
        .param<bool>("condition")
        .param<FlowString>("description", "")
        .bind(&Flower::flow_assert);

	registerHandler("assert_fail")
        .param<bool>("condition")
        .param<FlowString>("description", "")
        .bind(&Flower::flow_assertFail);

    registerHandler("fail")
        .param<FlowNumber>("a1", 0)
        .param<FlowNumber>("a2", 0)
        .bind(&Flower::flow_fail);

    registerHandler("pass")
        .param<FlowNumber>("a1", 0)
        .param<FlowNumber>("a2", 0)
        .bind(&Flower::flow_pass);

    registerFunction("numbers", FlowType::Void)
        .param<GCIntArray>("values")
        .verifier(&Flower::verify_numbers)
        .bind(&Flower::flow_numbers);

    registerFunction("names", FlowType::Void)
        .param<GCStringArray>("values")
        .bind(&Flower::flow_names);
}

Flower::~Flower()
{
}

bool Flower::import(const std::string& name, const std::string& path, std::vector<FlowVM::NativeCallback*>* builtins)
{
	return false;
}

bool Flower::onParseComplete(Unit* unit)
{
    FlowCallVisitor callv(unit);

    for (auto& call: callv.calls()) {
        if (call->callee()->name() != "assert" && call->callee()->name() != "assert_fail")
            continue;

        ParamList& args = call->args();
        assert(args.size() == 2);

        // add a string argument that equals the expression's source code
        Expr* arg = args.values()[0];

        if (StringExpr* descriptionExpr = dynamic_cast<StringExpr*>(args.values()[1])) {
            std::string description = descriptionExpr->value();
            if (!description.empty()) {
                // XXX Preserve custom passed value.
                continue;
            }
        }

        std::string source = arg->location().text();
        args.replace(1, std::make_unique<StringExpr>(source, FlowLocation()));
    }

	return true;
}

int Flower::runAll(const char *fileName)
{
    filename_ = fileName;

    FlowParser parser(this);

    parser.importHandler = [&](const std::string& name, const std::string& basedir, std::vector<FlowVM::NativeCallback*>*) -> bool {
        fprintf(stderr, "parser.importHandler('%s', '%s')\n", name.c_str(), basedir.c_str());
        return false;
    };

    if (!parser.open(fileName)) {
        fprintf(stderr, "Failed to open file: %s\n", fileName);
        return -1;
    }

    std::unique_ptr<Unit> unit = parser.parse();
    if (!unit) {
        fprintf(stderr, "Failed to parse file: %s\n", fileName);
        return -1;
    }

    if (!verify(unit.get())) {
        fprintf(stderr, "User verification failed.\n");
        return -1;
    }

    onParseComplete(unit.get());

    if (dumpAST_)
        ASTPrinter::print(unit.get());

    program_ = FlowAssemblyBuilder::compile(unit.get());
    if (!program_) {
        fprintf(stderr, "Code generation failed. Aborting.\n");
        return -1;
    }

    if (!program_->link(this)) {
        fprintf(stderr, "Program linking failed. Aborting.\n");
        return -1;
    }

    if (dumpTarget_) {
        program_->dump();
    }

    for (auto handler: program_->handlers()) {
        if (strncmp(handler->name().c_str(), "test_", 5) != 0)
            continue;

        printf("[ -------- ] Testing %s\n", handler->name().c_str());
        totalCases_++;

        bool failed = handler->run(nullptr);
        if (failed)
            totalFailed_++;

        printf("[ -------- ] %s\n\n", failed ? "FAILED" : "OK");
    }

    printf("[ ======== ] %zu tests from %zu cases ran\n", totalSuccess_ + totalFailed_, totalCases_);
    if (totalSuccess_)
        printf("[  PASSED  ] %zu tests\n", totalSuccess_);
    if (totalFailed_)
        printf("[  FAILED  ] %zu tests\n", totalFailed_);

    return totalFailed_;
}

void printDefUseChain(IRProgram* program)
{
    printf("================================================ def-use chain\n");
    for (IRHandler* handler: program->handlers()) {
        printf("handler:\n");
        for (BasicBlock* bb: handler->basicBlocks()) {
            printf("bb:\n");
            for (Instr* instr: bb->instructions()) {
                printf("def : ");
                instr->dump();
                for (Instr* use: instr->uses()) {
                    printf("use : ");
                    use->dump();
                }
                if (instr->uses().empty()) {
                    printf("no uses\n");
                }
                printf("\n");
            }
            printf("\n");
        }
        printf("\n");
    }
}

int Flower::run(const char* fileName, const char* handlerName)
{
	if (!handlerName || !*handlerName) {
		printf("No handler specified.\n");
		return -1;
	}

	filename_ = fileName;

	FlowParser parser(this);

    parser.importHandler = [&](const std::string& name, const std::string& basedir, std::vector<FlowVM::NativeCallback*>*) -> bool {
		fprintf(stderr, "parser.importHandler('%s', '%s')\n", name.c_str(), basedir.c_str());
		return false;
	};

	if (!parser.open(fileName)) {
		fprintf(stderr, "Failed to open file: %s\n", fileName);
		return -1;
	}

	std::unique_ptr<Unit> unit = parser.parse();
	if (!unit) {
		fprintf(stderr, "Failed to parse file: %s\n", fileName);
		return -1;
	}

    if (!verify(unit.get())) {
        fprintf(stderr, "User verification failed.\n");
        return -1;
    }

    onParseComplete(unit.get());

	if (dumpAST_)
		ASTPrinter::print(unit.get());

	Handler* handlerSym = unit->findHandler(handlerName);
	if (!handlerSym) {
		fprintf(stderr, "No handler with name '%s' found in unit '%s'.\n", handlerName, fileName);
		return -1;
	}

#if 0
    program_ = FlowAssemblyBuilder::compile(unit.get());
#else
    IRProgram* ir = IRGenerator::generate(unit.get());
    if (!ir) {
        fprintf(stderr, "IR generation failed. Aborting.\n");
        return -1;
    }

    if (1) {
        PassManager pm;
        pm.registerPass(std::make_unique<EmptyBlockElimination>());
        pm.registerPass(std::make_unique<InstructionElimination>());
        pm.run(ir);
    }

    if (dumpIR_) {
        ir->dump();
    }

    program_ = TargetCodeGenerator().generate(ir);
#endif

    if (!program_) {
        fprintf(stderr, "Code generation failed. Aborting.\n");
        return -1;
    }

    if (!program_->link(this)) {
        fprintf(stderr, "Program linking failed. Aborting.\n");
        return -1;
    }

    if (dumpTarget_) {
        program_->dump();
    }

    FlowVM::Handler* handler = program_->findHandler(handlerName);
    assert(handler != nullptr);

    printf("Running handler %s ...\n", handlerName);
    return handler->run(nullptr /*userdata*/ );
}

void Flower::dump()
{
    program_->dump();
}

void Flower::flow_print(FlowVM::Params& args)
{
	printf("%s\n", args.get<FlowString*>(1)->str().c_str());
}

void Flower::flow_log(FlowVM::Params& args)
{
    FlowString* message = args.get<FlowString*>(1);
    FlowNumber severity = args.get<FlowNumber>(2);

    printf("<%lu> %s\n", severity, message->str().c_str());
}

void Flower::flow_assert(FlowVM::Params& args)
{
	const FlowString* sourceValue = args.get<FlowString*>(2);

    if (!args.get<bool>(1)) {
		printf("[   FAILED ] %s\n", sourceValue->str().c_str());
		args.setResult(true);
	} else {
		printf("[       OK ] %s\n", sourceValue->str().c_str());
		++totalSuccess_;
		args.setResult(false);
	}
}

void Flower::flow_getcwd(FlowVM::Params& args)
{
    char buf[PATH_MAX];

    args.setResult(getcwd(buf, sizeof(buf)) ? buf : strerror(errno));
}

void Flower::flow_random(FlowVM::Params& args)
{
    srand(time(NULL));
    args.setResult(random());
}

void Flower::flow_getenv(FlowVM::Params& args)
{
	args.setResult(getenv(args.get<FlowString>(1).str().c_str()));
}

void Flower::flow_error(FlowVM::Params& args)
{
    if (args.size() == 2)
        printf("Error. %s\n", args.get<FlowString>(1).str().c_str());
    else
        printf("Error\n");

    args.setResult(true);
}

void Flower::flow_finish(FlowVM::Params& args)
{
    args.setResult(true);
}

void Flower::flow_fail(FlowVM::Params& args)
{
    args.setResult(true);
}

void Flower::flow_pass(FlowVM::Params& args)
{
    args.setResult(false);
}

void Flower::flow_assertFail(FlowVM::Params& args)
{
    if (args.get<bool>(1)) {
        fprintf(stderr, "Assertion failed. %s\n", args.get<FlowString>(2).str().c_str());
        args.setResult(true);
    } else {
        args.setResult(false);
    }
}

bool Flower::verify_numbers(CallExpr* call)
{
    printf("Verify numbers!\n");

    const ArrayExpr* array = (const ArrayExpr*) call->args()[0].second;

    for (size_t i = 0, e = array->values().size(); i != e; ++i) {
        const Expr* arg = array->values()[i].get();

        if (const auto e = dynamic_cast<const NumberExpr*>(arg)) {
            if (e->value() % 2) {
                printf("Odd numbers not allowed.\n");
                return false;
            }
        } else {
            printf("call args to numbers() must be literal.\n");
            return false;
        }
    }

    return true;
}

void Flower::flow_numbers(FlowVM::Params& args)
{
    GCIntArray* array = args.get<GCIntArray*>(1);

    for (FlowNumber value: array->data()) {
        printf("number: %li\n", value);
    }
}

void Flower::flow_names(FlowVM::Params& args)
{
    GCStringArray* array = args.get<GCStringArray*>(1);

    for (const FlowString& value: array->data()) {
        printf("string: %s\n", value.str().c_str());
    }
}

