require 'bundler'
require 'yard'
require 'rake/testtask'

Bundler::GemHelper.install_tasks

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

YARD::Rake::YardocTask.new do |t|
    t.files         = [ 'lib/**/*.rb', 'ext/ucl.c' ]
    t.options       = [ '-m', 'markdown' ]
    t.stats_options = [ '--list-undoc' ]
end
