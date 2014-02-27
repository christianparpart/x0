# Linear IR

### TODO

- renames
  - IRBuilder -> IRBuilderBase
  - IRGenerator -> IRBuilder
  - VMCodeGenerator -> MCBuilder

- IR optimizations
  - constant folding
  - same-BasicBlock merging
  - dead code elimination
  - jump threading
  - efficient register allocator

### Class Hierarchy

````
- IRProgram
- IRBuilder
  - IRGenerator
- VMCodeGenerator
- Value
  - BasicBlock            SSA-conform code sequence block
  - IRVariable            a writable variable
  - Constant              Base class for constants
    - ConstantValue<T>
      - ConstantString    <Buffer>
      - ConstantIP        <IPAddress>
      - ConstantCidr      <Cidr>
      - ConstantRegExp    <RegExp>
      - IRHandler
  - Instr
    - PhiNode             merge branched values 
    - VmInstr             generic non-branching VM instruction
    - AllocaInstr         array allocation
    - ArraySetInstr       array field initialization
    - StoreInstr          store to variable
    - LoadInstr           load from variable
    - BranchInstr
      - CondBrInstr       conditional jump
      - BrInstr           unconditional jump
````

### Useful References

 * http://en.wikipedia.org/wiki/Static_single_assignment_form
 * http://en.wikipedia.org/wiki/Basic_block
 * http://en.wikipedia.org/wiki/Use-define_chain
 * http://en.wikipedia.org/wiki/Compiler_optimization
 * http://llvm.org/doxygen/classllvm_1_1Value.html
