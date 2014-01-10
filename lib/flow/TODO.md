
### Incomplete Flow Rewrite TODO

- string interpolation
  - parse
  - AST
  - codegen
- flowtool: auto-add source string as 2nd arg to `assert` + `assert_fail` handlers, via old onParseComplete hook
- array type support or an alternative to express `compress.types(StringArray)`
- IPv6 support in Cidr.contains(IPAddress)

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

