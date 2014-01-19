
### Incomplete Flow Rewrite TODO

- fixme: `voidfunc;` parse error, should equal to `voidfunc();` and `voidfunc\n`
- method overloading (needs updates to symbol lookup, not just by name but by signature)
  - allows us to provide multiple implementations for example: workers(I)V and workers(i)V
- DONE: proper plugin unloading
- DONE: plugin loading
- DONE: array type support for string arrays and int arrays
- DONE: IPv6 support in Cidr.contains(IPAddress)
- DONE: parser: call statement parsing multiline args and postscript conditionals
- DONE: flowtool: auto-add source string as 2nd arg to `assert` + `assert_fail` handlers, via old onParseComplete hook
- DONE: fully-working string interpolation
- DONE: logical ops must be treated as boolean results for proper type checking (`assert true and not false` should work)
- DONE: unary operator: not

- 'match' stmt
  - DONE: AST
  - DONE: syntax
  - DONE: semantic validation (case expr must have same base type and match op must be compatible to this one)
  - DONE: code generation
  - DONE: vm: == (MatchSame)
  - DONE: vm: =^ (MatchHead)
  - DONE: vm: =$ (MatchTail)
  - DONE: vm: =~ (MatchRegEx)
  - DONE: regex match context
- DONE: codegen: native function
- DONE: codegen: custom handler invokation
- DONE: sema: native handler/function signature check (as a pass between parse and codegen)

- data type code generation (for passing data to native functions/handlers)
  - DONE: number
  - DONE: boolean
  - DONE: string
  - DONE: ipaddr
  - DONE: cidr

- expression operator implementation
  - DONE: `expr and expr`
  - DONE: `expr xor expr`
  - DONE: `expr or expr`
  - DONE: `bool == bool`
  - DONE: `bool != bool`
  - DONE: (num, num): `== != < > <= >= + - * / mod shl shr & | ^ **`
  - DONE: (string, string): `== != < > <= >= + in`
  - DONE: (ip, ip): `== !=`
  - DONE: (cidr, cidr): `== !=`
  - DONE: (ip, cidr): `in`
  - DONE: (string, regex): `=~`

