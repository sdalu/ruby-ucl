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

To track this repository instead of a released gem, **`submodules: true` is
required**:

~~~ruby
gem 'ucl', github: 'sdalu/ruby-ucl', submodules: true
~~~

libucl is vendored as a git submodule (see below), and Bundler does not fetch
submodules unless asked. Without that option `ext/libucl` stays empty and the
build fails — except on a machine that happens to have a system-wide libucl,
where it quietly links against that instead, and only fails once it is
deployed somewhere that has none.

The extension binds to the native [libucl][1] library. At build time it is
resolved as follows:

1. If a system-wide libucl is found (via `pkg-config`, or under `/opt` or
   `/usr/local`), the extension links against it.
2. Otherwise the copy bundled with the gem is compiled directly into the
   extension. This needs nothing but a C compiler — no network access, no
   `cmake`, no extra gems.

You can force the bundled copy regardless of any system installation:

~~~sh
gem install ucl -- --enable-vendor-libucl
# or, with Bundler:
bundle config set build.ucl --enable-vendor-libucl
~~~

Building the bundled copy also compiles out `.include` over http/ftp, which
is gated on libucl's optional fetch support. A system-wide libucl may have it
enabled — see [Untrusted input](#untrusted-input).

> **Debian/Ubuntu note:** do **not** `apt install libucl-dev`. That package
> is an unrelated [UCL *data-compression* library][3] that merely shares the
> name — it does not provide `ucl_parser_new`. The configuration parser is
> not packaged for Debian, so just let the gem build its bundled copy — a C
> compiler is all you need. (The parser *is* packaged on Fedora, Homebrew and
> FreeBSD ports as `libucl`.)


Usage
-----

~~~ruby
require 'ucl'

# Parse a string of UCL data
UCL.parse(File.read('foo.conf'))

# Same, with macros disabled -- use this for input you do not control
UCL.safe_parse(untrusted_string)

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
`UCL.safe_parse` takes the same arguments as `parse` and always adds
`UCL::DISABLE_MACRO` (see *Untrusted input* below).

On a malformed configuration, or if conversion of the parsed tree fails,
a `UCL::Error` is raised.

Given a configuration like:

~~~nginx
# a sample configuration
name = example
# timeout is parsed as a number of seconds, max_size understands multipliers
timeout = 30s
max_size = 10mb

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
| `DISABLE_MACRO`  | Disable processing of macros (e.g. `.include`); see *Untrusted input*. |
| `NO_FILEVARS`    | Do not predefine `$FILENAME` / `$CURDIR` (affects `parse`; `load_file` still sets them from the file). |


Untrusted input
---------------

`UCL.parse` and `UCL.load_file` process macros, so a configuration can pull
in other files through `.include`:

~~~ruby
UCL.parse('.include "/etc/secrets.conf"')
#=> the contents of that file, as Ruby objects
~~~

That is a feature of the format — it is how a configuration is split across
several files — but it means **a configuration string from an untrusted
source can read any file the process can read**. Parse such input with
`UCL.safe_parse`, which is `UCL.parse` with `UCL::DISABLE_MACRO` always
added:

~~~ruby
UCL.safe_parse(params[:config])      # `.include` is not executed
~~~

`load_file` has no `safe_` counterpart: the path comes from the caller, so
the decision to trust the file has already been made. Pass
`UCL::DISABLE_MACRO` explicitly if a trusted path holds untrusted content.

If libucl was built with URL includes enabled (the bundled build disables
them; a system-wide libucl may not), `.include` can also fetch over the
network. One more reason to reach for `safe_parse`.


Limits
------

Objects nested more than **1000 levels** deep are rejected with a
`UCL::Error`. Converting the parsed tree recurses once per level, as does
libucl's own handling of it, so an unbounded depth would exhaust the C
stack — a 1 MiB thread stack (what a Puma or Sidekiq worker gets) is gone
at roughly 15000 levels. Real configurations nest a handful of levels;
Ruby's own JSON parser stops at 100.

> **Known limitation.** The check happens when the parsed tree is converted,
> which is too late for input that is *both* deeply nested *and* malformed:
> libucl then releases its half-built tree recursively from inside
> `ucl_parser_free()`, out of reach of this binding, and a document such as
> `'a = ' + '[' * 20_000` raises `SystemStackError` instead of `UCL::Error`
> and leaks the parser. Deeply nested *well-formed* input is handled
> correctly. If you parse untrusted configurations, cap their size.


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

* **A trailing `#` comment suppresses suffix parsing.** `t = 30s` is the
  float `30.0` and `n = 10mb` the integer `10485760`, but `t = 30s # note`
  and `n = 10mb # note` come back as the *strings* `"30s"` and `"10mb"`.
  Plain integers and floats are unaffected (`n = 1 # note` is `1`). This is
  libucl's behaviour, not the binding's; put the comment on its own line
  when the value carries a unit or a multiplier.

* Returned strings carry their verbatim bytes but are tagged with the
  `ASCII-8BIT` (binary) encoding; call `String#force_encoding('UTF-8')` if
  you need them as UTF-8.


Development
-----------

~~~sh
git submodule update --init   # populate ext/libucl (bundled libucl sources)
bundle install          # install development dependencies
rake compile            # build the C extension into ext/
rake test               # compile and run the test suite
rake clobber            # remove all generated files
~~~

`rake test` works with the development gems installed system-wide too; with
a `Gemfile.lock` present, prefix commands with `bundle exec`.

`ext/libucl` is a git submodule pinned to an upstream release tag; `rake
compile` and `rake build` both refuse to run until it is populated. To move
to a newer libucl, check out the new tag inside it and commit the pointer.


License
-------

Released under the MIT License. See [LICENSE](LICENSE).

When built against a system-wide libucl, nothing else is distributed. When
the bundled copy is compiled in, the gem also redistributes libucl and the
libraries vendored inside it (uthash, klib, mum, tree.h) — all permissive,
all notice-retention only. Their texts are in
[LICENSE-DEPENDENCIES.md](LICENSE-DEPENDENCIES.md).


[1]: https://github.com/vstakhov/libucl
[2]: https://nginx.org/
[3]: https://www.oberhumer.com/opensource/ucl/
