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
| `NO_FILEVARS`    | Do not define the file-related variables when loading a file. |


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

Note: implicit arrays are disabled, so a key that appears several times
keeps only its last value rather than being collected into an array.


License
-------

Released under the MIT License. See [LICENSE](LICENSE).


[1]: https://github.com/vstakhov/libucl
[2]: https://nginx.org/
