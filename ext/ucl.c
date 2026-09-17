#include <ruby.h>
#include <ruby/io.h>
#include <ucl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Fake flag */
#define UCL_PARSER_KEY_SYMBOL (1 << 12)

/* Deepest object nesting accepted when converting a parsed tree to Ruby
 * objects. Both the conversion below and libucl's own tree destructor walk
 * children recursively, with nothing but the C stack bounding them: a 1 MiB
 * thread stack is gone at roughly 15000 levels. Real configurations nest a
 * handful of levels (Ruby's own JSON parser stops at 100), so anything
 * deeper is refused with UCL::Error instead of being left to overflow the
 * stack. */
#define UCL_MAX_NESTING 1000


/**
 * Document-class: UCL
 *
 * Parser for configuration files written in the Universal Configuration
 * Language (UCL), a JSON-superset format handled by the libucl library.
 *
 * Parsed configurations are returned as plain Ruby objects (Hash, Array,
 * String, Integer, Float, true/false, nil).
 *
 * Objects nested more than 1000 levels deep are rejected with {UCL::Error};
 * see {UCL.parse}. Input that is both that deeply nested *and* malformed is
 * the one case this cannot cover: libucl releases its half-built tree
 * recursively, out of reach here, and a SystemStackError comes out instead.
 *
 * @example Parse a string
 *   UCL.parse('name = value')        #=> { "name" => "value" }
 *
 * @example Load a file with symbol keys
 *   UCL.load_file('foo.conf', UCL::KEY_SYMBOL)
 *
 * @see https://github.com/vstakhov/libucl
 */

/**
 * Document-class: UCL::Error
 *
 * Raised when a configuration cannot be parsed, or when the parsed tree
 * cannot be converted to Ruby objects.
 */

/**
 * Document-const: KEY_LOWERCASE
 * Flag: convert all object keys to lower case.
 */

/**
 * Document-const: NO_TIME
 * Flag: do not parse time values; keep them as strings.
 */

/**
 * Document-const: DISABLE_MACRO
 * Flag: disable processing of macros (e.g. <code>.include</code>).
 */

/**
 * Document-const: NO_FILEVARS
 * Flag: do not predefine the file variables (<code>$FILENAME</code>,
 * <code>$CURDIR</code>). This affects {UCL.parse}; {UCL.load_file} still
 * derives those variables from the file being loaded.
 */

/**
 * Document-const: KEY_SYMBOL
 * Flag: return object keys as Symbol instead of String.
 */



static VALUE mUCL                        = Qundef;
static VALUE eUCLError                   = Qundef;

static int ucl_allowed_c_flags = UCL_PARSER_KEY_LOWERCASE |
	                         UCL_PARSER_NO_TIME       |
	                         UCL_PARSER_DISABLE_MACRO |
                                 UCL_PARSER_NO_FILEVARS   ;


/* State threaded through a conversion.
 *
 * `iters` points at an array of UCL_MAX_NESTING slots living in the frame
 * that started the conversion: each level parks its iterator there, so an
 * exception unwinding the C stack does not strand them. `max_depth` is the
 * deepest slot ever written; every shallower slot has been written too (a
 * container at depth d sits inside a container at every depth above it),
 * which is what lets the error path sweep exactly iters[0..max_depth]
 * without having to clear the array up front. */
struct ucl_conv {
    int                flags;
    bool               failed;
    int                max_depth;
    ucl_object_iter_t *iters;
};


