/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 2019 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_overload.h"

#include "zend_closures.h"
#include "zend_extensions.h"
#include "zend_ini_scanner.h"
#include "zend_vm.h"

zend_extension      zend_extension_entry;

typedef enum _zend_overload_type_t {
    ZEND_OVERLOAD_EMPTY = 0,
	ZEND_OVERLOAD_FUNCTION,
	ZEND_OVERLOAD_METHOD,
} zend_overload_type_t;

typedef struct _zend_overload_t {
	zend_overload_type_t type;
	struct {
	    zend_string *class;
	    zend_string *function;
	} source;
	struct {
	    zend_string *class;
	    zend_string *function;
	} target;
} zend_overload_t;

static zend_overload_t zend_overload_empty = 
    {ZEND_OVERLOAD_EMPTY, {NULL, NULL}, {NULL, NULL}};

static HashTable    zend_overload_configuration;
static HashTable*   zend_overload_configuration_section = NULL;

static HashTable    zend_overload_functions;
static HashTable    zend_overload_methods;
static zend_bool    zend_overload_opcache = 0;
static zend_bool    zend_overload_enabled = 0;
static int          zend_overload_resource = 0;

static void zend_overload_shutdown(zend_extension *extension);

static void zend_overload_configuration_dtor(zval *zv);
static void zend_overload_overload_dtor(zval *zv);

static zend_always_inline zend_bool zend_overload_compile_section(zend_string *name, HashTable *section) {
    zend_overload_t overload = zend_overload_empty;
    zval *class = NULL, 
         *function = NULL;
    
	if ((class = zend_hash_find(section, ZSTR_KNOWN(ZEND_STR_CLASS))) ||
	    (function = zend_hash_find(section, ZSTR_KNOWN(ZEND_STR_FUNCTION)))) {
	    if (class) {
	        const char *split;
	        HashTable  *table;
	        
	        if (!(function = zend_hash_find(section, ZSTR_KNOWN(ZEND_STR_FUNCTION)))) {
	            zend_error(E_ERROR, 
	                "Zend Overload found invalid section in "
	                "configuration: [%s], missing function", 
	                ZSTR_VAL(name));
	            return 0;
	        }
	        if (!(split = zend_memnstr(
	                ZSTR_VAL(name), 
	                ZEND_STRL("::"), 
	                ZSTR_VAL(name)+ZSTR_LEN(name)))) {
	           zend_error(E_ERROR,
	                "Zend Overload found invalid method name in "
	                "configuration: [%s]",
	                ZSTR_VAL(name));
	            return 0;
	        }
	        
	        overload.type = ZEND_OVERLOAD_METHOD;
	        overload.source.class = zend_string_init(
                ZSTR_VAL(name),
                ZSTR_VAL(name)+ZSTR_LEN(name)-split-(sizeof("::")-1), 
                1);
	        overload.source.function = zend_string_init(
                split + (sizeof("::")-1),
                strlen(split - (sizeof("::")-1)),
                1);
	        overload.target.class = Z_STR_P(class);
	        overload.target.function = Z_STR_P(function);
	        
	        if (!(table = zend_hash_find_ptr(&zend_overload_methods, overload.source.class))) {
	            HashTable overloads;
	            
	            zend_hash_init(
	                &overloads, 8, 
	                NULL, zend_overload_overload_dtor, 1);
	            
	            table = zend_hash_add_mem(
	                &zend_overload_methods, 
	                overload.source.class, 
	                &overloads, 
	                sizeof(HashTable));
	        }
	        
	        zend_hash_add_mem(
	            table,
	            overload.source.function,
	            &overload,
	            sizeof(zend_overload_t));
	    } else {
	        overload.type = ZEND_OVERLOAD_FUNCTION;
	        overload.source.function = zend_string_dup(name, 1);
	        overload.target.function = zend_string_dup(Z_STR_P(function), 1);
	        
	        zend_hash_add_mem(
	            &zend_overload_functions,
	            overload.source.function,
	            &overload, 
	            sizeof(zend_overload_t));
	    }

	    return 1;
	}

	zend_error(E_ERROR, "Zend Overload found invalid section in "
	                    "configuration: [%s], missing class and or function", 
	                    ZSTR_VAL(name));
	return 0;
}

