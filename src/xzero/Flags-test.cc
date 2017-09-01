// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Flags.h>
#include <xzero/net/IPAddress.h>
#include <xzero/testing.h>

using namespace xzero;

/**
 * cmdline test cases
 *
 * - explicit boolean (long option)
 * - explicit boolean (short option)
 * - explicit short option, single
 * - explicit short option, first in chain
 * - explicit short option, deep in chain
 * - defaulting long-option
 * - required long-option with value
 * - required short-option with value
 * - errors
 *   - raise on unknown long option
 *   - raise on unknown short option
 *   - raise on missing long-option value
 *   - raise on missing short-option value
 *   - raise on missing required option
 *   - raise on invalid type (int)
 *   - raise on invalid type (float)
 *   - raise on invalid type (ip)
 */

TEST(Flags, defaults) {
  Flags flags;
  flags.defineString("some", 's', "<text>", "Something", "some value", nullptr);
  flags.defineBool("bool", 'b', "some boolean");

  std::error_code ec = flags.parse({});

  ASSERT_EQ(2, flags.size());
  ASSERT_EQ("some value", flags.getString("some"));
  ASSERT_EQ(false, flags.getBool("bool"));
}

TEST(Flags, getNumberDefault) {
  Flags flags;
  flags.defineNumber("some", 's', "<num>", "description", 42);
  std::error_code ec = flags.parse({});
  ASSERT_EQ(42, flags.getNumber("some"));
}

TEST(Flags, emptyStringDefault) {
  Flags flags;
  flags.defineString("some", 's', "<text>", "description", "");
  std::error_code ec = flags.parse({});
  ASSERT_EQ("", flags.getString("some"));
}

TEST(Flags, fail_on_unknown_long_option) {
  Flags flags;
  flags.defineBool("some", 's', "Something");
  std::error_code ec = flags.parse({"--something-else"});
  ASSERT_EQ(Flags::Error::UnknownOption, ec);
}

TEST(Flags, fail_on_unknown_short_option) {
  Flags flags;
  flags. defineBool("some", 's', "Something");
  std::error_code ec = flags.parse({"-t"});
  ASSERT_EQ(Flags::Error::UnknownOption, ec);
}

TEST(Flags, fail_on_missing_long_option) {
  Flags flags;
  flags.defineString("some", 's', "<text>", "Something");
  std::error_code ec = flags.parse({"--some"});
  ASSERT_EQ(Flags::Error::MissingOption, ec);
}

TEST(Flags, fail_on_missing_option_value) {
  Flags flags;
  flags.defineString("some", 's', "<some>", "Something");
  flags.defineString("tea", 't', "<some>", "Tea Time");

  std::error_code ec = flags.parse({"-s", "-tblack"});
  ASSERT_EQ(Flags::Error::MissingOptionValue, ec);

  ec = flags.parse({"-swhite", "-t"});
  ASSERT_EQ(Flags::Error::MissingOptionValue, ec);
}

TEST(Flags, short_option_values) {
  Flags flags;
  flags.defineString("some", 's', "<text>", "Something");
  flags.defineString("tea", 't', "<text>", "Tea Time");

  std::error_code ec = flags.parse({"-sthing", "-t", "time"});

  ASSERT_EQ(2, flags.size());
  ASSERT_EQ("thing", flags.getString("some"));
  ASSERT_EQ("time", flags.getString("tea"));
}

TEST(Flags, short_option_single) {
  Flags flags;
  flags.defineBool("some", 's', "Something");

  std::error_code ec = flags.parse({"-s"});

  ASSERT_EQ(1, flags.size());
  ASSERT_TRUE(flags.getBool("some"));
}

TEST(Flags, short_option_multi) {
  Flags flags;
  flags.defineBool("some", 's', "The Some");
  flags.defineBool("thing", 't', "The Thing");
  flags.defineBool("else", 'e', "The Else");

  std::error_code ec = flags.parse({"-tes"});

  ASSERT_EQ(3, flags.size());
  ASSERT_TRUE(flags.getBool("some"));
  ASSERT_TRUE(flags.getBool("thing"));
  ASSERT_TRUE(flags.getBool("else"));
}

TEST(Flags, short_option_multi_mixed) {
  Flags flags;
  flags.defineBool("some", 's', "The Some");
  flags.defineString("text", 't', "<text>", "The Text");

  std::error_code ec = flags.parse({"-sthello"});

  ASSERT_EQ(2, flags.size());
  ASSERT_TRUE(flags.getBool("some"));
  ASSERT_EQ("hello", flags.getString("text"));
}

TEST(Flags, short_option_value_inline) {
  Flags flags;
  flags.defineString("text", 't', "<text>", "The Text");

  std::error_code ec = flags.parse({"-thello"});

  ASSERT_EQ(1, flags.size());
  ASSERT_EQ("hello", flags.getString("text"));
}

