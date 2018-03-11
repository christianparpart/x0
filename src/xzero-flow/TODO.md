### incomplete TODO

All the things I've found out on the way, that need retouching

- [ ] FlowParser.cc: `enter(new SymbolTable(...))`
- [ ] FlowParser.cc: `new Handler(...)`
- [ ] FlowParser.h: `new SymbolTable(...)`
- [ ] FlowParser.h: `new T(...)`
- [ ] NativeCallback: replace operator new/delete with `std::variant` + `bool hasDefault_`
- [x] TargetCodeGenerator: `stack_` vs `sp_` vs `variables_`
- [ ] nested scopes with local variables must be initialized in this block
      currently also initialized in entry block;
      the problem is already how the AST is created.
