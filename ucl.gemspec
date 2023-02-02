Gem::Specification.new do |s|
    s.name        = 'ucl'
    s.version     = '0.1.3.1'
    s.summary     = " Universal configuration library parser"
    s.description =  <<~EOF
      
      Read configuration file in UCL format (binding to the libucl).

      EOF

    s.homepage    = 'https://github.com/sdalu/ruby-ucl'
    s.license     = 'MIT'

    s.authors     = [ "Stéphane D'Alu"  ]
    s.email       = [ 'sdalu@sdalu.com' ]

    s.extensions  = [ 'ext/extconf.rb' ]
    s.files       = %w[ ucl.gemspec ]  + Dir['ext/**/*.{c,h,rb}']
end
