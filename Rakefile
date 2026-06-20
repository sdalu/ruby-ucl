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

# Intermediate build products (removed by `rake clean`).
CLEAN.include('ext/Makefile', 'ext/*.o', 'ext/mkmf.log',
              'ext/.*.time', 'ext/ports', 'ext/tmp', '.yardoc')
# Final build products (removed, with the above, by `rake clobber`).
CLOBBER.include('ext/*.so', 'doc')

desc "Compile the C extension into ext/"
task :compile do
    Dir.chdir('ext') do
        ruby 'extconf.rb'
        sh 'make'
    end
end

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
