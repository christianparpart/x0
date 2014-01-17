Flow is a domain specific language to provide very flexible custom control flows to
host applications, such as an HTTP web server to let the operator control the
way your app the way he things.

## Flow Virtual Machine

The *Flow Virtual Machine* is the backend to the *Flow Language* that implements
a bytecode VM interpreter to execute your control flow as fast as possible.

### TODO

- find solution to support `compress.types(string_array)` alike signatures
- find solution to implement effective named based vhost matching (`vhost.mapping`)
- `match`-keyword, support multi-branch instruction (merely like tableswitch in JVM)
- regex primitive type
- direct-threaded VM impl and token-threaded-to-direct-threaded opcode transformer

### Data Types

#### Numbers

Numbers are 64-bit signed. Booleans are represented as numbers.

#### Strings

All strings in flow are immutable. So all string instructions do not disinguish between
strings from constant table, dynamically allocated strings, or strings as retrieved from
another virtual machine instruction (such as a native function call).

#### Handler References

Handler references are passed as an index value into the programs handler table.
The native callee has to resolve this index into the handler's object itself.

#### IP Addresses

(IPv4 and IPv6)

IPv4 could be directly represented via the least significant 32-bit from a number.

#### Cidr Network Notations

    10.10.0.0/19
    3ffe::/16

#### Arrays

    ["text/plain", "application/octet-stream"]

    [0, 2, 4, 6, 8]

### Instruction Stream

The program is stored as an array (stream) of fixed-length 32-bit instructions.

The opcode always takes the least-significant 8-bit of an instruction, also determining the
interpretation of the higher 24 bits to one of the variations as described below.

A register is always represented as an 8-bit index to the register array, effectively limiting
the total number of registers to 256.

Registers does not necessarily require them to be located in a CPU hardware
but can also be represented as software array.

In the following tables, the values have the following meaning:

- *OP*: opcode
- *A*: first operand
- *B*: second operand
- *C*: third operand

Instructions are represented as follows:

    0    8  16  24  32  40  48  56  64
    +----+---+---+---+---+---+---+---+      Instruction with no operands.
    |   OP   |                       |
    +----+---+---+---+---+---+---+---+

    0    8  16  24  32  40  48  56  64
    +----+---+---+---+---+---+---+---+      Instruction with one operand.
    |   OP   |   A   |               |
    +----+---+---+---+---+---+---+---+

    0    8  16  24  32  40  48  56  64
    +----+---+---+---+---+---+---+---+      Instruction with two operands.
    |   OP   |   A   |   B   |       |
    +----+---+---+---+---+---+---+---+

    0    8  16  24  32  40  48  56  64
    +----+---+---+---+---+---+---+---+      Instruction with three operands.
    |   OP   |   A   |   B   |   C   |
    +----+---+---+---+---+---+---+---+



### Constants

Constants are all stored in a constant table, each type of constants in its own table.

- integer constants: 64-bit signed
- string constants: raw string plus its string length
- regular expression constants: defined as strings, but may be pre-compiled into dedicated AST for faster execution during runtime.

### Opcodes

#### Instruction Prefixes

 - *V* - variable
 - *N* - integer constant
 - *I* - immediate 16-bit integer literal
 - *S* - string constant
 - *P* - IP address
 - *R* - regular expression
 - *A* - array

#### Instruction Operand Types

- *imm* - immediate literal values
- *num* - offset into the register array, cast to an integer.
- *str* - offset into the register array, cast to a string object.
- *var* - immediate offset into the register array, any type.
- *vres* - same as *var* but used by to store the instruction's result.
- *vbase* - same as *var* but used to denote the first of a consecutive list of registers.
- *pc* - jump program offsets, immedate offset into the program's instruction array

#### Debug Ops

    Opcode  Mnemonic  A       B             Description
    --------------------------------------------------------------------------------------------
    0x??    NTICKS    vres    -             dumps performance instruction counter into A
    0x??    NDUMPN    vbase   imm           dumps register contents of consecutive registers [vbase, vbase+N]

#### Conversion Ops

    Opcode  Mnemonic  A       B             Description
    --------------------------------------------------------------------------------------------
    0x??    S2I       vres    str           A = atoi(B)
    0x??    I2S       vres    num           A = itoa(B)
    0x??    P2S       vres    ip            A = ip(B).toString()
    0x??    C2S       vres    cidr          A = cidr(B).toString()
    0x??    R2S       vres    regex         A = regex(B).toString()
    0x??    SURLENC   vres    str           A = urlencode(B)
    0x??    SURLDEC   vres    str           A = urldecode(B)

#### Unary Ops

    Opcode  Mnemonic  A       B             Description
    --------------------------------------------------------------------------------------------
    0x??    MOV       vres    var           A = B         /* raw register value copy */

