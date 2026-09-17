# Activate the bundle only when one has actually been installed
# (Gemfile.lock present). Otherwise fall back to system-installed gems, so
# the tasks also work without running `bundle install` first. Calling
# bundler/setup without an installed bundle would mutate the environment
# (e.g. GEM_HOME) and break child processes such as the test runner.
require 'bundler/setup' if File.exist?('Gemfile.lock')

begin
    require 'bundler/gem_tasks'
rescue LoadError
    # bundler not available: build/release tasks are unavailable
end

require 'rake/testtask'
require 'rake/clean'

# Intermediate build products (removed by `rake clean`). The bundled libucl
# compiles into ext/ alongside ucl.o, so ext/*.o covers it.
CLEAN.include('ext/Makefile', 'ext/*.o', 'ext/mkmf.log',
              'ext/.*.time', '.yardoc')
# Final build products (removed, with the above, by `rake clobber`).
CLOBBER.include('ext/*.so', 'doc')

# ext/libucl is a git submodule and is empty in a fresh clone. Building the gem
# without it would succeed and produce a package with no libucl in it, which
# only surfaces when someone installs it -- so both gates check first.
LIBUCL_SENTINEL = 'ext/libucl/src/ucl_parser.c'.freeze

desc "Check that the bundled libucl submodule is populated"
task :vendor_check do
    next if File.exist?(LIBUCL_SENTINEL)

    abort "ext/libucl is empty (#{LIBUCL_SENTINEL} is missing).\n" \
          "Populate it with:\n"                                   \
          "    git submodule update --init\n"
end

desc "Compile the C extension into ext/"
task :compile => :vendor_check do
    Dir.chdir('ext') do
        ruby 'extconf.rb'
        sh 'make'
    end
end

# Defined by bundler/gem_tasks above; a no-op placeholder otherwise.
task :build => :vendor_check

Rake::TestTask.new(:test) do |t|
    t.libs       << 'test' << 'ext'
    t.test_files  = FileList['test/test_*.rb']
    t.warning     = true
end
task :test => :compile

task :default => :test

# Documentation task; only available when yard is installed.
begin
    require 'yard'
    YARD::Rake::YardocTask.new do |t|
        t.files         = [ 'lib/**/*.rb', 'ext/ucl.c' ]
        t.options       = [ '-m', 'markdown' ]
        t.stats_options = [ '--list-undoc' ]
    end
rescue LoadError
    # yard not installed: `rake yard` is unavailable
end
