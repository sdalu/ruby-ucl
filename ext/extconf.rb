require 'mkmf'

# Vendored libucl (vstakhov's Universal Configuration Language parser), used
# when no system-wide installation is found. It lives in ext/libucl, a git
# submodule pinned to a release tag. NOTE: this is *not* the Debian
# `libucl-dev` package, which is an unrelated compression library.
#
# Its sources are compiled straight into this extension rather than built as a
# separate library: src/ucl_internal.h defines the HAVE_* macros itself when
# HAVE_CONFIG_H is absent -- upstream calls this the "embedded build" and ships
# a Makefile.unix for it -- so no configure step is needed, and with it go the
# download, cmake, and any question of which libucl the linker picks up.
#
# One useful side effect: `.include` over http/ftp is gated on HAVE_FETCH_H and
# CURL_H, which the embedded build never defines, so remote includes are
# compiled out. A system-wide libucl may well have them enabled.
LIBUCL_DIR = File.join(__dir__, 'libucl').freeze

# Force using the bundled copy, ignoring any system installation:
#   gem install ucl -- --enable-vendor-libucl
#   bundle config set build.ucl --enable-vendor-libucl
#   UCL_VENDOR_LIBUCL=1 rake compile
force_vendor = enable_config('vendor-libucl', false) ||
               ENV.key?('UCL_VENDOR_LIBUCL')

# Locate a system-wide libucl: pkg-config first, then the usual prefixes.
def system_libucl
  return true if pkg_config('libucl')

  find_header( 'ucl.h',                 '/opt/include', '/usr/local/include') &&
    find_library('ucl', 'ucl_parser_new', '/opt/lib',     '/usr/local/lib')
end

# The submodule is not populated in a fresh clone, and a gem packaged without
# it would fail here rather than at `git submodule update` time.
def bundled_libucl
  File.exist?(File.join(LIBUCL_DIR, 'src', 'ucl_parser.c'))
end

if !force_vendor && system_libucl
  message "Using system libucl.\n"
else
  unless bundled_libucl
    abort "\nThe bundled libucl sources are missing from #{LIBUCL_DIR}.\n"     \
          "In a git checkout, populate the submodule with:\n"                  \
          "    git submodule update --init\n"                                  \
          "In an installed gem this means the gem was packaged without them; " \
          "please report it.\n"                                                \
          "Failing that, install libucl system-wide and build again.\n"
  end

  # Prepended so that a system-wide ucl.h cannot shadow the bundled one.
  $INCFLAGS = "-I#{LIBUCL_DIR}/include -I#{LIBUCL_DIR}/uthash " \
              "-I#{LIBUCL_DIR}/src -I#{LIBUCL_DIR}/klib #{$INCFLAGS}"

  # Every .c under src/ -- the same set CMakeLists.txt lists as UCLSRC.
  $srcs = [File.join(__dir__, 'ucl.c')] +
          Dir[File.join(LIBUCL_DIR, 'src', '*.c')].sort
  $objs = $srcs.map { |src| "#{File.basename(src, '.c')}.o" }
  $VPATH << "$(srcdir)/libucl/src"

  # libucl's own build passes both; without them its sources emit a few hundred
  # warning lines through an unrelated gem's install log.
  $CFLAGS << ' -Wno-pointer-sign -Wno-unused-parameter'

  have_library('m') # libucl relies on the math library

  message "Compiling the bundled libucl into the extension.\n"
end

create_makefile('ucl')