static VALUE
_iterate_valid_ucl(struct ucl_conv *cv, ucl_object_t const *root, int depth)
{
    ucl_object_iter_t   it  = NULL;   /* only allocated for objects/arrays */
    const ucl_object_t *obj = NULL;

    VALUE val = Qnil;

    /* Bound the recursion (see UCL_MAX_NESTING). Checked before this level
     * allocates its iterator, so the raise cannot strand one. */
    if (depth >= UCL_MAX_NESTING)
	rb_raise(eUCLError, "nesting deeper than %d levels", UCL_MAX_NESTING);

    switch (root->type) {
    case UCL_INT:
	val = rb_ll2inum((long long)ucl_object_toint(root));
	break;

    case UCL_FLOAT:
	val = rb_float_new(ucl_object_todouble(root));
	break;

    case UCL_STRING: {
	size_t len;
	const char *str = ucl_object_tolstring(root, &len);
	val = rb_str_new(str, len);
	break;
    }

    case UCL_BOOLEAN:
	val = ucl_object_toboolean(root) ? Qtrue : Qfalse;
	break;

    case UCL_TIME:
	val = rb_float_new(ucl_object_todouble(root));
	break;

    case UCL_OBJECT: {
	bool iterated = false;
	val = rb_hash_new();
	it  = ucl_object_iterate_new(root);
	if (it == NULL)
	    rb_raise(eUCLError, "failed to allocate UCL iterator");
	cv->iters[depth] = it;
	if (depth > cv->max_depth) cv->max_depth = depth;
	while ((obj = ucl_object_iterate_safe(it, !true))) {
	    iterated = true;
	    size_t keylen;
	    const char *key = ucl_object_keyl(obj, &keylen);
	    VALUE v_key = rb_str_new(key, keylen);
	    if (cv->flags & UCL_PARSER_KEY_SYMBOL)
		v_key = rb_to_symbol(v_key);
	    rb_hash_aset(val, v_key, _iterate_valid_ucl(cv, obj, depth + 1));
	}
	/* An empty object has a NULL hash that the safe iterator reports as an
	 * exception (EINVAL); ignore that and only flag a genuine error that
	 * occurs while iterating. Accumulate so a nested failure deeper in the
	 * tree is never cleared by a successful parent iteration. */
	if (iterated && ucl_object_iter_chk_excpn(it)) cv->failed = true;
	break;
    }

    case UCL_ARRAY: {
	bool iterated = false;
	val = rb_ary_new();
	it  = ucl_object_iterate_new(root);
	if (it == NULL)
	    rb_raise(eUCLError, "failed to allocate UCL iterator");
	cv->iters[depth] = it;
	if (depth > cv->max_depth) cv->max_depth = depth;
	while ((obj = ucl_object_iterate_safe(it, !true))) {
	    iterated = true;
	    rb_ary_push(val, _iterate_valid_ucl(cv, obj, depth + 1));
	}
	if (iterated && ucl_object_iter_chk_excpn(it)) cv->failed = true;
	break;
    }

    case UCL_USERDATA:
	val = rb_str_new(root->value.sv, root->len);
	break;

    case UCL_NULL:
	val = Qnil;
	break;

    default:
	rb_raise(eUCLError, "unhandled UCL type (%d)", root->type);

    }

    if (it != NULL) {
	ucl_object_iterate_free(it);
	cv->iters[depth] = NULL;
    }
    return val;
}


/* Remove one child from `top` and hand it back with its reference
 * transferred to the caller; NULL once `top` holds no child any more. */
static ucl_object_t *
_ucl_detach_child(ucl_object_t *top)
{
    if (top->type == UCL_ARRAY)
	return ucl_array_pop_last(top);

    if (top->type == UCL_OBJECT) {
	ucl_object_iter_t   it     = ucl_object_iterate_new(top);
	const ucl_object_t *first  = (it == NULL) ? NULL
	                           : ucl_object_iterate_safe(it, !true);
	const char         *key    = NULL;
	size_t              keylen = 0;

	if (first != NULL) key = ucl_object_keyl(first, &keylen);
	if (it    != NULL) ucl_object_iterate_free(it);
	if (key   == NULL) return NULL;
	/* `key` points into `first`, which `top` still holds a reference to
	 * until the pop below hands it over. The iterator is released first
	 * because the pop mutates the object it was iterating. */
	return ucl_object_pop_keyl(top, key, keylen);
    }

    return NULL;
}


/* Append `obj` to the worklist, growing it as needed; false only when the
 * allocation fails. */