static zend_always_inline zend_bool zend_overload_compile(HashTable *configuration) {
    zend_string *name;
    HashTable   *section;
    
    ZEND_HASH_FOREACH_STR_KEY_PTR(configuration, name, section) {
        if (!zend_overload_compile_section(name, section)) {
            return 0;
        }
    } ZEND_HASH_FOREACH_END();
    
    return 1;
}

static void zend_overload_configure(zval *key, zval *value, zval *unused, int type, HashTable *configuration) {
    HashTable   *target;
    
    if (type == ZEND_INI_PARSER_SECTION) {
         HashTable section;
    
        zend_hash_init(&section, 32, NULL, NULL, 1);
        
	    zend_overload_configuration_section = 
	        zend_hash_add_mem(
	            &zend_overload_configuration, 
	            Z_STR_P(key),
	            &section, sizeof(HashTable));
	    return;
    }
    
    if (zend_overload_configuration_section) {
        target = zend_overload_configuration_section;
    } else {
        target = configuration;
    }
    
	if (key && value) {
		if (zend_hash_update(target, Z_STR_P(key), value)) {
		    Z_ADDREF_P(value);
		}
	}
}

static int zend_overload_begin(zend_execute_data *execute_data);

static void zend_overload_configuration_dtor(zval *zv) {
    zend_hash_destroy(Z_PTR_P(zv));
    pefree(Z_PTR_P(zv), 1);
}

static void zend_overload_functions_dtor(zval *zv) {
    pefree(Z_PTR_P(zv), 1);
}

static void zend_overload_methods_dtor(zval *zv) {    
    pefree(Z_PTR_P(zv), 1);
}

static void zend_overload_overload_dtor(zval *zv) {
    zend_overload_t *overload = Z_PTR_P(zv);
    
    if (overload->type == ZEND_OVERLOAD_METHOD) {
        zend_string_release_ex(overload->source.class, 1);
        zend_string_release_ex(overload->source.function, 1);
        if (overload->target.class)
            zend_string_release_ex(overload->target.class, 1);
        if (overload->target.function)
            zend_string_release_ex(overload->target.function, 1);
    }
}

int zend_overload_startup(zend_extension *extension) {
    char *overloads = getenv("ZEND_OVERLOADS");
    
    if (overloads) {
        zend_file_handle fh;
#ifdef ZTS
        ZEND_TSRMLS_CACHE_UPDATE();
#endif
        memset(&fh, 0, sizeof(zend_file_handle));

        fh.filename  = overloads;
        fh.handle.fp = fopen(overloads, "rb");
        if (!fh.handle.fp) {
            zend_error(E_ERROR, "Zend Overload could not open %s", overloads);
            zend_bailout();
        }
        fh.type      = ZEND_HANDLE_FP;

        zend_hash_init(&zend_overload_configuration, 32, NULL, zend_overload_configuration_dtor, 1);
        zend_hash_init(&zend_overload_functions,     32, NULL, zend_overload_functions_dtor,     1);
        zend_hash_init(&zend_overload_methods,       32, NULL, zend_overload_methods_dtor,       1);

        if (zend_parse_ini_file(&fh, 1, 
                ZEND_INI_SCANNER_NORMAL, 
                (zend_ini_parser_cb_t) zend_overload_configure, 
                (void*) &zend_overload_configuration) == FAILURE) {
            zend_error(E_ERROR, "Zend Overload could not parse %s", overloads);
            zend_overload_shutdown(&zend_extension_entry);
            zend_destroy_file_handle(&fh);
            zend_bailout();
            return FAILURE;
        } else if (!zend_overload_compile(&zend_overload_configuration)) {
            zend_overload_shutdown(&zend_extension_entry);
            zend_destroy_file_handle(&fh);
            zend_bailout();
            return FAILURE;
        }

        zend_overload_enabled = 1;
        zend_overload_resource = zend_get_resource_handle(&zend_extension_entry);

        zend_set_user_opcode_handler(ZEND_DO_FCALL,         zend_overload_begin);
        zend_set_user_opcode_handler(ZEND_DO_FCALL_BY_NAME, zend_overload_begin);
        
        zend_file_handle_dtor(&fh);
    }

    if (zend_get_extension("Zend OPcache")) {
        zend_overload_opcache = 1;
        /* must use minimal jit to force fallback for call handlers */
        if (INI_INT("opcache.jit")) {
            zend_string *jit = zend_string_init(
                 ZEND_STRL("opcache.jit"), 1);
	        zend_string *value = strpprintf(0, "%d", 1);

	        zend_alter_ini_entry(jit, value,
		        ZEND_INI_SYSTEM, ZEND_INI_STAGE_ACTIVATE);

	        zend_string_release(jit);
	        zend_string_release(value);
        }
    }
    
    return SUCCESS;
}

