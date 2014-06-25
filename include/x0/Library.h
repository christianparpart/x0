// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_library_hpp
#define sw_x0_library_hpp (1)

#include <x0/Api.h>
#include <system_error>
#include <string>

namespace x0 {

class X0_API Library
{
public:
    explicit Library(const std::string& filename = std::string());
    Library(Library&& movable);
    ~Library();

    Library& operator=(Library&& movable);

    std::error_code open(const std::string& filename);
    bool open(const std::string& filename, std::error_code& ec);
    bool isOpen() const;
    void *resolve(const std::string& symbol, std::error_code& ec);
    void close();

    void *operator[](const std::string& symbol);

private:
    std::string filename_;
    void *handle_;
};

} // namespace x0

#endif
