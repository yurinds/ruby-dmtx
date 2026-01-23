require 'mkmf'
dir_config('dmtx')
abort 'libdmtx not found' unless have_library('dmtx')
abort 'libpng not found' unless have_library('png') && have_header('png.h')
create_makefile('Rdmtx')
