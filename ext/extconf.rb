require 'mkmf'

# Pinned libucl (vstakhov's Universal Configuration Language parser) used
# when the library has to be built from source. NOTE: this is *not* the
# Debian `libucl-dev` package, which is an unrelated compression library.
LIBUCL_VERSION = '0.8.2'
LIBUCL_SHA256  = 'd95a0e2151cc167a0f3e51864fea4e8977a0f4c473faa805269a347f7fb4e165'

# Force building libucl from source, ignoring any system installation:
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

if !force_vendor && system_libucl
  message "Using system libucl.\n"
else
  message "Building libucl #{LIBUCL_VERSION} from source.\n"

  # Building from source needs cmake and a working C compiler.
  missing = []
  missing << 'cmake'          unless find_executable('cmake')
  missing << 'a C compiler'   unless try_compile('int main(void) { return 0; }')
  unless missing.empty?
    abort "\nBuilding libucl from source requires #{missing.join(' and ')}, " \
          "which #{missing.length > 1 ? 'are' : 'is'} not available.\n"      \
          "Install the missing tool(s), or provide a system-wide libucl.\n"
  end

  require 'mini_portile2'

  recipe = MiniPortileCMake.new('libucl', LIBUCL_VERSION)
  recipe.files = [{
    url:    "https://github.com/vstakhov/libucl/archive/refs/tags/#{LIBUCL_VERSION}.tar.gz",
    sha256: LIBUCL_SHA256,
  }]
  # Static, position-independent build with every optional feature disabled.
  recipe.configure_options += %w[
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DENABLE_URL_INCLUDE=OFF
    -DENABLE_LUA=OFF
    -DENABLE_UTILS=OFF
  ]
  recipe.cook     # download, verify checksum, cmake build + install into recipe.path
  recipe.activate

  $INCFLAGS << " -I#{recipe.path}/include"
  $LIBPATH.unshift "#{recipe.path}/lib"
  have_library('m') # libucl relies on the math library

  unless find_header( 'ucl.h',                 "#{recipe.path}/include") &&
         find_library('ucl', 'ucl_parser_new', "#{recipe.path}/lib")
    abort "\nlibucl was built but could not be linked " \
          "(check the static archive name/dir under #{recipe.path}/lib).\n"
  end
end

create_makefile('ucl')