static void zend_overload_shutdown(zend_extension *extension) {
	zend_hash_destroy(&zend_overload_configuration);
    zend_hash_destroy(&zend_overload_functions);
    zend_hash_destroy(&zend_overload_methods);
}

static void zend_overload_message(int type, void *arg) {
    if (type == ZEND_EXTMSG_NEW_EXTENSION && !zend_overload_opcache) {
		zend_extension *extension = (zend_extension*) arg;

    if (strcasecmp(extension->name, "zend opcache") == SUCCESS) {
			zend_error(E_ERROR, 
			    "Zend Overload must be loaded after Zend Opcache");

			zend_bailout();
        }
	}
}

static zend_always_inline zend_overload_t* zend_overload_find(zend_function *target) {
    zend_overload_t *overload = NULL;
    
    if (!target->common.scope) {
        overload = zend_hash_find_ptr(
                        &zend_overload_functions, target->common.function_name);
    } else {
        HashTable *overloads = zend_hash_find_ptr(
                                &zend_overload_methods, target->common.scope->name);

        if (!overloads) {
            return NULL;
        }
        
        overload = zend_hash_find_ptr(overloads, target->common.function_name);
    }
   
    return overload;
}

static zend_always_inline void zend_overload_setup(zend_function *source, zend_overload_t *overload) {
    zend_function *target = NULL;
    
    if (overload->type == ZEND_OVERLOAD_METHOD) {
        
    } else {
        zend_string *key = 
            zend_string_tolower(overload->target.function);

        target = zend_hash_find_ptr(EG(function_table), key);
        zend_string_release(key);
    }

    if (source->type == ZEND_INTERNAL_FUNCTION) {
        source->internal_function.reserved[zend_overload_resource] = target;
    } else {
        source->op_array.reserved[zend_overload_resource] = target;
    }
}

static void zend_overload_handle(zend_op_array *ops) {
    zend_overload_t *overload;

    if (!zend_overload_enabled || !ops->function_name) {
        ops->reserved[zend_overload_resource] = NULL;
        return;
    }
    
    overload = zend_overload_find((zend_function*) ops);
    
    if (!overload) {
        return;
    }

    zend_overload_setup((zend_function*) ops, overload);
}

static zend_always_inline zend_function* zend_overload_fetch(zend_function *target) {
    zend_function *overload = NULL;

    if (target->type == ZEND_INTERNAL_FUNCTION) {
        zend_overload_t *found;
        
        overload = target->internal_function.reserved[zend_overload_resource];
        
        if (!overload && (found = zend_overload_find(target))) {
            zend_overload_setup(target, found);
          
            overload = target->internal_function.reserved[zend_overload_resource];
        }
    } else {
        overload = target->op_array.reserved[zend_overload_resource];
    }
    
    if (overload && overload->type == ZEND_USER_FUNCTION) {
        if (!RUN_TIME_CACHE(&overload->op_array)) {
            void *rtc = zend_arena_alloc(
                &CG(arena), overload->op_array.cache_size);

            memset(rtc, 0, overload->op_array.cache_size);

            ZEND_MAP_PTR_INIT(overload->op_array.run_time_cache, 
                zend_arena_alloc(&CG(arena), sizeof(void*)));
            ZEND_MAP_PTR_SET(overload->op_array.run_time_cache, rtc);
        } else {
            memset(
                RUN_TIME_CACHE(&overload->op_array), 
                0, 
                overload->op_array.cache_size);
        }
    }
    
    return overload;
}