static bool
_ucl_worklist_push(ucl_object_t ***work, size_t *len, size_t *cap,
		   ucl_object_t *obj)
{
    if (*len == *cap) {
	size_t         ncap = (*cap == 0) ? 64 : *cap * 2;
	ucl_object_t **narr = realloc(*work, ncap * sizeof(*narr));
	if (narr == NULL) return false;
	*work = narr;
	*cap  = ncap;
    }
    (*work)[(*len)++] = obj;
    return true;
}


/* Destroy everything below `root`, leaving `root` itself alive but empty,
 * without recursing.
 *
 * libucl's destructor walks children recursively, so releasing a tree
 * deeper than the C stack allows overflows it -- and that is exactly the
 * tree UCL_MAX_NESTING refuses to convert, which would otherwise turn a
 * clean UCL::Error into a stack overflow inside libucl. Children are
 * detached onto a heap worklist and released once they are childless, so
 * only the worklist grows with the tree. Once this returns, releasing
 * `root` is O(1) and safe. */
static void
ucl_tree_dismantle(ucl_object_t *root)
{
    ucl_object_t **work = NULL;
    size_t         len  = 0;
    size_t         cap  = 0;
    ucl_object_t  *cur  = root;

    if (root == NULL) return;

    for (;;) {
	ucl_object_t *child;

	while ((child = _ucl_detach_child(cur)) != NULL) {
	    if (!_ucl_worklist_push(&work, &len, &cap, child)) {
		/* Out of memory: fall back to the recursive release. */
		ucl_object_unref(child);
	    }
	}

	if (cur != root) ucl_object_unref(cur);   /* childless now: O(1) */
	if (len == 0) break;
	cur = work[--len];
    }

    free(work);
}


struct ucl_conv_args {
    struct ucl_conv    *cv;
    const ucl_object_t *root;
};

static VALUE
_ucl_convert_root(VALUE arg)
{
    struct ucl_conv_args *a = (struct ucl_conv_args *)arg;
    return _iterate_valid_ucl(a->cv, a->root, 0);
}


/* Convert the tree held by `parser` into Ruby objects.
 *
 * Takes ownership of `parser`: it is released on every path out, including
 * the ones where the conversion is unwound by an exception -- letting one
 * escape would leak the parser and the whole tree it holds. */
static VALUE
ucl_parser_result(struct ucl_parser *parser, int flags)
{
    ucl_object_iter_t iters[UCL_MAX_NESTING];
    struct ucl_conv   cv = { .flags     = flags,
	                     .failed    = false,
	                     .max_depth = -1,
	                     .iters     = iters };
    ucl_object_t     *root;
    VALUE             res;
    int               state = 0;

    if (ucl_parser_get_error(parser)) {
	/* Copy the message into the exception before freeing the parser:
	 * ucl_parser_get_error() points into memory owned by the parser. */
	VALUE err = rb_exc_new2(eUCLError, ucl_parser_get_error(parser));
	ucl_parser_free(parser);
	rb_exc_raise(err);
    }

    root = ucl_parser_get_object(parser);
    if (root == NULL) {
	ucl_parser_free(parser);
	rb_raise(eUCLError, "parser produced no object");
    }

    struct ucl_conv_args args = { &cv, root };
    res = rb_protect(_ucl_convert_root, (VALUE)&args, &state);

    if (state != 0) {
	/* Unwound by an exception: release the iterators the conversion was
	 * still holding, then take the tree apart iteratively, since it may
	 * be deeper than libucl's recursive destructor can walk. */
	int d;
	for (d = 0; d <= cv.max_depth; d++)
	    if (iters[d] != NULL) ucl_object_iterate_free(iters[d]);
	ucl_tree_dismantle(root);
    }

    ucl_parser_free(parser);
    ucl_object_unref(root);

    if (state != 0) rb_jump_tag(state);
    if (cv.failed)  rb_raise(eUCLError, "failed to iterate over ucl object");

    return res;
}


