require 'minitest/autorun'
require 'tempfile'

# Load the compiled extension. By default it is looked up in ../ext (where
# `rake compile` builds it); UCL_EXT_DIR can point elsewhere.
libdir = ENV['UCL_EXT_DIR'] || File.expand_path('../ext', __dir__)
$LOAD_PATH.unshift(libdir) unless $LOAD_PATH.include?(libdir)
require 'ucl'

class TestUCL < Minitest::Test
  def setup
    # UCL.flags is global state; keep tests independent.
    UCL.flags = 0
  end

  # ---- scalar types -------------------------------------------------------

  def test_integer
    assert_equal({ 'n' => 42 }, UCL.parse('n = 42'))
  end

  def test_negative_integer
    assert_equal({ 'n' => -5 }, UCL.parse('n = -5'))
  end

  def test_large_integer
    assert_equal({ 'n' => 9_999_999_999_999 }, UCL.parse('n = 9999999999999'))
  end

  def test_max_int64
    assert_equal({ 'n' => 9_223_372_036_854_775_807 },
                 UCL.parse('n = 9223372036854775807'))
  end

  def test_hexadecimal_integer
    assert_equal({ 'h' => 31 }, UCL.parse('h = 0x1f'))
  end

  def test_float
    assert_equal({ 'f' => 1.5 }, UCL.parse('f = 1.5'))
  end

  def test_float_with_exponent
    assert_equal({ 'f' => 1500.0 }, UCL.parse('f = 1.5e3'))
  end

  def test_string
    assert_equal({ 's' => 'hello' }, UCL.parse('s = hello'))
  end

  def test_quoted_string
    assert_equal({ 's' => 'hello world' }, UCL.parse('s = "hello world"'))
  end

  def test_string_keeps_utf8_bytes
    # Strings are returned with ASCII-8BIT encoding, but the bytes are the
    # verbatim (UTF-8) content of the configuration.
    s = UCL.parse('s = "héllo"')['s']
    assert_equal 'héllo'.b, s.b
  end

  def test_booleans
    assert_equal({ 't' => true, 'f' => false }, UCL.parse("t = true\nf = false"))
  end

  def test_boolean_word_variants
    assert_equal({ 'a' => true, 'b' => true, 'c' => false, 'd' => false },
                 UCL.parse("a = yes\nb = on\nc = off\nd = no"))
  end

  def test_null
    assert_equal({ 'z' => nil }, UCL.parse('z = null'))
  end

  # ---- size multipliers ---------------------------------------------------

  def test_si_multiplier
    assert_equal({ 's' => 1000 }, UCL.parse('s = 1k'))
  end

  def test_binary_multipliers
    assert_equal({ 'a' => 1024, 'b' => 1_048_576 },
                 UCL.parse("a = 1kb\nb = 1mb"))
  end

  # ---- containers ---------------------------------------------------------

  def test_array
    assert_equal({ 'a' => [1, 2, 3] }, UCL.parse('a = [1, 2, 3]'))
  end

  def test_mixed_type_array
    assert_equal({ 'a' => [1, 'two', true, nil] },
                 UCL.parse('a = [1, "two", true, null]'))
  end

  def test_nested_arrays
    assert_equal({ 'a' => [[1, 2], [3, 4]] }, UCL.parse('a = [[1,2],[3,4]]'))
  end

  def test_array_of_objects
    assert_equal({ 'a' => [{ 'x' => 1 }, { 'y' => 2 }] },
                 UCL.parse('a = [ {x=1}, {y=2} ]'))
  end

  def test_nested_object
    assert_equal({ 'o' => { 'x' => 1 } }, UCL.parse('o { x = 1 }'))
  end

  def test_deeply_nested
    assert_equal({ 'a' => { 'b' => { 'c' => { 'd' => 1 } } } },
                 UCL.parse('a { b { c { d = 1 } } }'))
  end

  def test_dotted_key_is_kept_flat
    assert_equal({ 'a.b.c' => 1 }, UCL.parse('a.b.c = 1'))
  end

  # ---- JSON compatibility -------------------------------------------------

  def test_parses_json
    assert_equal({ 'a' => 1, 'b' => [2, 3] }, UCL.parse('{"a": 1, "b": [2,3]}'))
  end

  # ---- comments -----------------------------------------------------------

  def test_comments_are_ignored
    assert_equal({ 'x' => 1, 'y' => 2 },
                 UCL.parse("# header\nx = 1 # inline\ny = 2"))
  end

  # Characterisation, not endorsement: in libucl a trailing comment suppresses
  # suffix parsing, so a value carrying a unit or a multiplier comes back as a
  # string. Plain numbers are unaffected (see above). The README documents this
  # as a trap; if a future libucl fixes it, this test is the canary that says
  # the documentation needs updating.
  def test_trailing_comment_suppresses_suffix_parsing
    assert_equal({ 't' => 30.0 },       UCL.parse('t = 30s'))
    assert_equal({ 't' => '30s' },      UCL.parse('t = 30s # note'))
    assert_equal({ 'n' => 10_485_760 }, UCL.parse('n = 10mb'))
    assert_equal({ 'n' => '10mb' },     UCL.parse('n = 10mb # note'))
  end

  # ---- duplicate keys become an explicit array ----------------------------

  def test_duplicate_keys_make_array
    assert_equal({ 'k' => [1, 2, 3] }, UCL.parse("k = 1\nk = 2\nk = 3"))
  end

  def test_single_key_stays_scalar
    assert_equal({ 'k' => 1 }, UCL.parse('k = 1'))
  end

  # ---- empty objects (regression: used to raise) --------------------------

  def test_empty_input_returns_empty_hash
    assert_equal({}, UCL.parse(''))
  end

  def test_whitespace_only_returns_empty_hash
    assert_equal({}, UCL.parse("   \n  "))
  end

  def test_comment_only_returns_empty_hash
    assert_equal({}, UCL.parse("# nothing here\n"))
  end

  def test_empty_object
    assert_equal({ 'e' => {} }, UCL.parse('e {}'))
  end

  def test_nested_empty_object
    assert_equal({ 'a' => { 'b' => {} } }, UCL.parse('a { b {} }'))
  end

  def test_empty_object_then_more_keys
    assert_equal({ 'a' => {}, 'b' => 2 }, UCL.parse("a {}\nb = 2"))
  end

  def test_empty_array
    assert_equal({ 'a' => [] }, UCL.parse('a = []'))
  end

  # ---- time ---------------------------------------------------------------

  def test_time_is_parsed_to_float
    assert_equal({ 't' => 10.0 }, UCL.parse('t = 10s'))
  end

  def test_no_time_keeps_string
    assert_equal({ 't' => '10s' }, UCL.parse('t = 10s', UCL::NO_TIME))
  end

  # ---- key flags ----------------------------------------------------------

  def test_key_symbol
    assert_equal({ name: 'value' }, UCL.parse('name = value', UCL::KEY_SYMBOL))
  end

  def test_key_symbol_recurses_into_nested_objects
    assert_equal({ o: { x: 1 } }, UCL.parse('o { x = 1 }', UCL::KEY_SYMBOL))
  end

  def test_key_lowercase
    assert_equal({ 'foo' => 1 }, UCL.parse('FOO = 1', UCL::KEY_LOWERCASE))
  end

  def test_combined_flags
    assert_equal({ foo: 1 },
                 UCL.parse('FOO = 1', UCL::KEY_SYMBOL | UCL::KEY_LOWERCASE))
  end

  # ---- macros -------------------------------------------------------------

  def test_disable_macro_does_not_process_include
    # DISABLE_MACRO stops macros from being executed, so the referenced file
    # is never read. libucl versions differ in how they treat the macro line:
    # older ones ignore it (parsing only the remaining keys), newer ones reject
    # it as an invalid key. Both outcomes are acceptable here.
    config = %(.include "/no/such/file"\nx = 1)
    begin
      assert_equal({ 'x' => 1 }, UCL.parse(config, UCL::DISABLE_MACRO))
    rescue UCL::Error => e
      refute_empty e.message
    end
  end

  def test_include_of_missing_file_raises_without_flag
    assert_raises(UCL::Error) { UCL.parse(%(.include "/no/such"\nx = 1)) }
  end

  # ---- file variables -----------------------------------------------------

  def test_no_filevars_leaves_variable_unexpanded
    assert_equal({ 'd' => '$CURDIR' },
                 UCL.parse('d = $CURDIR', UCL::NO_FILEVARS))
  end

  def test_parse_expands_curdir_by_default
    refute_equal '$CURDIR', UCL.parse('d = $CURDIR')['d']
  end

  # ---- default flags ------------------------------------------------------

  def test_flags_default_to_zero
    assert_equal 0, UCL.flags
  end

  def test_flags_are_used_as_default
    UCL.flags = UCL::KEY_SYMBOL
    assert_equal({ name: 'value' }, UCL.parse('name = value'))
  end

  def test_explicit_flags_override_default
    UCL.flags = UCL::KEY_SYMBOL
    assert_equal({ 'name' => 'value' }, UCL.parse('name = value', 0))
  end

  # ---- result object ------------------------------------------------------

  def test_result_is_mutable
    result = UCL.parse('a = 1')
    result['b'] = 2
    assert_equal({ 'a' => 1, 'b' => 2 }, result)
  end

  # ---- load_file ----------------------------------------------------------

  def test_load_file
    with_conf("list = [1, 2, 3]\nname = test\n") do |path|
      assert_equal({ 'list' => [1, 2, 3], 'name' => 'test' },
                   UCL.load_file(path))
    end
  end

  def test_load_file_sets_filename_variable
    with_conf('f = "$FILENAME"' + "\n") do |path|
      assert_equal({ 'f' => File.realpath(path) }, UCL.load_file(path))
    end
  end

  def test_load_file_with_flags
    with_conf("Name = value\n") do |path|
      assert_equal({ name: 'value' },
                   UCL.load_file(path, UCL::KEY_SYMBOL | UCL::KEY_LOWERCASE))
    end
  end

  def test_load_missing_file_raises
    assert_raises(UCL::Error) { UCL.load_file('/no/such/file.conf') }
  end

  # ---- errors -------------------------------------------------------------

  def test_malformed_raises_ucl_error
    assert_raises(UCL::Error) { UCL.parse('a = [') }
  end

  def test_error_message_is_present
    err = assert_raises(UCL::Error) { UCL.parse('a = [') }
    refute_empty err.message
  end

  def test_error_is_a_standard_error
    assert_operator UCL::Error, :<, StandardError
  end

  def test_repeated_malformed_parses_do_not_crash
    100.times do
      assert_raises(UCL::Error) { UCL.parse('} bad {') }
    end
  end

  def test_parse_rejects_non_string
    assert_raises(TypeError) { UCL.parse(42) }
  end

  # ---- nesting limit ------------------------------------------------------

  # The conversion to Ruby objects and libucl's own tree handling both recurse
  # once per nesting level, so the depth is capped instead of being left to
  # exhaust the C stack. The threads below are what make these meaningful: a
  # thread gets a 1 MiB machine stack, which is the small stack the limit has
  # to protect.

  def test_deeply_nested_array_raises_ucl_error
    err = Thread.new { assert_raises(UCL::Error) { UCL.parse(deep_array) } }.value
    assert_match(/nesting/, err.message)
  end

  def test_deeply_nested_object_raises_ucl_error
    src = ('k {' * 20_000) + ('}' * 20_000)
    Thread.new { assert_raises(UCL::Error) { UCL.parse(src) } }.join
  end

  def test_deeply_nested_file_raises_ucl_error
    with_conf(deep_array) do |path|
      Thread.new { assert_raises(UCL::Error) { UCL.load_file(path) } }.join
    end
  end

  def test_nesting_just_below_the_limit_is_accepted
    src = 'a = ' + ('[' * 999) + (']' * 999)
    assert_kind_of Array, UCL.parse(src)['a']
  end

  def test_parser_still_works_after_a_rejected_document
    Thread.new { assert_raises(UCL::Error) { UCL.parse(deep_array) } }.join
    assert_equal({ 'x' => 1 }, UCL.parse('x = 1'))
  end

  def test_rejected_document_does_not_leak_the_parser
    # Regression: the conversion used to escape with the parser and the whole
    # parsed tree still allocated, losing ~2.7 MiB per rejected 40 KiB input.
    before = process_rss_kib
    skip 'cannot read RSS on this platform' if before.nil?
    100.times do
      UCL.parse(deep_array)
    rescue UCL::Error
      # expected; what is being measured is what the failure path leaves behind
    end
    GC.start
    growth = process_rss_kib - before
    # Pre-fix this grew by ~280 MiB; the margin is deliberately generous so
    # that ordinary heap growth cannot make the test flaky.
    assert_operator growth, :<, 20 * 1024,
                    "leaked #{growth} KiB over 100 rejected documents"
  end

  # ---- macro safety -------------------------------------------------------

  def test_parse_processes_the_include_macro
    with_conf("secret = value\n") do |path|
      assert_equal({ 'secret' => 'value' }, UCL.parse(%(.include "#{path}")))
    end
  end

  def test_safe_parse_does_not_read_the_included_file
    with_conf("secret = value\n") do |path|
      # libucl versions differ in how they treat a macro line when macros are
      # disabled (ignored, or rejected as an invalid key); either is fine, as
      # long as the file is not read.
      begin
        refute_includes UCL.safe_parse(%(.include "#{path}")), 'secret'
      rescue UCL::Error => e
        refute_empty e.message
      end
    end
  end

  def test_safe_parse_parses_ordinary_input
    assert_equal({ 'a' => 1, 'b' => [2, 3] }, UCL.safe_parse("a = 1\nb = [2,3]"))
  end

  def test_safe_parse_honours_explicit_flags
    assert_equal({ a: 1 },
                 UCL.safe_parse('A = 1', UCL::KEY_SYMBOL | UCL::KEY_LOWERCASE))
  end

  def test_safe_parse_uses_the_default_flags
    UCL.flags = UCL::KEY_SYMBOL
    assert_equal({ a: 1 }, UCL.safe_parse('a = 1'))
  end

  # ---- subclasses ---------------------------------------------------------

  def test_subclass_uses_its_own_default_flags
    klass = Class.new(UCL)
    klass.flags = UCL::KEY_SYMBOL
    assert_equal({ 'name' => 'value' }, UCL.parse('name = value'))
    assert_equal({ name: 'value' },     klass.parse('name = value'))
  end

  def test_subclass_without_flags_falls_back_to_ucl
    UCL.flags = UCL::KEY_SYMBOL
    klass = Class.new(UCL)
    assert_equal UCL::KEY_SYMBOL,   klass.flags
    assert_equal({ name: 'value' }, klass.parse('name = value'))
  end

  # ---- constants ----------------------------------------------------------

  def test_constants_defined
    %i[KEY_SYMBOL KEY_LOWERCASE NO_TIME DISABLE_MACRO NO_FILEVARS].each do |c|
      assert UCL.const_defined?(c), "UCL::#{c} should be defined"
    end
  end

  private

  # 20 000 levels: deeper than the 1000-level limit, and deep enough that the
  # C stack would be gone without it.
  def deep_array
    'a = ' + ('[' * 20_000) + (']' * 20_000)
  end

  # Resident set size in KiB, or nil where it cannot be read.
  def process_rss_kib
    out = `ps -o rss= -p #{$$} 2>/dev/null`
    $?.success? && !out.strip.empty? ? out.to_i : nil
  rescue StandardError
    nil
  end

  def with_conf(content)
    Tempfile.create(['ucl', '.conf']) do |f|
      f.write(content)
      f.flush
      yield f.path
    end
  end
end
