
### Flow TODO

- TODO renames
  - IRBuilder -> IRBuilderBase
  - IRGenerator -> IRBuilder
  - VMCodeGenerator -> TargetCodeBuilder

- fixme: `voidfunc;` parse error, should equal to `voidfunc();` and `voidfunc\n`
- method overloading (needs updates to symbol lookup, not just by name but by signature)
  - allows us to provide multiple implementations for example: workers(I)V and workers(i)V

- update documentation
  - manual page: x0d.conf.5
  - manual page: x0d.8
  - manual page: flow.7
