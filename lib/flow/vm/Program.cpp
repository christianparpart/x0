#include <x0/flow/vm/Program.h>
#include <x0/flow/vm/Handler.h>
#include <x0/flow/vm/Instruction.h>
#include <x0/flow/vm/Runtime.h>
#include <x0/flow/vm/Runner.h>
#include <x0/flow/vm/Match.h>
#include <utility>
#include <vector>
#include <memory>
#include <new>

namespace x0 {
namespace FlowVM {

/* {{{ possible binary file format
 * ----------------------------------------------
 * u32                  magic number (0xbeafbabe)
 * u32                  version
 * u64                  flags
 * u64                  register count
 * u64                  code start
 * u64                  code size
 * u64                  integer const-table start
 * u64                  integer const-table element count
 * u64                  string const-table start
 * u64                  string const-table element count
 * u64                  regex const-table start (stored as string)
 * u64                  regex const-table element count
 * u64                  debug source-lines-table start
 * u64                  debug source-lines-table element count
 *
 * u32[]                code segment
 * u64[]                integer const-table segment
 * u64[]                string const-table segment
 * {u32, u8[]}[]        strings
 * {u32, u32, u32}[]    debug source lines segment
 */ // }}}

Program::Program() :
    numbers_(),
    strings_(),
    ipaddrs_(),
    cidrs_(),
    regularExpressions_(),
    matches_(),
    modules_(),
    nativeHandlerSignatures_(),
    nativeFunctionSignatures_(),
    nativeHandlers_(),
    nativeFunctions_(),
    handlers_(),
    runtime_(nullptr)
{
}

Program::Program(
        const std::vector<FlowNumber>& numbers,
        const std::vector<std::string>& strings,
        const std::vector<IPAddress>& ipaddrs,
        const std::vector<Cidr>& cidrs,
        const std::vector<std::string>& regularExpressions,
        const std::vector<MatchDef>& matches,
        const std::vector<std::pair<std::string, std::string>>& modules,
        const std::vector<std::string>& nativeHandlerSignatures,
        const std::vector<std::string>& nativeFunctionSignatures,
        const std::vector<std::pair<std::string, std::vector<Instruction>>>& handlers) :
    numbers_(numbers),
    strings_(),
    ipaddrs_(ipaddrs),
    cidrs_(cidrs),
    regularExpressions_(),
    matches_(),
    modules_(modules),
    nativeHandlerSignatures_(nativeHandlerSignatures),
    nativeFunctionSignatures_(nativeFunctionSignatures),
    nativeHandlers_(),
    nativeFunctions_(),
    handlers_(),
    runtime_(nullptr)
{
    strings_.resize(strings.size());
    for (size_t i = 0, e = strings.size(); i != e; ++i) {
        auto& one = strings_[i];
        one.first = strings[i];
        one.second = BufferRef(one.first);
    }

    for (const auto& s: regularExpressions)
        regularExpressions_.push_back(new RegExp(s));

    for (const auto& handler: handlers)
        createHandler(handler.first, handler.second);

    setup(matches);
}

Program::~Program()
{
    for (auto& handler: handlers_)
        delete handler;

    for (auto re: regularExpressions_)
        delete re;

    for (auto m: matches_)
        delete m;
}

void Program::setup(const std::vector<MatchDef>& matches)
{
    for (size_t i = 0, e = matches.size(); i != e; ++i) {
        const MatchDef& def = matches[i];
        switch (def.op) {
            case MatchClass::Same:
                matches_.push_back(new MatchSame(def, this));
                break;
            case MatchClass::Head:
                matches_.push_back(new MatchHead(def, this));
                break;
            case MatchClass::Tail:
                matches_.push_back(new MatchTail(def, this));
                break;
            case MatchClass::RegExp:
                matches_.push_back(new MatchRegEx(def, this));
                break;
        }
    }
}

Handler* Program::createHandler(const std::string& name)
{
    Handler* handler = new Handler(this, name, {});
    handlers_.push_back(handler);
    return handler;
}

Handler* Program::createHandler(const std::string& name, const std::vector<Instruction>& instructions)
{
    Handler* handler = new Handler(this, name, instructions);
    handlers_.push_back(handler);

    return handler;
}

Handler* Program::findHandler(const std::string& name) const
{
    for (auto handler: handlers_)
        if (handler->name() == name)
            return handler;

    return nullptr;
}

int Program::indexOf(const Handler* handler) const
{
    for (int i = 0, e = handlers_.size(); i != e; ++i)
        if (handlers_[i] == handler)
            return i;

    return -1;
}

void Program::dump()
{
    printf("; Program\n");

    if (!modules_.empty()) {
        printf("\n; Modules\n");
        for (size_t i = 0, e = modules_.size(); i != e; ++i) {
            if (modules_[i].second.empty())
                printf(".module '%s'\n", modules_[i].first.c_str());
            else
                printf(".module '%s' from '%s'\n", modules_[i].first.c_str(), modules_[i].second.c_str());
        }
    }

    if (!nativeFunctionSignatures_.empty()) {
        printf("\n; External Functions\n");
        for (size_t i = 0, e = nativeFunctionSignatures_.size(); i != e; ++i) {
            if (nativeFunctions_[i])
                printf(".extern function %3zu = %-20s ; linked to %p\n", i, nativeFunctionSignatures_[i].c_str(), nativeFunctions_[i]);
            else
                printf(".extern function %3zu = %-20s\n", i, nativeFunctionSignatures_[i].c_str());
        }
    }

    if (!nativeHandlerSignatures_.empty()) {
        printf("\n; External Handlers\n");
        for (size_t i = 0, e = nativeHandlerSignatures_.size(); i != e; ++i) {
            if (nativeHandlers_[i])
                printf(".extern handler %4zu = %-20s ; linked to %p\n", i, nativeHandlerSignatures_[i].c_str(), nativeHandlers_[i]);
            else
                printf(".extern handler %4zu = %-20s\n", i, nativeHandlerSignatures_[i].c_str());
        }
    }

    if (!numbers_.empty()) {
        printf("\n; Integer Constants\n");
        for (size_t i = 0, e = numbers_.size(); i != e; ++i) {
            printf(".const integer %5zu = %li\n", i, (FlowNumber) numbers_[i]);
        }
    }

    if (!strings_.empty()) {
        printf("\n; String Constants\n");
        for (size_t i = 0, e = strings_.size(); i != e; ++i) {
            printf(".const string %6zu = '%s'\n", i, strings_[i].first.c_str());
        }
    }

    if (!ipaddrs_.empty()) {
        printf("\n; IP Constants\n");
        for (size_t i = 0, e = ipaddrs_.size(); i != e; ++i) {
            printf(".const ipaddr %6zu = %s\n", i, ipaddrs_[i].str().c_str());
        }
    }

    if (!cidrs_.empty()) {
        printf("\n; CIDR Constants\n");
        for (size_t i = 0, e = cidrs_.size(); i != e; ++i) {
            printf(".const cidr %8zu = %s\n", i, cidrs_[i].str().c_str());
        }
    }

    if (!regularExpressions_.empty()) {
        printf("\n; Regular Expression Constants\n");
        for (size_t i = 0, e = regularExpressions_.size(); i != e; ++i) {
            printf(".const regex %7zu = /%s/\n", i, regularExpressions_[i]->c_str());
        }
    }

    if (!matches_.empty()) {
        printf("\n; Match Table\n");
        for (size_t i = 0, e = matches_.size(); i != e; ++i) {
            const MatchDef& def = matches_[i]->def();
            printf(".const match %7zu = handler %zu, op %s, elsePC %lu ; %s\n",
                i,
                def.handlerId,
                tos(def.op).c_str(),
                def.elsePC,
                handler(def.handlerId)->name().c_str()
            );

            for (size_t k = 0, m = def.cases.size(); k != m; ++k) {
                const MatchCaseDef& one = def.cases[k];

                printf("                       case %3zu = label %2zu, pc %4zu ; ", k, one.label, one.pc);

                if (def.op == MatchClass::RegExp) {
                    printf("/%s/\n", regularExpressions_[one.label]->c_str());
                } else {
                    printf("'%s'\n", strings_[one.label].first.c_str());
                }
            }
        }
    }

    for (size_t i = 0, e = handlers_.size(); i != e; ++i) {
        Handler* handler = handlers_[i];
        printf("\n.handler %-20s ; #%zu (%zu registers, %zu instructions)\n",
                handler->name().c_str(),
                i,
                handler->registerCount() ? handler->registerCount() - 1 : 0, // r0 is never used
                handler->code().size()
        );
        handler->disassemble();
    }

    printf("\n\n");
}

/**
 * Maps all native functions/handlers to their implementations (report unresolved symbols)
 *
 * \param runtime the runtime to link this program against, resolving any external native symbols.
 * \retval true Linking succeed.
 * \retval false Linking failed due to unresolved native signatures not found in the runtime.
 */
bool Program::link(Runtime* runtime)
{
    runtime_ = runtime;
    int errors = 0;

    // load runtime modules
    for (const auto& module: modules_) {
        if (!runtime->import(module.first, module.second, nullptr)) {
            errors++;
        }
    }

    // link nattive handlers
    nativeHandlers_.resize(nativeHandlerSignatures_.size());
    size_t i = 0;
    for (const auto& signature: nativeHandlerSignatures_) {
        // map to nativeHandlers_[i]
        if (NativeCallback* cb = runtime->find(signature)) {
            nativeHandlers_[i] = cb;
        } else {
            nativeHandlers_[i] = nullptr;
            fprintf(stderr, "Unresolved native handler signature: %s\n", signature.c_str());
            // TODO unresolvedSymbols_.push_back(signature);
            errors++;
        }
        ++i;
    }

    // link nattive functions
    nativeFunctions_.resize(nativeFunctionSignatures_.size());
    i = 0;
    for (const auto& signature: nativeFunctionSignatures_) {
        if (NativeCallback* cb = runtime->find(signature)) {
            nativeFunctions_[i] = cb;
        } else {
            nativeFunctions_[i] = nullptr;
            fprintf(stderr, "Unresolved native function signature: %s\n", signature.c_str());
            // TODO unresolvedSymbols_.push_back(signature);
            errors++;
        }
        ++i;
    }

    return errors == 0;
}

} // namespace FlowVM
} // namespace x0
