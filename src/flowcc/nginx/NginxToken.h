namespace nginx {

enum class Token {
  Unknown,
  On,
  Off,
  Begin,      // {
  End,        // }
  RndOpen,
  RndClose,
  If,
  Not,        // !
  String,
  Number,
  Ident,
  Equ,        // =
  Semicolon,  // ;
  Include,
  Comment,
  Eof,
  COUNT,
};

} // namespace nginx
