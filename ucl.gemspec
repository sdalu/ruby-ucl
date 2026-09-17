Gem::Specification.new do |s|
    s.name        = 'ucl'
    s.version     = '0.2.0'
    s.summary     = 'Universal Configuration Language (UCL) parser'
    s.description =  <<~EOF
      Parse configuration files written in the Universal Configuration
      Language (UCL), a human-friendly JSON superset. Native bindings to
      the libucl library; results are returned as plain Ruby objects.
    EOF

    s.homepage    = 'https://github.com/sdalu/ruby-ucl'
    s.license     = 'MIT'

    s.authors     = [ "Stéphane D'Alu"  ]
    s.email       = [ 'sdalu@sdalu.com' ]

    # The floor that has actually been exercised: 3.1 (Debian bookworm) and
    # 3.4. Nothing here is known to need a newer Ruby.
    s.required_ruby_version = '>= 3.1'

    s.extensions  = [ 'ext/extconf.rb' ]

    # ext/libucl is a git submodule holding the whole upstream tree, most of
    # which (tests, utils, the Lua and Python bindings, docs) has no business
    # in the gem -- hence an explicit list rather than an ext/**/* glob. It
    # names what the embedded build compiles, plus the licence texts that
    # redistributing those sources requires; see LICENSE-DEPENDENCIES.md.
    # `rake build` refuses to run when the submodule is not populated, since
    # these globs would silently come back empty.
    s.files       = %w[ ucl.gemspec README.md LICENSE LICENSE-DEPENDENCIES.md ] +
                    Dir['ext/*.{c,h,rb}']                                       +
                    Dir['test/**/*.rb']                                         +
                    %w[ ext/libucl/COPYING ]                                    +
                    Dir['ext/libucl/include/ucl.h']                             +
                    Dir['ext/libucl/src/*.{c,h}']                               +
                    Dir['ext/libucl/uthash/*.h']                                +
                    Dir['ext/libucl/klib/*.h']

    s.add_development_dependency 'rake'
    s.add_development_dependency 'minitest', '~> 5.0'
    s.add_development_dependency 'yard'
end
