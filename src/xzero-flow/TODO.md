### incomplete TODO

All the things I've found out on the way, that need retouching

- [ ] NativeCallback: replace operator new/delete with `std::variant` + `bool hasDefault_`
- [ ] nested scopes with local variables must be initialized in this block
      currently also initialized in entry block;
      the problem is already how the AST is created.
