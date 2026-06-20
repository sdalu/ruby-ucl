Gem::Specification.new do |s|
    s.name        = 'ucl'
    s.version     = '0.1.4'
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

    s.extensions  = [ 'ext/extconf.rb' ]
    s.files       = %w[ ucl.gemspec README.md LICENSE ] +
                    Dir['ext/**/*.{c,h,rb}'] +
                    Dir['test/**/*.rb']

    # Used at build time to download and compile libucl from source when no
    # system-wide installation is found (requires cmake and a C compiler).
    s.add_dependency 'mini_portile2', '~> 2.8'

    s.add_development_dependency 'rake'
    s.add_development_dependency 'minitest', '~> 5.0'
end