/**
 * Default flags applied by {UCL.parse} and {UCL.load_file} when none are
 * given explicitly.
 *
 * @return [Integer] the current default flags (0 by default)
 */
static VALUE
ucl_s_get_flags(VALUE klass)
{
    /* @flags is a plain instance variable and so is not inherited: a
     * subclass that never set its own falls back to UCL's, which is the
     * value its parse/load_file would have used anyway. */
    VALUE flags = rb_attr_get(klass, rb_intern("@flags"));
    if (NIL_P(flags)) flags = rb_attr_get(mUCL, rb_intern("@flags"));
    return flags;
}


/**
 * Set the default flags applied by {UCL.parse} and {UCL.load_file} when
 * none are given explicitly.
 *
 * @param val [Integer] flags, combined with a bitwise OR
 *
 * @example
 *   UCL.flags = UCL::KEY_SYMBOL | UCL::KEY_LOWERCASE
 *
 * @return [Integer] the flags that were set
 */
static VALUE
ucl_s_set_flags(VALUE klass, VALUE val)
{
    rb_check_type(val, T_FIXNUM);
    rb_iv_set(klass, "@flags", val);
    return val;
}


/* Shared body of UCL.parse and UCL.safe_parse; `forced_flags` are the ones
 * the entry point imposes whatever the caller asked for. */
static VALUE
ucl_parse_string(int argc, VALUE *argv, VALUE klass, int forced_flags)
{
    VALUE data, flags;
    rb_scan_args(argc, argv, "11", &data, &flags);
    if (NIL_P(flags)) flags = ucl_s_get_flags(klass);

    rb_check_type(data,  T_STRING);
    rb_check_type(flags, T_FIXNUM);

    int r_flags = FIX2INT(flags) | forced_flags;
    int c_flags = r_flags & ucl_allowed_c_flags;

    struct ucl_parser *parser =
	ucl_parser_new(c_flags | UCL_PARSER_NO_IMPLICIT_ARRAYS);
    if (parser == NULL)
	rb_raise(eUCLError, "failed to allocate UCL parser");

    ucl_parser_add_chunk(parser,
			 (unsigned char *)RSTRING_PTR(data),
			 RSTRING_LEN(data));

    return ucl_parser_result(parser, r_flags);
}


/**
 * Parse a UCL configuration from a string.
 *
 * Macros are processed, so a configuration is able to pull in other files
 * through <code>.include</code>. Use {UCL.safe_parse} (or the
 * {UCL::DISABLE_MACRO} flag) for input that is not trusted.
 *
 * Objects nested more than 1000 levels deep are refused: both this
 * conversion and libucl's own tree handling recurse per level, so a deeper
 * tree would exhaust the C stack.
 *
 * @overload parse(data, flags = UCL.flags)
 *   @param data  [String]  the UCL configuration to parse
 *   @param flags [Integer] parsing flags combined with a bitwise OR;
 *     defaults to {UCL.flags} when omitted
 *
 * @example
 *   UCL.parse('name = value')            #=> { "name" => "value" }
 *   UCL.parse('name = value', UCL::KEY_SYMBOL)  #=> { :name => "value" }
 *
 * @raise [UCL::Error] if the configuration is malformed, or nested too deeply
 *
 * @return [Hash, Array, Object] the configuration as Ruby objects
 */
static VALUE
ucl_s_parse(int argc, VALUE *argv, VALUE klass)
{
    return ucl_parse_string(argc, argv, klass, 0);
}


/**
 * Parse a UCL configuration from a string with macros disabled.
 *
 * Identical to {UCL.parse} but always adds {UCL::DISABLE_MACRO}, so the
 * configuration cannot reach outside itself through <code>.include</code>.
 * This is the entry point to use for input from an untrusted source.
 *
 * @overload safe_parse(data, flags = UCL.flags)
 *   @param data  [String]  the UCL configuration to parse
 *   @param flags [Integer] parsing flags combined with a bitwise OR;
 *     defaults to {UCL.flags} when omitted; {UCL::DISABLE_MACRO} is added
 *     to whatever is given
 *
 * @example
 *   UCL.safe_parse('.include "/etc/passwd"')  #=> raises UCL::Error
 *
 * @raise [UCL::Error] if the configuration is malformed, or nested too deeply
 *
 * @return [Hash, Array, Object] the configuration as Ruby objects
 */
