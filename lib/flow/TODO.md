
### Flow TODO

- parser:
  - FIXME: handler calls w/o arguments and trailing space: `staticfile;`
  - FIXME: `var i = 42; i = i / 2;` second stmt fails due to regex parsing attempts.
- IR:
  - TargetCodeGenerator: builtin function call
  - TargetCodeGenerator: builtin handler call
- fixme: `voidfunc;` parse error, should equal to `voidfunc();` and `voidfunc\n`
- method overloading (needs updates to symbol lookup, not just by name but by signature)
  - allows us to provide multiple implementations for example: workers(I)V and workers(i)V

- update documentation
  - manual page: x0d.conf.5
  - manual page: x0d.8
  - manual page: flow.7