TEST(Flags, short_option_value_sep) {
  Flags flags;
  flags.defineString("text", 't', "<text>", "The Text");

  std::error_code ec = flags.parse({"-t", "hello"});

  ASSERT_EQ(1, flags.size());
  ASSERT_EQ("hello", flags.getString("text"));
}

TEST(Flags, long_option_with_value_inline) {
  Flags flags;
  flags.defineString("text", 't', "<text>", "The Text");

  std::error_code ec = flags.parse({"--text=hello"});

  ASSERT_EQ(1, flags.size());
  ASSERT_EQ("hello", flags.getString("text"));
}

TEST(Flags, long_option_with_value_sep) {
  Flags flags;
  flags.defineString("text", 't', "<text>", "The Text");

  std::error_code ec = flags.parse({"--text", "hello"});

  ASSERT_EQ(1, flags.size());
  ASSERT_EQ("hello", flags.getString("text"));
}

TEST(Flags, type_int) {
  Flags flags;
  flags.defineNumber("number", 'n', "<number>", "The Number");

  std::error_code ec = flags.parse({"-n42"});
  ASSERT_EQ(1, flags.size());
  ASSERT_EQ(42, flags.getNumber("number"));
}

TEST(Flags, type_float) {
  Flags flags;
  flags.defineFloat("float", 'f', "<float>", "The Float");

  std::error_code ec = flags.parse({"-f1.42"});
  ASSERT_EQ(1, flags.size());
  ASSERT_EQ(1.42f, flags.getFloat("float"));
}

TEST(Flags, type_ip) {
  Flags flags;
  flags.defineIPAddress("ip", 'a', "<IP>", "The IP");

  std::error_code ec = flags.parse({"--ip=4.2.2.1"});
  ASSERT_EQ(1, flags.size());
  ASSERT_EQ(IPAddress("4.2.2.1"), flags.getIPAddress("ip"));
}

TEST(Flags, callbacks_on_explicit) {
  IPAddress bindIP;

  Flags flags;
  flags.defineIPAddress("bind", 'a', "<IP>", "IP address to bind listener address to.", None(),
      [&](const IPAddress& ip) {
        bindIP = ip;
      });

  std::error_code ec = flags.parse({"--bind", "127.0.0.2"});

  ASSERT_EQ(IPAddress("127.0.0.2"), bindIP);
}

TEST(Flags, callbacks_on_defaults__passed) {
  IPAddress bindIP;

  Flags flags;
  flags.defineIPAddress(
      "bind", 'a', "<IP>", "IP address to bind listener address to.",
      IPAddress("127.0.0.1"),
      [&](const IPAddress& ip) { bindIP = ip; });

  std::error_code ec = flags.parse({"--bind", "127.0.0.3"});
  ASSERT_EQ(IPAddress("127.0.0.3"), bindIP);
}

TEST(Flags, callbacks_on_defaults__default) {
  IPAddress bindIP;

  Flags flags;
  flags.defineIPAddress(
      "bind", 'a', "<IP>", "IP address to bind listener address to.",
      IPAddress("127.0.0.1"),
      [&](const IPAddress& ip) { bindIP = ip; });

  std::error_code ec = flags.parse({});
  ASSERT_EQ(IPAddress("127.0.0.1"), bindIP);
}

TEST(Flags, callbacks_on_repeated_args) {
  std::vector<IPAddress> hosts;
  Flags flags;
  flags.defineIPAddress(
      "host", 't', "<IP>", "Host address to talk to.", None(),
      [&](const IPAddress& host) { hosts.emplace_back(host); });

  std::error_code ec = flags.parse({"--host=127.0.0.1", "--host=192.168.0.1", "-t10.10.20.40"});

  ASSERT_EQ(3, hosts.size());
  ASSERT_EQ(IPAddress("127.0.0.1"), hosts[0]);
  ASSERT_EQ(IPAddress("192.168.0.1"), hosts[1]);
  ASSERT_EQ(IPAddress("10.10.20.40"), hosts[2]);
}

TEST(Flags, argc_argv_to_vector) {
  Flags flags;
  flags.defineBool("help", 'h', "Shows this help and terminates.");
  flags.defineBool("bool", 'b', "some boolean");

  const int argc = 3;
  static const char* argv[] = {
    "/proc/self/exe",
    "--help",
    "-b",
    nullptr
  };

  std::error_code ec = flags.parse(argc, argv);

  ASSERT_EQ(2, flags.size());
  ASSERT_TRUE(flags.getBool("help"));
  ASSERT_TRUE(flags.getBool("bool"));
}
