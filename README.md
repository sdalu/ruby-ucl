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


Requirements
------------

The native [libucl][1] library (version 0.8.2 or later) and its headers
must be installed. By default the extension also looks in `/opt/lib` and
`/opt/include`.

> **Debian/Ubuntu note:** do **not** `apt install libucl-dev`. That package
> is an unrelated [UCL *data-compression* library][3] that happens to share
> the name — it does not provide `ucl_parser_new` and the build will fail.
> The configuration parser required here is not packaged for Debian; build
> it from source instead (e.g. `./configure --prefix=/opt && make install`,
> matching the default search paths above). It is, however, available on
> Fedora (`libucl`), Homebrew (`libucl`) and FreeBSD ports.


Installation
------------

~~~sh
gem install ucl
~~~

Or add it to your `Gemfile`:

~~~ruby
gem 'ucl'
~~~


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

Note: the parser uses explicit (not implicit) arrays, so a key repeated
several times is collected into an `Array` of its values:

~~~ruby
UCL.parse("a = 1\na = 2")   #=> { "a" => [1, 2] }
UCL.parse("a = 1")          #=> { "a" => 1 }
~~~


License
-------

Released under the MIT License. See [LICENSE](LICENSE).


[1]: https://github.com/vstakhov/libucl
[2]: https://nginx.org/
[3]: https://www.oberhumer.com/opensource/ucl/
