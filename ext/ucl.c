#include <ruby.h>
#include <ruby/io.h>
#include <ucl.h>
#include <stdio.h>
#include <stdbool.h>

/* Fake flag */
#define UCL_PARSER_KEY_SYMBOL (1 << 10)


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


VALUE
_iterate_valid_ucl(ucl_object_t const *root, int flags)
{
    const ucl_object_t *obj;
    ucl_object_iter_t it    = NULL;
    const ucl_object_t *cur;
    ucl_object_iter_t it_obj = NULL;

    VALUE lst = rb_ary_new();
    
    while ((obj = ucl_object_iterate (root, &it, false))) {
	VALUE val;

	switch (obj->type) {
	case UCL_INT:
	    val = rb_ll2inum((long long)ucl_object_toint(obj));
	    break;
	    
	case UCL_FLOAT:
	    val = rb_float_new(ucl_object_todouble(obj));
	    break;
	    
	case UCL_STRING:
	    val = rb_str_new_cstr(ucl_object_tostring(obj));
	    break;
	    
	case UCL_BOOLEAN:
	    val = ucl_object_toboolean(obj) ? Qtrue : Qfalse;
	    break;
	    
	case UCL_TIME:
	    val = rb_float_new(ucl_object_todouble(obj));
	    break;
	    
	case UCL_OBJECT:
	    it_obj = NULL;
	    val    = rb_hash_new();		
	    while ((cur = ucl_object_iterate(obj, &it_obj, true))) {
		const char *obj_key = ucl_object_key(cur);
		VALUE key = (flags & UCL_PARSER_KEY_SYMBOL)
		          ? rb_id2sym(rb_intern(obj_key))
		          : rb_str_new_cstr(obj_key);
		rb_hash_aset(val, key, _iterate_valid_ucl(cur, flags));
	    }
	    break;
	    
	case UCL_ARRAY:
	    it_obj = NULL;		
	    val    = rb_ary_new();
	    while ((cur = ucl_object_iterate (obj, &it_obj, true))) {
		rb_ary_push(val, _iterate_valid_ucl(cur, flags));
	    }
	    break;

	case UCL_USERDATA:
	    val = rb_str_new(obj->value.sv, obj->len);
	    break;
	    
	case UCL_NULL:
	    val = Qnil;
	    break;

	default:
	    rb_bug("unhandled type (%d)", obj->type);
    
	}
	rb_ary_push(lst, val);

    }

    switch(RARRAY_LENINT(lst)) {
    case  0: return Qnil;
    case  1: return RARRAY_PTR(lst)[0];
    default: return lst;
    }
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
    static int allowed_flags = UCL_PARSER_KEY_LOWERCASE |
	                       UCL_PARSER_NO_TIME       |
	                       UCL_PARSER_DISABLE_MACRO |
	                       UCL_PARSER_NO_FILEVARS   ;

    VALUE data, flags;
    rb_scan_args(argc, argv, "11", &data, &flags);
    if (NIL_P(flags)) flags = ucl_s_get_flags(mUCL);

    rb_check_type(data,  T_STRING);
    rb_check_type(flags, T_FIXNUM);
	
    int c_flags = FIX2INT(flags) & allowed_flags;

    struct ucl_parser *parser =
	ucl_parser_new(c_flags | UCL_PARSER_NO_IMPLICIT_ARRAYS);

    ucl_parser_add_chunk(parser,
			 (unsigned char *)RSTRING_PTR(data),
			 RSTRING_LEN(data));
    
    if (ucl_parser_get_error(parser)) {
	rb_raise(eUCLError, "%s", ucl_parser_get_error(parser));
    }

    ucl_object_t *root = ucl_parser_get_object(parser);

    VALUE res = _iterate_valid_ucl(root, FIX2INT(flags));

    if (parser != NULL) {
	ucl_parser_free(parser);
    }
    
    if (root != NULL) {
	ucl_object_unref(root);
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

    VALUE read_kwargs = rb_hash_new();
    rb_hash_aset(read_kwargs, rb_id2sym(rb_intern("encoding")),
		 rb_str_new_cstr("BINARY"));

    VALUE read_args[] = { file, read_kwargs };
    VALUE data = rb_funcallv_kw(rb_cFile, rb_intern("read"),
				2, read_args, RB_PASS_KEYWORDS);

    VALUE parse_args[] = { data, flags };
    return ucl_s_parse(2, parse_args, klass);
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


