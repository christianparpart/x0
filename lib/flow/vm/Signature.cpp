#include <x0/flow/vm/Signature.h>
#include <vector>
#include <string>

namespace x0 {
namespace FlowVM {

Signature::Signature() :
    name_(),
    returnType_(FlowType::Void),
    args_()
{
}

Signature::Signature(const std::string& signature) :
    name_(),
    returnType_(FlowType::Void),
    args_()
{
    // signature  ::= NAME [ '(' args ')' returnType
    // args       ::= type*
    // returnType ::= primitive | 'V'
    // type       ::= array | assocArray | primitive
    // array      ::= '[' primitive
    // assocArray ::= '>' primitive primitive
    // primitive  ::= 'B' | 'I' | 'S' | 'P' | 'C' | 'R' | 'H'

    enum class State {
        END         = 0,
        Name        = 1,
        ArgsBegin   = 2,
        Args        = 3,
        ReturnType  = 4
    };

    const char* i = signature.data();
    const char* e = signature.data() + signature.size();
    State state = State::Name;

    while (i != e) {
        switch (state) {
            case State::Name:
                if (*i == '(') {
                    state = State::ArgsBegin;
                }
                ++i;
                break;
            case State::ArgsBegin:
                name_ = std::string(signature.data(), i - signature.data() - 1);
                state = State::Args;
                break;
            case State::Args:
                if (*i == ')') {
                    state = State::ReturnType;
                } else {
                    args_.push_back(typeSignature(*i));
                }
                ++i;
                break;
            case State::ReturnType:
                returnType_ = typeSignature(*i);
                state = State::END;
                ++i;
                break;
            case State::END:
                fprintf(stderr, "Garbage at end of signature string. %s\n", i);
                i = e;
                break;
        }
    }

    if (state != State::END) {
        fprintf(stderr, "Premature end of signature string. %s\n", signature.c_str());
    }
}

std::string Signature::to_s() const
{
    std::string result = name_;
    result += "(";
    for (FlowType t: args_)
        result += signatureType(t);
    result += ")";
    result += signatureType(returnType_);
    return result;
}

FlowType typeSignature(char ch)
{
    switch (ch) {
        case 'V': return FlowType::Void;
        case 'B': return FlowType::Boolean;
        case 'I': return FlowType::Number;
        case 'S': return FlowType::String;
        case 'P': return FlowType::IPAddress;
        case 'C': return FlowType::Cidr;
        case 'R': return FlowType::RegExp;
        case 'H': return FlowType::Handler;
        case '[': return FlowType::Array;
        case '>': return FlowType::AssocArray;
        default: return FlowType::Void; //XXX
    }
}

char signatureType(FlowType t)
{
    switch (t) {
        case FlowType::Void: return 'V';
        case FlowType::Boolean: return 'B';
        case FlowType::Number: return 'I';
        case FlowType::String: return 'S';
        case FlowType::IPAddress: return 'P';
        case FlowType::Cidr: return 'C';
        case FlowType::RegExp: return 'R';
        case FlowType::Handler: return 'H';
        case FlowType::Array: return '[';
        case FlowType::AssocArray: return '>';
        default: return '?';
    }
}

} // namespace FlowVM
} // namespace x0