#### Numerical Ops

    Opcode  Mnemonic  A       B     C       Description
    --------------------------------------------------------------------------------------------
    0x??    NCONST    vres    imm   -       set integer A to value at constant integer pool's offset B
    0x??    IMOV      vres    imm   -       set integer A to immediate 16-bit integer literal B
    0x??    INEG      vres    imm   -       A = -B
    0x??    NNEG      vres    var   -       A = -B
    0x??    NNOT      vres    var   -       A = ~B
    0x??    NADD      vres    var   var     A = B + C
    0x??    NSUB      vres    var   var     A = B - C
    0x??    NMUL      vres    var   var     A = B * C
    0x??    NDIV      vres    var   var     A = B / C
    0x??    NREM      vres    var   var     A = B % C
    0x??    NSHL      vres    var   var     A = B << C
    0x??    NSHR      vres    var   var     A = B >> C
    0x??    NPOW      vres    var   var     A = B ** C
    0x??    NAND      vres    var   var     A = B & C
    0x??    NOR       vres    var   var     A = B | C
    0x??    NXOR      vres    var   var     A = B ^ C
    0x??    NCMPEQ    vres    var   var     A = B == C
    0x??    NCMPNE    vres    var   var     A = B != C
    0x??    NCMPLE    vres    var   var     A = B <= C
    0x??    NCMPGE    vres    var   var     A = B >= C
    0x??    NCMPLT    vres    var   var     A = B < C
    0x??    NCMPGT    vres    var   var     A = B > C

#### String Ops

    Opcode  Mnemonic  A       B     C       Description
    --------------------------------------------------------------------------------------------
    0x??    SCONST    vres    imm   -       A = stringConstantPool[B]
    0x??    SADD      vres    str   str     A = B + C
    0x??    SSUBSTR   vres    str   -       A = substr(B, C /* offset */, C + 1 /* count */)
    0x??    SCMPEQ    vres    str   str     A = B == C
    0x??    SCMPNE    vres    str   str     A = B != C
    0x??    SCMPLE    vres    str   str     A = B <= C
    0x??    SCMPGE    vres    str   str     A = B >= C
    0x??    SCMPLT    vres    str   str     A = B < C
    0x??    SCMPGT    vres    str   str     A = B > C
    0x??    SCMPBEG   vres    str   str     A = B =^ C
    0x??    SCMPEND   vres    str   str     A = B =$ C
    0x??    SCMPSET   vres    str   str     A = B in C
    0x??    SREGMATCH vres    str   regex   A = B =~ C
    0x??    SREGGROUP vres    num   -       A = regex_group(B /* regex-context offset */)
    0x??    SLEN      vres    str   -       A = strlen(B)
    0x??    SMATCHEQ  str     imm   -       $pc = MatchSame[A].evaluate(str);
    0x??    SMATCHBEG str     imm   -       $pc = MatchBegin[A].evaluate(str);
    0x??    SMATCHEND str     imm   -       $pc = MatchEnd[A].evaluate(str);
    0x??    SMATCHR   str     imm   -       $pc = MatchRegEx[A].evaluate(str);

#### IP Address Ops

    Opcode  Mnemonic  A       B     C       Description
    --------------------------------------------------------------------------------------------
    0x??    PCONST    vres    imm   -       A = ipConstantPool[B]
    0x??    PCMPEQ    vres    ip    ip      A = ip(B) == ip(C)
    0x??    PCMPNE    vres    ip    ip      A = ip(B) != ip(C)

#### Cidr Ops

    Opcode  Mnemonic  A       B     C       Description
    --------------------------------------------------------------------------------------------
    0x??    CCONST    vres    imm   -       A = cidrConstantPool[B]
    0x??    CCMPEQ    vres    cidr  cidr    A = cidr(B) == cidr(C)
    0x??    CCMPNE    vres    cidr  cidr    A = cidr(B) != cidr(C)
    0x??    CCONTAINS vres    cidr  ip      A = cidr(B).contains(ip(C))

#### Array Ops

    Opcode  Mnemonic  A       B     C       Description
    --------------------------------------------------------------------------------------------
    0x??    ASNEW     vres    imm   -       vres = new Array<int>(imm)
    0x??    ASINIT    array   index value   array[index] = toString(value)
    0x??    ANNEW     vres    imm   -       vres = new Array<int>(imm)
    0x??    ANINIT    vres    index value   array[index] = toNumber(value)
    0x??    ANINITI   vres    index imm     array[index] = value

#### Control Ops

    Opcode  Mnemonic  A       B     C       Description
    --------------------------------------------------------------------------------------------
    0x??    JMP       pc      -     -       Unconditionally jump to $pc
    0x??    CONDBR    var     pc    -       Conditionally jump to $pc if int(A) evaluates to true
    0x??    EXIT      imm     -     -       End program with given boolean status code

#### Native Call Ops

    Opcode  Mnemonic  A       B     C       Description
    --------------------------------------------------------------------------------------------
    0x??    CALL      id      argc  argv    Invoke native func with identifier A with B arguments.
                                            Return value is stored in C+0.
                                            Arguments are stored in C+N with 0 < N < argc.
    0x??    HANDLER   id      argc  argv    Invoke native handler (can cause program to exit).
                                            Return code must be a boolean in B+0,
                                            and parameters are stored as for CALL.

##### Parameter Passing Conventions

Just like a C program, a consecutive array of `argv[]`'s is created, but where
`argv[0]` contains the result value (if any) and `argv[1]` to `argv[argc - 1]` will
contain the parameter values.

- Integers: register contents cast to a 64bit signed integer
- Boolean: as integer, false is 0, true is != 0
- String: register contents is cast to a VM internal `String` pointer.
- IPAddress: register contents is cast to a VM internal `IPAddress` pointer.
- Cidr: register contents is cast to a VM internal `Cidr` pointer.
- RegExp: register contents is cast to a VM internal `RegExp` pointer.
- Handler: register contents is an offset into the programs handler table.
- Array of V: conecutive registers as passed in `argv` directly represent the elements of the given array.
- Associative Array of (K, V): Keys are stored in `argv[N+0]` and values in `argv[N+1]`.