static VALUE
ucl_s_safe_parse(int argc, VALUE *argv, VALUE klass)
{
    return ucl_parse_string(argc, argv, klass, UCL_PARSER_DISABLE_MACRO);
}


/**
 * Load and parse a UCL configuration from a file.
 *
 * Unlike {UCL.parse}, this defines the file variables ($FILENAME,
 * $CURDIR) from the loaded file, so they can be referenced from within
 * the configuration.
 *
 * Macros are processed, and the nesting limit of {UCL.parse} applies here
 * too.
 *
 * @overload load_file(file, flags = UCL.flags)
 *   @param file  [String]  path to the configuration file
 *   @param flags [Integer] parsing flags combined with a bitwise OR;
 *     defaults to {UCL.flags} when omitted
 *
 * @example
 *   UCL.load_file('foo.conf', UCL::KEY_SYMBOL)
 *
 * @raise [UCL::Error] if the file cannot be read, is malformed, or is
 *   nested too deeply
 *
 * @return [Hash, Array, Object] the configuration as Ruby objects
 */
static VALUE
ucl_s_load_file(int argc, VALUE *argv, VALUE klass)
{
    VALUE file, flags;
    rb_scan_args(argc, argv, "11", &file, &flags);
    if (NIL_P(flags)) flags = ucl_s_get_flags(klass);

    rb_check_type(file,  T_STRING);
    rb_check_type(flags, T_FIXNUM);

    int   r_flags = FIX2INT(flags);
    int   c_flags = r_flags & ucl_allowed_c_flags;
    char *c_file  = StringValueCStr(file);

    struct ucl_parser *parser =
	ucl_parser_new(c_flags | UCL_PARSER_NO_IMPLICIT_ARRAYS);
    if (parser == NULL)
	rb_raise(eUCLError, "failed to allocate UCL parser");

    /* Before the file is read, not after: the variables are substituted
     * while parsing, so setting them afterwards would have no effect at
     * all. (libucl sets them from the file as well.) */
    ucl_parser_set_filevars(parser, c_file, false);
    ucl_parser_add_file(parser, c_file);

    return ucl_parser_result(parser, r_flags);
}




void Init_ucl(void) {
    /* Main classes */
    mUCL      = rb_define_class("UCL", rb_cObject);
    eUCLError = rb_define_class_under(mUCL, "Error", rb_eStandardError);

    /* Constants */
    rb_define_const(mUCL, "KEY_LOWERCASE", INT2FIX(UCL_PARSER_KEY_LOWERCASE));
    rb_define_const(mUCL, "NO_TIME",       INT2FIX(UCL_PARSER_NO_TIME      ));
    rb_define_const(mUCL, "DISABLE_MACRO", INT2FIX(UCL_PARSER_DISABLE_MACRO));
    rb_define_const(mUCL, "NO_FILEVARS",   INT2FIX(UCL_PARSER_NO_FILEVARS  ));
    rb_define_const(mUCL, "KEY_SYMBOL",    INT2FIX(UCL_PARSER_KEY_SYMBOL   ));

    /* Variables */
    ucl_s_set_flags(mUCL, INT2FIX(0));

    /* Definitions */
    rb_define_singleton_method(mUCL, "load_file",  ucl_s_load_file,  -1);
    rb_define_singleton_method(mUCL, "parse",      ucl_s_parse,      -1);
    rb_define_singleton_method(mUCL, "safe_parse", ucl_s_safe_parse, -1);
    rb_define_singleton_method(mUCL, "flags",      ucl_s_get_flags,   0);
    rb_define_singleton_method(mUCL, "flags=",     ucl_s_set_flags,   1);
}
