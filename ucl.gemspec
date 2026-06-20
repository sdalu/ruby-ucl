Gem::Specification.new do |s|
    s.name        = 'ucl'
    s.version     = '0.1.3.2'
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
    s.files       = %w[ ucl.gemspec ]  + Dir['ext/**/*.{c,h,rb}']
end
