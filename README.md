This library allows parsing of configuration file in UCL format,
using the [libucl][1] library.


Examples
========

~~~ruby
UCL.load_file('foo.conf', UCL::KEY_SYMBOL | KEY_LOWERCASE)
UCL.parse(File.read('foo.conf'))
~~~

Supported flags:
* KEY_SYMBOL
* KEY_LOWERCASE
* NO_TIME
* DISABLE_MACRO
* NO_FILEVARS


[1]: https://github.com/vstakhov/libucl

