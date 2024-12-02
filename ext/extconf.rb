require 'mkmf'


find_library('ucl', 'ucl_parser_new', '/opt/lib')
find_header('ucl.h', '/opt/include')

create_makefile("ucl")
