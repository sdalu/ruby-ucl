#include <ruby.h>
#include <ruby/io.h>
#include <ucl.h>
#include <stdio.h>
#include <stdbool.h>

/* Fake flag */
#define UCL_PARSER_KEY_SYMBOL (1 << 12)


/**
 * Document-class: UCL
 *
 * UCL configuration file.
 */

/**
 * Document-class: UCL::Error
 *
 * Generic error raised by UCL.
 */



static VALUE mUCL                        = Qundef;
static VALUE eUCLError                   = Qundef;

static int ucl_allowed_c_flags = UCL_PARSER_KEY_LOWERCASE |
	                         UCL_PARSER_NO_TIME       |
	                         UCL_PARSER_DISABLE_MACRO |
                                 UCL_PARSER_NO_FILEVARS   ;



VALUE
_iterate_valid_ucl(ucl_object_t const *root, int flags, bool *failed)
{
    ucl_object_iter_t   it  = ucl_object_iterate_new(NULL);
    const ucl_object_t *obj = NULL;

    VALUE val;

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
	
    case UCL_OBJECT:
	val = rb_hash_new();
	it  = ucl_object_iterate_reset(it, root);
	while ((obj = ucl_object_iterate_safe(it, !true))) {
	    size_t keylen;
	    const char *key = ucl_object_keyl(obj, &keylen);
	    VALUE v_key = rb_str_new(key, keylen);
	    if (flags & UCL_PARSER_KEY_SYMBOL)
		v_key = rb_to_symbol(v_key);
	    rb_hash_aset(val, v_key, _iterate_valid_ucl(obj, flags, failed));
	}
	*failed = ucl_object_iter_chk_excpn(it);
	break;
	
    case UCL_ARRAY:
	val = rb_ary_new();
	it = ucl_object_iterate_reset(it, root);
	while ((obj = ucl_object_iterate_safe(it, !true))) {
	    rb_ary_push(val, _iterate_valid_ucl(obj, flags, failed));
	}
	*failed = ucl_object_iter_chk_excpn(it);
	break;
	
    case UCL_USERDATA:
	val = rb_str_new(root->value.sv, root->len);
	break;
	
    case UCL_NULL:
	val = Qnil;
	break;
	
    default:
	rb_bug("unhandled type (%d)", root->type);
	
    }

    ucl_object_iterate_free(it);
    return val;
}

static VALUE
ucl_s_get_flags(VALUE klass)
{
    return rb_iv_get(klass, "@flags");
}


static VALUE
ucl_s_set_flags(VALUE klass, VALUE val)
{
    rb_check_type(val, T_FIXNUM);
    rb_iv_set(klass, "@flags", val);
    return val;
}


/**
 * Parse a configuration file
 *
 * @param data [String]
 * @param flags [Integer]
 *
 * @return configuration file as ruby objects.
 */
static VALUE
ucl_s_parse(int argc, VALUE *argv, VALUE klass)
{
    VALUE data, flags;
    rb_scan_args(argc, argv, "11", &data, &flags);
    if (NIL_P(flags)) flags = ucl_s_get_flags(mUCL);

    rb_check_type(data,  T_STRING);
    rb_check_type(flags, T_FIXNUM);
	
    int c_flags = FIX2INT(flags) & ucl_allowed_c_flags;

    struct ucl_parser *parser =
	ucl_parser_new(c_flags | UCL_PARSER_NO_IMPLICIT_ARRAYS);

    ucl_parser_add_chunk(parser,
			 (unsigned char *)RSTRING_PTR(data),
			 RSTRING_LEN(data));
    
    if (ucl_parser_get_error(parser)) {
	const char *errormsg = ucl_parser_get_error(parser);
	if (parser != NULL) { ucl_parser_free(parser); }
	rb_raise(eUCLError, "%s", errormsg);
    }

    bool          failed = false;
    ucl_object_t *root   = ucl_parser_get_object(parser);
    VALUE         res    = _iterate_valid_ucl(root, FIX2INT(flags), &failed);

    if (parser != NULL) { ucl_parser_free(parser); }
    if (root   != NULL) { ucl_object_unref(root);  }

    if (failed) {
    	rb_raise(eUCLError, "failed to iterate over ucl object");
    }

    return res;
}


/**
 * Load configuration file
 *
 * @param file  [String] 
 * @param flags [Integer]
 *
 * @example
 *   UCL.load_file('foo.conf', UCL::KEY_SYMBOL)
 *
 * @return configuration file as ruby objects.
 */
static VALUE
ucl_s_load_file(int argc, VALUE *argv, VALUE klass)
{
    VALUE file, flags;
    rb_scan_args(argc, argv, "11", &file, &flags);
    if (NIL_P(flags)) flags = ucl_s_get_flags(mUCL);

    rb_check_type(file,  T_STRING);
    rb_check_type(flags, T_FIXNUM);
	
    int   c_flags = FIX2INT(flags) & ucl_allowed_c_flags;
    char *c_file  = StringValueCStr(file);

    struct ucl_parser *parser =
	ucl_parser_new(c_flags | UCL_PARSER_NO_IMPLICIT_ARRAYS);

    ucl_parser_add_file(parser, c_file);
    ucl_parser_set_filevars(parser, c_file, false);
	
    if (ucl_parser_get_error(parser)) {
	const char *errormsg = ucl_parser_get_error(parser);
	if (parser != NULL) { ucl_parser_free(parser); }
	rb_raise(eUCLError, "%s", errormsg);
    }

    bool          failed = false;
    ucl_object_t *root   = ucl_parser_get_object(parser);
    VALUE         res    = _iterate_valid_ucl(root, FIX2INT(flags), &failed);

    if (parser != NULL) { ucl_parser_free(parser); }
    if (root   != NULL) { ucl_object_unref(root);  }
    
    if (failed) {
    	rb_raise(eUCLError, "failed to iterate over ucl object");
    }

    return res;
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
    rb_define_singleton_method(mUCL, "load_file", ucl_s_load_file, -1);
    rb_define_singleton_method(mUCL, "parse",     ucl_s_parse,     -1);
    rb_define_singleton_method(mUCL, "flags",     ucl_s_get_flags,  0);
    rb_define_singleton_method(mUCL, "flags=",    ucl_s_set_flags,  1);
}


