ruby-ucl
========

Ruby bindings to the [libucl][1] library for parsing configuration files
written in the **U**niversal **C**onfiguration **L**anguage (UCL).

UCL is a configuration format inspired by [nginx][2] and JSON. It is a
superset of JSON, so any valid JSON document is also valid UCL, while
adding a more relaxed, human-friendly syntax (unquoted keys, comments,
optional commas, multipliers, macros, …).

Parsed configurations are returned as plain Ruby objects (`Hash`,
`Array`, `String`, `Integer`, `Float`, `true`/`false`, `nil`), so no
special object model has to be learned.


Installation
------------

~~~sh
gem install ucl
~~~

Or add it to your `Gemfile`:

~~~ruby
gem 'ucl'
~~~

The extension binds to the native [libucl][1] library (version 0.8.2 or
later). At build time it is located as follows:

1. If a system-wide libucl is found (via `pkg-config`, or under `/opt` or
   `/usr/local`), it is used.
2. Otherwise libucl is **downloaded and compiled from source** automatically
   (using [mini_portile2][4] and CMake). This requires `cmake` and a C
   compiler to be available.

You can force the source build regardless of any system installation:

~~~sh
gem install ucl -- --enable-vendor-libucl
# or, with Bundler:
bundle config set build.ucl --enable-vendor-libucl
~~~

> **Debian/Ubuntu note:** do **not** `apt install libucl-dev`. That package
> is an unrelated [UCL *data-compression* library][3] that merely shares the
> name — it does not provide `ucl_parser_new`. Since the configuration
> parser is not packaged for Debian, just install `cmake` and a compiler and
> let the gem build libucl from source. (The parser *is* packaged on Fedora,
> Homebrew and FreeBSD ports as `libucl`.)


Usage
-----

~~~ruby
require 'ucl'

# Parse a string of UCL data
UCL.parse(File.read('foo.conf'))

# Parse a file directly (enables file-relative variables such as $FILENAME)
UCL.load_file('foo.conf')

# Pass flags explicitly (combine them with a bitwise OR)
UCL.load_file('foo.conf', UCL::KEY_SYMBOL | UCL::KEY_LOWERCASE)

# Or set the default flags applied to every subsequent call
UCL.flags = UCL::KEY_SYMBOL
UCL.parse('name = value')          #=> { :name => "value" }
~~~

Both `parse` and `load_file` accept an optional `flags` argument. When it
is omitted, the value of `UCL.flags` (default: no flag) is used.

On a malformed configuration, or if conversion of the parsed tree fails,
a `UCL::Error` is raised.

Given a configuration like:

~~~nginx
# a sample configuration
name = example
timeout = 30s            # parsed as a number of seconds
max_size = 10mb          # size multipliers are understood

servers = [
    { host = a.example, port = 8080 },
    { host = b.example, port = 8081 },
]

logging {
    level = info
    enabled = yes
}
~~~

`UCL.load_file` returns:

~~~ruby
{
  "name"     => "example",
  "timeout"  => 30.0,
  "max_size" => 10485760,
  "servers"  => [ { "host" => "a.example", "port" => 8080 },
                  { "host" => "b.example", "port" => 8081 } ],
  "logging"  => { "level" => "info", "enabled" => true },
}
~~~


Flags
-----

| Flag             | Effect                                                        |
|------------------|---------------------------------------------------------------|
| `KEY_SYMBOL`     | Return object keys as `Symbol` instead of `String`.           |
| `KEY_LOWERCASE`  | Convert all keys to lower case.                               |
| `NO_TIME`        | Do not parse time values; keep them as strings.              |
| `DISABLE_MACRO`  | Disable processing of macros (e.g. `.include`).              |
| `NO_FILEVARS`    | Do not predefine `$FILENAME` / `$CURDIR` (affects `parse`; `load_file` still sets them from the file). |


Type mapping
------------

| UCL type   | Ruby type                |
|------------|--------------------------|
| object     | `Hash`                   |
| array      | `Array`                  |
| string     | `String`                 |
| integer    | `Integer`                |
| float      | `Float`                  |
| time       | `Float` (seconds)        |
| boolean    | `true` / `false`         |
| null       | `nil`                    |

Notes:

* The parser uses explicit (not implicit) arrays, so a key repeated several
  times is collected into an `Array` of its values:

  ~~~ruby
  UCL.parse("a = 1\na = 2")   #=> { "a" => [1, 2] }
  UCL.parse("a = 1")          #=> { "a" => 1 }
  ~~~

* Integers understand size multipliers (`1k` → 1000, `1kb` → 1024,
  `1mb` → 1048576), hexadecimal (`0x1f` → 31) and JSON is accepted as-is.

* Returned strings carry their verbatim bytes but are tagged with the
  `ASCII-8BIT` (binary) encoding; call `String#force_encoding('UTF-8')` if
  you need them as UTF-8.


Development
-----------

~~~sh
bundle install          # install development dependencies
rake compile            # build the C extension into ext/
rake test               # compile and run the test suite
rake clobber            # remove all generated files
~~~

`rake test` works with the development gems installed system-wide too; with
a `Gemfile.lock` present, prefix commands with `bundle exec`.


License
-------

Released under the MIT License. See [LICENSE](LICENSE).


[1]: https://github.com/vstakhov/libucl
[2]: https://nginx.org/
[3]: https://www.oberhumer.com/opensource/ucl/
[4]: https://github.com/flavorjones/mini_portile
