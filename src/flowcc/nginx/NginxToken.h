// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
