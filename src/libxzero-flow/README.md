## Embeddable Configuration Flow Programming Language

```!sh
# Example Flow:
handler main {
  if req.path =^ '/private/' {
    return 403;
  }
  docroot '/var/www';
  staticfile;
}
```

### Features

* Dedicated language for programming control flow (without loops).
* Optimizing Compiler.
* Virtual Machine Instruction Set is optimized for our usecase.
* Virtual Machine supports direct threading and indirect threading.
* Powerful C++ API to extend Flow with functions and handlers.
* ...

### Build Me Now

You want to include `libxzero-flow` as a library in your own project.
Check out our examples.

### Copyright

```
Copyright (c) 2009-2014 Christian Parpart <trapni@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```
