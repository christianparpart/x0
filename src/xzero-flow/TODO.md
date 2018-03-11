### incomplete TODO

All the things I've found out on the way, that need retouching

- [x] FlowParser.cc: `enter(new SymbolTable(...))`
- [x] FlowParser.cc: `new Handler(...)`
- [x] FlowParser.h: `new SymbolTable(...)`
- [x] FlowParser.h: `new T(...)`
- [ ] NativeCallback: replace operator new/delete with `std::variant` + `bool hasDefault_`
- [x] TargetCodeGenerator: `stack_` vs `sp_` vs `variables_`
- [ ] nested scopes with local variables must be initialized in this block
      currently also initialized in entry block;
      the problem is already how the AST is created.