static int zend_overload_begin(zend_execute_data *execute_data) {
    zend_function *target;
    zend_function *overload;
    
    if (!EX(call) || 
        !EX(call)->func ||
        !EX(call)->func->common.function_name ||
        (EX(call)->func->common.fn_flags & ZEND_ACC_CLOSURE)) {
        return ZEND_USER_OPCODE_DISPATCH;
    }

    target = EX(call)->func;

    if ((overload = zend_overload_fetch(target))) {
    	zval      closure;
        zval     *argv     = ZEND_CALL_ARG(EX(call), 1);

        zend_vm_stack_free_call_frame(EX(call));

        if (EX(func) && EX(func)->type == ZEND_USER_FUNCTION) {
            if (RUN_TIME_CACHE(&EX(func)->op_array)) {
                memset(
                    RUN_TIME_CACHE(&EX(func)->op_array),
                    0,
                    EX(func)->op_array.cache_size);
            }
        }

        EX(call) = zend_vm_stack_push_call_frame(
		        ZEND_CALL_NESTED, 
		        overload, 
		        overload->common.num_args, 
		        overload->common.scope, 
		        Z_TYPE(EX(This)) == IS_OBJECT ? Z_OBJ(EX(This)) : NULL);

        zend_create_closure(&closure, 
                target, 
                target->common.scope, target->common.scope, 
                Z_TYPE(EX(This)) == IS_OBJECT ? &EX(This) : NULL);

        /* this set refcount is not nice, an attempt to make sure the closure is freed */
        Z_SET_REFCOUNT(closure, 0);
        {
	        uint32_t it = 0, end = ZEND_CALL_NUM_ARGS(EX(call));

	        zval *stack = ecalloc(end - 1, sizeof(zval)),
	             *sptr  = stack,
	             *nptr  = ZEND_CALL_ARG(EX(call), 1);

	        memcpy(stack, argv, (end-1) * sizeof(zval));

	        while (it < end) {
		        if (it == 0) {
			        *nptr = closure;
		        } else {
			        *nptr = *sptr;
			        sptr++;
		        }
		        nptr++;
		        it++;
	        }

	        efree(stack);
        }
    }

    return ZEND_USER_OPCODE_DISPATCH;
}

static int zend_overload_zero(zval *zv) {
    zend_function *function = Z_PTR_P(zv);
    
    if (function->type == ZEND_INTERNAL_FUNCTION) {
        function->internal_function.reserved[zend_overload_resource] = NULL;
    }
    
    return ZEND_HASH_APPLY_KEEP;
}

static int zend_overload_zero_class(zval *zv) {
    zend_class_entry *ce = Z_PTR_P(zv);

    zend_hash_apply(&ce->function_table, zend_overload_zero);

    return ZEND_HASH_APPLY_KEEP;
}

static void zend_overload_activate() {
    if (zend_overload_enabled) {
#ifdef ZTS
        ZEND_TSRMLS_CACHE_UPDATE();
#endif
        zend_hash_apply(CG(function_table), zend_overload_zero);
        zend_hash_apply(CG(class_table),    zend_overload_zero_class);
        
        CG(compiler_options) |= ZEND_COMPILE_IGNORE_USER_FUNCTIONS |
                                ZEND_COMPILE_IGNORE_INTERNAL_FUNCTIONS;
                                
        if (zend_overload_opcache) {
            /*
             we have to disable passes that are not compatible with zend vm
             or ignore compiler flags
            */
            if (INI_INT("opcache.optimization_level")) {
		        zend_string *optimizer = zend_string_init(
			        ZEND_STRL("opcache.optimization_level"), 1);
		        zend_long level = INI_INT("opcache.optimization_level");
		        zend_string *value;
		        
		        /* disable pass 1 (pre-evaluate constant function calls) */
		        level &= ~(1<<0);
		        
		        /* disable pass 4 (optimize function calls) */
		        level &= ~(1<<3);
                
		        value = strpprintf(0, "0x%08X", (unsigned int) level);

		        zend_alter_ini_entry(optimizer, value,
			        ZEND_INI_SYSTEM, ZEND_INI_STAGE_ACTIVATE);

		        zend_string_release(optimizer);
		        zend_string_release(value);
	        }
        }
    }
}

ZEND_OVERLOAD_EXPORT zend_extension zend_extension_entry = {
    ZEND_OVERLOAD_EXTNAME,
    ZEND_OVERLOAD_VERSION,
    ZEND_OVERLOAD_AUTHORS,
    ZEND_OVERLOAD_HELPERS,
    ZEND_OVERLOAD_COPYRIGHT,
    zend_overload_startup,
    zend_overload_shutdown,
    zend_overload_activate,
    NULL,
    zend_overload_message,
    zend_overload_handle,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    STANDARD_ZEND_EXTENSION_PROPERTIES
};

ZEND_OVERLOAD_EXPORT zend_extension_version_info extension_version_info = { 
    ZEND_EXTENSION_API_NO, 
    ZEND_EXTENSION_BUILD_ID 
};

# if defined(ZTS)
ZEND_TSRMLS_CACHE_DEFINE()
# endif
