/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2015 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Reeze Xia <reeze@php.net>                                    |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_interfaces.h"
#include "php_leveldb.h"

#include <leveldb/c.h>

#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4)
# define LEVELDB_SAFE_MODE_CHECK(file) || (PG(safe_mode) && !php_checkuid((file), "rb+", CHECKUID_CHECK_MODE_PARAM))
#else
# define LEVELDB_SAFE_MODE_CHECK(file)
#endif

#define LEVELDB_CHECK_OPEN_BASEDIR(file) \
	if (php_check_open_basedir((file) TSRMLS_CC) LEVELDB_SAFE_MODE_CHECK((file))){ \
		RETURN_FALSE; \
	}

#define PHP_LEVELDB_ERROR_DB_CLOSED 1
#define PHP_LEVELDB_ERROR_ITERATOR_CLOSED 2

#define LEVELDB_CHECK_DB_NOT_CLOSED(db_object) \
	if ((db_object)->db == NULL) { \
		zend_throw_exception(php_leveldb_ce_LevelDBException, "Can not operate on closed db", PHP_LEVELDB_ERROR_DB_CLOSED TSRMLS_CC); \
		return; \
	}

#ifndef PHP_FE_END
# define PHP_FE_END { NULL, NULL, NULL, 0, 0 }
#endif

#if ZEND_MODULE_API_NO < 20090626
# define Z_ADDREF_P(arg) ZVAL_ADDREF(arg)
# define Z_ADDREF_PP(arg) ZVAL_ADDREF(*(arg))
# define Z_DELREF_P(arg) ZVAL_DELREF(arg)
# define Z_DELREF_PP(arg) ZVAL_DELREF(*(arg))
#endif

#ifndef zend_parse_parameters_none
# define zend_parse_parameters_none() zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "")
#endif

#if ZEND_MODULE_API_NO >= 20100525
#define init_properties(intern) object_properties_init(&intern->std, class_type)
#else
#define init_properties(intern) do { \
	zval *tmp; \
	zend_hash_copy(intern->std.properties, \
    &class_type->default_properties, (copy_ctor_func_t) zval_add_ref,  \
    (void *) &tmp, sizeof(zval *)); \
} while(0)
#endif

/* PHP-LevelDB MAGIC identifier don't change this */
#define PHP_LEVELDB_CUSTOM_COMPARATOR_NAME "php_leveldb.custom_comparator"

#define php_leveldb_obj_new(obj, class_type)					\
  zend_object_value retval;										\
  obj *intern;													\
																\
  intern = (obj *)emalloc(sizeof(obj));               			\
  memset(intern, 0, sizeof(obj));                          		\
                                                                \
  zend_object_std_init(&intern->std, class_type TSRMLS_CC);     \
  init_properties(intern);										\
                                                                \
  retval.handle = zend_objects_store_put(intern,				\
     (zend_objects_store_dtor_t) zend_objects_destroy_object,	\
     php_##obj##_free, NULL TSRMLS_CC);							\
  retval.handlers = &leveldb_default_handlers;					\
                                                                \
  return retval;

#define LEVELDB_CHECK_ERROR(err) \
	if ((err) != NULL) { \
		zend_throw_exception(php_leveldb_ce_LevelDBException, err, 0 TSRMLS_CC); \
		free(err); \
		return; \
	}

/* {{{ leveldb_functions[]
 */
const zend_function_entry leveldb_functions[] = {
	PHP_FE_END	/* Must be the last line in leveldb_functions[] */
};
/* }}} */

/* {{{ leveldb_module_entry
 */
zend_module_entry leveldb_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"leveldb",
	leveldb_functions,
	PHP_MINIT(leveldb),
	PHP_MSHUTDOWN(leveldb),
	NULL,
	NULL,
	PHP_MINFO(leveldb),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_LEVELDB_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_LEVELDB
ZEND_GET_MODULE(leveldb)
#endif

/* Handlers */
static zend_object_handlers leveldb_default_handlers;
static zend_object_handlers leveldb_object_handlers;
static zend_object_handlers leveldb_iterator_object_handlers;

/* Class entries */
zend_class_entry *php_leveldb_ce_LevelDBException;
zend_class_entry *php_leveldb_class_entry;
zend_class_entry *php_leveldb_write_batch_class_entry;
zend_class_entry *php_leveldb_iterator_class_entry;
zend_class_entry *php_leveldb_snapshot_class_entry;

/* Objects */
typedef struct {
	zend_object std;
	leveldb_t *db;
	/* default read options */
	unsigned char verify_check_sum;
	unsigned char fill_cache;
	/* default write options */
	unsigned char sync;

	/* custom comparator */
	leveldb_comparator_t *comparator;
	char *callable_name;
} leveldb_object;

typedef struct {
	zend_object std;
	leveldb_writebatch_t *batch;
} leveldb_write_batch_object;

/* define an overloaded iterator structure */
typedef struct {
	zend_object_iterator  intern;
	leveldb_iterator_t *iterator;
	zval **current;
} leveldb_iterator_iterator;

typedef struct {
	zend_object std;
	leveldb_iterator_t *iterator;
	zval *db;
} leveldb_iterator_object;

typedef struct {
	zend_object std;
	zval *db;
	leveldb_snapshot_t *snapshot;
} leveldb_snapshot_object;

void php_leveldb_object_free(void *object TSRMLS_DC)
{
	leveldb_object *obj = (leveldb_object *)object;

	if (obj->db) {
		leveldb_close(obj->db);
	}

	if (obj->comparator) {
		leveldb_comparator_destroy(obj->comparator);
		efree(obj->callable_name);
	}

	zend_objects_free_object_storage((zend_object *)object TSRMLS_CC);
}

static zend_object_value php_leveldb_object_new(zend_class_entry *class_type TSRMLS_DC)
{
	php_leveldb_obj_new(leveldb_object, class_type);
}

/* Write batch object operate */
void php_leveldb_write_batch_object_free(void *object TSRMLS_DC)
{
	leveldb_write_batch_object *obj = (leveldb_write_batch_object *)object;

	if (obj->batch) {
		leveldb_writebatch_destroy(obj->batch);
	}

	zend_objects_free_object_storage((zend_object *)object TSRMLS_CC);
}

static zend_object_value php_leveldb_write_batch_object_new(zend_class_entry *class_type TSRMLS_DC)
{
	php_leveldb_obj_new(leveldb_write_batch_object, class_type);
}

/* Iterator object operate */
void php_leveldb_iterator_object_free(void *object TSRMLS_DC)
{
	leveldb_iterator_object *obj = (leveldb_iterator_object *)object;

	if (obj->iterator && ((leveldb_object *)zend_object_store_get_object(obj->db TSRMLS_CC))->db != NULL) {
		leveldb_iter_destroy(obj->iterator);
	}

	if (obj->db) {
		zval_ptr_dtor(&obj->db);
	}

	zend_objects_free_object_storage((zend_object *)object TSRMLS_CC);
}

static zend_object_value php_leveldb_iterator_object_new(zend_class_entry *class_type TSRMLS_DC)
{
	php_leveldb_obj_new(leveldb_iterator_object, class_type);
}

/* Snapshot object operate */
void php_leveldb_snapshot_object_free(void *object TSRMLS_DC)
{
	leveldb_t *db;
	leveldb_snapshot_object *obj = (leveldb_snapshot_object *)object;

	if (obj->snapshot) {
		if((db = ((leveldb_object *)zend_object_store_get_object(obj->db TSRMLS_CC))->db) != NULL) {
			leveldb_release_snapshot(db, obj->snapshot);
		}
		zval_ptr_dtor(&obj->db);
	}

	zend_objects_free_object_storage((zend_object *)object TSRMLS_CC);
}

static zend_object_value php_leveldb_snapshot_object_new(zend_class_entry *class_type TSRMLS_DC)
{
	php_leveldb_obj_new(leveldb_snapshot_object, class_type);
}

/* arg info */
ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_construct, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, options)
	ZEND_ARG_INFO(0, read_options)
	ZEND_ARG_INFO(0, write_options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_get, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, read_options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_set, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, write_options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_delete, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, write_options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_write, 0, 0, 1)
	ZEND_ARG_INFO(0, batch)
	ZEND_ARG_INFO(0, write_options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_getProperty, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_getApproximateSizes, 0, 0, 2)
	ZEND_ARG_INFO(0, start)
	ZEND_ARG_INFO(0, limit)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_compactRange, 0, 0, 2)
	ZEND_ARG_INFO(0, start)
	ZEND_ARG_INFO(0, limit)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_get_iterator, 0, 0, 1)
	ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_destroy, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_repair, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_iterator_construct, 0, 0, 1)
	ZEND_ARG_INFO(0, db)
	ZEND_ARG_INFO(0, read_options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_iterator_seek, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_snapshot_construct, 0, 0, 1)
	ZEND_ARG_INFO(0, db)
ZEND_END_ARG_INFO()

/* Methods */
/* {{{ php_leveldb_class_methods */
static zend_function_entry php_leveldb_class_methods[] = {
	PHP_ME(LevelDB, __construct, arginfo_leveldb_construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(LevelDB, get, arginfo_leveldb_get, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDB, set, arginfo_leveldb_set, ZEND_ACC_PUBLIC)
	/* make put an alias of set, since leveldb use put */
	PHP_MALIAS(LevelDB,	put, set, arginfo_leveldb_set, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDB, delete, arginfo_leveldb_delete, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDB, write, arginfo_leveldb_write, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDB, getProperty, arginfo_leveldb_getProperty, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDB, getApproximateSizes, arginfo_leveldb_getApproximateSizes, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDB, compactRange, arginfo_leveldb_compactRange, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDB, close, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDB, getIterator, arginfo_leveldb_get_iterator, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDB, getSnapshot, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDB, destroy, arginfo_leveldb_destroy, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(LevelDB, repair, arginfo_leveldb_repair, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_FE_END
};
/* }}} */

/* {{{ php_leveldb_write_batch_class_methods */
static zend_function_entry php_leveldb_write_batch_class_methods[] = {
	PHP_ME(LevelDBWriteBatch, __construct, arginfo_leveldb_construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(LevelDBWriteBatch, set, arginfo_leveldb_set, ZEND_ACC_PUBLIC)
	/* make put an alias of set, since leveldb use put */
	PHP_MALIAS(LevelDBWriteBatch, put, set, arginfo_leveldb_set, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDBWriteBatch, delete, arginfo_leveldb_delete, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDBWriteBatch, clear, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

/* {{{ php_leveldb_iterator_class_methods */
static zend_function_entry php_leveldb_iterator_class_methods[] = {
	PHP_ME(LevelDBIterator, __construct, arginfo_leveldb_iterator_construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(LevelDBIterator, valid, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDBIterator, rewind, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDBIterator, last, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDBIterator, seek, arginfo_leveldb_iterator_seek, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDBIterator, next, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDBIterator, prev, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDBIterator, key, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDBIterator, current, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDBIterator, getError, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
	PHP_ME(LevelDBIterator, destroy, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

/* {{{ php_leveldb_iterator_class_methods */
static zend_function_entry php_leveldb_snapshot_class_methods[] = {
	PHP_ME(LevelDBSnapshot, __construct, arginfo_leveldb_snapshot_construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
	PHP_ME(LevelDBSnapshot, release, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

static void leveldb_custom_comparator_destructor(void *stat)
{
	zval *callable = (zval *)stat;
	zval_ptr_dtor(&callable);
}

static int leveldb_custom_comparator_compare(void *stat, const char *a, size_t alen, const char *b, size_t blen)
{
	zval *callable = (zval *)stat;
	zval *params[2], *result = NULL;
	int ret;
	TSRMLS_FETCH();

	MAKE_STD_ZVAL(params[0]);
	MAKE_STD_ZVAL(params[1]);
	MAKE_STD_ZVAL(result);

	ZVAL_STRINGL(params[0], (char *)a, alen, 1);
	ZVAL_STRINGL(params[1], (char *)b, blen, 1);

	if (call_user_function(EG(function_table), NULL, callable, result, 2, params TSRMLS_CC) == SUCCESS) {
		convert_to_long(result);
	}

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);

	ret = Z_LVAL_P(result);
	zval_ptr_dtor(&result);

	return ret;
}

static const char* leveldb_custom_comparator_name(void *stat)
{
	return PHP_LEVELDB_CUSTOM_COMPARATOR_NAME;
}

static inline leveldb_options_t* php_leveldb_get_open_options(zval *options_zv, leveldb_comparator_t **out_comparator, char **callable_name TSRMLS_DC)
{
	zval **value;
	HashTable *ht;
	leveldb_options_t *options = leveldb_options_create();

	/* default true */
	leveldb_options_set_create_if_missing(options, 1);

	if (options_zv == NULL) {
		return options;
	}

	ht = Z_ARRVAL_P(options_zv);

	if (zend_hash_find(ht, "create_if_missing", sizeof("create_if_missing"), (void **)&value) == SUCCESS) {
		leveldb_options_set_create_if_missing(options, zend_is_true(*value));
	}

	if (zend_hash_find(ht, "error_if_exists", sizeof("error_if_exists"), (void **)&value) == SUCCESS) {
		leveldb_options_set_error_if_exists(options, zend_is_true(*value));
	}

	if (zend_hash_find(ht, "paranoid_checks", sizeof("paranoid_checks"), (void **)&value) == SUCCESS) {
		leveldb_options_set_paranoid_checks(options, zend_is_true(*value));
	}

	if (zend_hash_find(ht, "write_buffer_size", sizeof("write_buffer_size"), (void **)&value) == SUCCESS) {
		convert_to_long(*value);
		leveldb_options_set_write_buffer_size(options, Z_LVAL_PP(value));
	}

	if (zend_hash_find(ht, "max_open_files", sizeof("max_open_files"), (void **)&value) == SUCCESS) {
		convert_to_long(*value);
		leveldb_options_set_max_open_files(options, Z_LVAL_PP(value));
	}

	if (zend_hash_find(ht, "block_size", sizeof("block_size"), (void **)&value) == SUCCESS) {
		convert_to_long(*value);
		leveldb_options_set_block_size(options, Z_LVAL_PP(value));
	}

	if (zend_hash_find(ht, "block_cache_size", sizeof("block_cache_size"), (void **)&value) == SUCCESS) {
		convert_to_long(*value);
		leveldb_options_set_cache(options, leveldb_cache_create_lru(Z_LVAL_PP(value)));
	}

	if (zend_hash_find(ht, "block_restart_interval", sizeof("block_restart_interval"), (void **)&value) == SUCCESS) {
		convert_to_long(*value);
		leveldb_options_set_block_restart_interval(options, Z_LVAL_PP(value));
	}

	if (zend_hash_find(ht, "compression", sizeof("compression"), (void **)&value) == SUCCESS) {
		convert_to_long(*value);
		if (Z_LVAL_PP(value) != leveldb_no_compression && Z_LVAL_PP(value) != leveldb_snappy_compression) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid compression type");
		} else {
			leveldb_options_set_compression(options, Z_LVAL_PP(value));
		}
	}

	if (zend_hash_find(ht, "comparator", sizeof("comparator"), (void **)&value) == SUCCESS
		&& !ZVAL_IS_NULL(*value)) {
		leveldb_comparator_t *comparator;
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 3)
		if (!zend_is_callable(*value, 0, callable_name)) {
#else
		if (!zend_is_callable(*value, 0, callable_name TSRMLS_CC)) {
#endif
			zend_throw_exception_ex(php_leveldb_ce_LevelDBException, 0 TSRMLS_CC,
				"Invalid open option: comparator, %s() is not callable", *callable_name);

			efree(*callable_name);
			*callable_name = NULL;

			leveldb_options_destroy(options);

			return NULL;
		}

		Z_ADDREF_PP(value);
		comparator = leveldb_comparator_create((void *)(*value),
			leveldb_custom_comparator_destructor, leveldb_custom_comparator_compare,
			leveldb_custom_comparator_name);

		if (comparator) {
			*out_comparator = comparator;
		}

		leveldb_options_set_comparator(options, comparator);
	}

	return options;
}

static inline leveldb_readoptions_t *php_leveldb_get_readoptions(leveldb_object *intern, zval *options_zv TSRMLS_DC)
{
	zval **value;
	HashTable *ht;
	leveldb_readoptions_t *readoptions = leveldb_readoptions_create();

	if (options_zv == NULL) {
		return readoptions;
	}

	ht = Z_ARRVAL_P(options_zv);
	if (zend_hash_find(ht, "verify_check_sum", sizeof("verify_check_sum"), (void **)&value) == SUCCESS) {
		leveldb_readoptions_set_verify_checksums(readoptions, zend_is_true(*value));
	} else {
		leveldb_readoptions_set_verify_checksums(readoptions, intern->verify_check_sum);
	}

	if (zend_hash_find(ht, "fill_cache", sizeof("fill_cache"), (void **)&value) == SUCCESS) {
		leveldb_readoptions_set_fill_cache(readoptions, zend_is_true(*value));
	} else {
		leveldb_readoptions_set_fill_cache(readoptions, intern->fill_cache);
	}

	if (zend_hash_find(ht, "snapshot", sizeof("snapshot"), (void **)&value) == SUCCESS
		&& !ZVAL_IS_NULL(*value)) {
		if (Z_TYPE_PP(value) == IS_OBJECT && Z_OBJCE_PP(value) == php_leveldb_snapshot_class_entry) {
			leveldb_snapshot_object *obj = (leveldb_snapshot_object *)zend_object_store_get_object(*value TSRMLS_CC);
			if (obj->snapshot == NULL) {
				zend_throw_exception_ex(php_leveldb_ce_LevelDBException, 0 TSRMLS_CC,
					"Invalid snapshot parameter, it has been released");

				leveldb_readoptions_destroy(readoptions);
				return NULL;
			}

			leveldb_readoptions_set_snapshot(readoptions, obj->snapshot);
		} else {
			zend_throw_exception_ex(php_leveldb_ce_LevelDBException, 0 TSRMLS_CC,
				"Invalid snapshot parameter, it must be an instance of LevelDBSnapshot");

			leveldb_readoptions_destroy(readoptions);
			return NULL;
		}
	}

	return readoptions;
}

static inline void php_leveldb_set_readoptions(leveldb_object *intern, zval *options_zv)
{
	zval **value;
	HashTable *ht;

	if (options_zv == NULL) {
		return;
	}

	ht = Z_ARRVAL_P(options_zv);

	if (zend_hash_find(ht, "verify_check_sum", sizeof("verify_check_sum"), (void **)&value) == SUCCESS) {
		intern->verify_check_sum = zend_is_true(*value);
	}

	if (zend_hash_find(ht, "fill_cache", sizeof("fill_cache"), (void **)&value) == SUCCESS) {
		intern->fill_cache = zend_is_true(*value);
	}
}

static inline leveldb_writeoptions_t *php_leveldb_get_writeoptions(leveldb_object *intern, zval *options_zv)
{
	zval **value;
	HashTable *ht;
	leveldb_writeoptions_t *writeoptions = leveldb_writeoptions_create();

	if (options_zv == NULL) {
		return writeoptions;
	}

	ht = Z_ARRVAL_P(options_zv);
	if (zend_hash_find(ht, "sync", sizeof("sync"), (void **)&value) == SUCCESS) {
		leveldb_writeoptions_set_sync(writeoptions, zend_is_true(*value));
	} else {
		leveldb_writeoptions_set_sync(writeoptions, intern->sync);
	}

	return writeoptions;
}

static inline void php_leveldb_set_writeoptions(leveldb_object *intern, zval *options_zv)
{
	zval **value;

	if (options_zv == NULL) {
		return;
	}

	if (zend_hash_find(Z_ARRVAL_P(options_zv), "sync", sizeof("sync"), (void **)&value) == SUCCESS) {
		intern->sync = zend_is_true(*value);
	}
}

/* {{{ proto LevelDB LevelDB::__construct(string $name [, array $options [, array $readoptions [, array $writeoptions]]])
   Instantiates a LevelDB object and opens the give database */
PHP_METHOD(LevelDB, __construct)
{
	char *name;
	int name_len;
	zval *options_zv = NULL, *readoptions_zv = NULL, *writeoptions_zv = NULL;

	char *err = NULL;
	leveldb_t *db = NULL;
	leveldb_options_t *openoptions;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a!a!a!",
			&name, &name_len, &options_zv, &readoptions_zv, &writeoptions_zv) == FAILURE) {
		return;
	}

	LEVELDB_CHECK_OPEN_BASEDIR(name);

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);

	if (intern->db) {
		leveldb_close(db);
	}

	openoptions = php_leveldb_get_open_options(options_zv, &intern->comparator, &intern->callable_name TSRMLS_CC);

	if (!openoptions) {
		return;
	}

	php_leveldb_set_readoptions(intern, readoptions_zv);
	php_leveldb_set_writeoptions(intern, writeoptions_zv);

	db = leveldb_open(openoptions, (const char *)name, &err);

	leveldb_options_destroy(openoptions);

	LEVELDB_CHECK_ERROR(err);

	intern->db = db;
}
/* }}} */

/*	{{{ proto mixed LevelDB::get(string $key [, array $read_options])
	Returns the value for the given key or false */
PHP_METHOD(LevelDB, get)
{
	char *key, *value;
	int key_len;
	size_t value_len;
	zval *readoptions_zv = NULL;

	char *err = NULL;
	leveldb_readoptions_t *readoptions;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z",
			&key, &key_len, &readoptions_zv) == FAILURE) {
		return;
	}

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_DB_NOT_CLOSED(intern);

	readoptions = php_leveldb_get_readoptions(intern, readoptions_zv TSRMLS_CC);
	value = leveldb_get(intern->db, readoptions, key, key_len, &value_len, &err);
	leveldb_readoptions_destroy(readoptions);

	LEVELDB_CHECK_ERROR(err);

	if (value == NULL) {
		RETURN_FALSE;
	}

	RETVAL_STRINGL(value, value_len, 1);
	free(value);
}
/* }}} */

/*	{{{ proto bool LevelDB::set(string $key, string $value [, array $write_options])
	An alias method for LevelDB::put() */
/*	proto bool LevelDB::put(string $key, string $value [, array $write_options])
	Puts the value for the given key */
PHP_METHOD(LevelDB, set)
{
	char *key, *value;
	int key_len, value_len;
	zval *writeoptions_zv = NULL;

	char *err = NULL;
	leveldb_writeoptions_t *writeoptions;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|z",
			&key, &key_len, &value, &value_len, &writeoptions_zv) == FAILURE) {
		return;
	}

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_DB_NOT_CLOSED(intern);

	writeoptions = php_leveldb_get_writeoptions(intern, writeoptions_zv);
	leveldb_put(intern->db, writeoptions, key, key_len, value, value_len, &err);
	leveldb_writeoptions_destroy(writeoptions);

	LEVELDB_CHECK_ERROR(err);

	RETURN_TRUE;
}
/* }}} */

/*	{{{ proto bool LevelDB::delete(string $key [, array $write_options])
	Deletes the given key */
PHP_METHOD(LevelDB, delete)
{
	char *key;
	int key_len;
	zval *writeoptions_zv = NULL;

	char *err = NULL;
	leveldb_writeoptions_t *writeoptions;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z",
			&key, &key_len, &writeoptions_zv) == FAILURE) {
		return;
	}

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_DB_NOT_CLOSED(intern);

	writeoptions = php_leveldb_get_writeoptions(intern, writeoptions_zv);
	leveldb_delete(intern->db, writeoptions, key, key_len, &err);
	leveldb_writeoptions_destroy(writeoptions);

	LEVELDB_CHECK_ERROR(err);

	if (err != NULL) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/*	{{{ proto bool LevelDB::write(LevelDBWriteBatch $batch [, array $write_options])
	Executes all of the operations added in the write batch */
PHP_METHOD(LevelDB, write)
{
	zval *writeoptions_zv = NULL;
	zval *write_batch;

	char *err = NULL;
	leveldb_object *intern;
	leveldb_writeoptions_t *writeoptions;
	leveldb_write_batch_object *write_batch_object;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|z",
			&write_batch, php_leveldb_write_batch_class_entry, &writeoptions_zv) == FAILURE) {
		return;
	}

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_DB_NOT_CLOSED(intern);

	writeoptions = php_leveldb_get_writeoptions(intern, writeoptions_zv);
	write_batch_object = (leveldb_write_batch_object *)zend_object_store_get_object(write_batch TSRMLS_CC);

	leveldb_write(intern->db, writeoptions, write_batch_object->batch, &err);
	leveldb_writeoptions_destroy(writeoptions);

	LEVELDB_CHECK_ERROR(err);

	RETURN_TRUE;
}
/*	}}} */

/*	{{{ proto mixed LevelDB::getProperty(string $name)
	Returns the property of the db */
PHP_METHOD(LevelDB, getProperty)
{
	char *name, *property;
	int name_len;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len) == FAILURE) {
		return;
	}

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_DB_NOT_CLOSED(intern);

	property = leveldb_property_value(intern->db, (const char *)name);

	if (property == NULL) {
		RETURN_FALSE;
	}

	RETVAL_STRING(property, 1);
	free(property);
}
/* }}} */

/*	{{{ proto void LevelDB::compactRange(string $start, string $limit)
	Compact the range between the specified */
PHP_METHOD(LevelDB, compactRange)
{
	char *start, *limit;
	int start_len, limit_len;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &start, &start_len, &limit, &limit_len) == FAILURE) {
		return;
	}

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_DB_NOT_CLOSED(intern);

	leveldb_compact_range(intern->db, (const char *)start, (size_t)start_len, (const char *)limit, (size_t)limit_len);
}
/* }}} */

/*	{{{ proto mixed LevelDB::getApproximateSizes(array $start, array $limit)
	Get the approximate number of bytes of file system space used by one or more key ranges
	or return false on failure */
PHP_METHOD(LevelDB, getApproximateSizes)
{
	leveldb_object *intern;
	zval *start, *limit, **start_val, **limit_val;
	char **start_key, **limit_key;
	size_t *start_len, *limit_len;
	uint num_ranges, i = 0;
	HashPosition pos_start, pos_limit;
	uint64_t *sizes;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "aa", &start, &limit) == FAILURE) {
		return;
	}

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_DB_NOT_CLOSED(intern);

	num_ranges = zend_hash_num_elements(Z_ARRVAL_P(start));

	if (num_ranges != zend_hash_num_elements(Z_ARRVAL_P(limit))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The num of start keys and limit keys didn't match");
		RETURN_FALSE;
	}

	array_init(return_value);

	start_key = emalloc(num_ranges * sizeof(char *));
	limit_key = emalloc(num_ranges * sizeof(char *));
	start_len = emalloc(num_ranges * sizeof(size_t));
	limit_len = emalloc(num_ranges * sizeof(size_t));
	sizes = emalloc(num_ranges * sizeof(uint64_t));

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(start), &pos_start);
	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(limit), &pos_limit);

	while (zend_hash_get_current_data_ex(Z_ARRVAL_P(start), (void **)&start_val, &pos_start) == SUCCESS &&
		zend_hash_get_current_data_ex(Z_ARRVAL_P(limit), (void **)&limit_val, &pos_limit) == SUCCESS) {

		start_key[i] = Z_STRVAL_PP(start_val);
		start_len[i] = Z_STRLEN_PP(start_val);
		limit_key[i] = Z_STRVAL_PP(limit_val);
		limit_len[i] = Z_STRLEN_PP(limit_val);

		zend_hash_move_forward_ex(Z_ARRVAL_P(start), &pos_start);
		zend_hash_move_forward_ex(Z_ARRVAL_P(limit), &pos_limit);

		++i;
	}

	leveldb_approximate_sizes(intern->db, num_ranges,
		(const char* const *)start_key, (const size_t *)start_len,
		(const char* const *)limit_key, (const size_t *)limit_len, sizes);

	for (i = 0; i < num_ranges; ++i) {
		add_next_index_long(return_value, sizes[i]);
	}

	efree(start_key);
	efree(start_len);
	efree(limit_key);
	efree(limit_len);
	efree(sizes);
}
/*	}}} */

/*	{{{ proto bool LevelDB::close()
	Close the opened db */
PHP_METHOD(LevelDB, close)
{
	leveldb_object *intern;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	if (intern->db) {
		leveldb_close(intern->db);
		intern->db = NULL;
	}

	RETURN_TRUE;
}
/*	}}} */

/*	{{{ proto LevelDBIterator LevelDB::getIterator([array $read_options])
	Gets a new iterator for the db */
PHP_METHOD(LevelDB, getIterator)
{
	zval *readoptions_zv = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a", &readoptions_zv) == FAILURE) {
		return;
	}

	object_init_ex(return_value, php_leveldb_iterator_class_entry);

	zend_call_method(&return_value, php_leveldb_iterator_class_entry,
		&php_leveldb_iterator_class_entry->constructor, "__construct", sizeof("__construct") - 1,
		NULL, (readoptions_zv == NULL ? 1 : 2), getThis(), readoptions_zv TSRMLS_CC);
}
/*	}}} */

/*	{{{ proto LevelDBSnapshot LevelDB::getSnapshot()
	Gets a new snapshot for the db */
PHP_METHOD(LevelDB, getSnapshot)
{
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	object_init_ex(return_value, php_leveldb_snapshot_class_entry);

	zend_call_method_with_1_params(&return_value, php_leveldb_snapshot_class_entry,
		&php_leveldb_snapshot_class_entry->constructor, "__construct", NULL, getThis());
}
/*	}}} */

/*	{{{ proto bool LevelDB::destroy(string $name [, array $options])
	Destroy the contents of the specified database */
PHP_METHOD(LevelDB, destroy)
{
	char *name;
	int name_len;
	zval *options_zv = NULL;

	char *err = NULL, *callable_name = NULL;
	leveldb_options_t *options;
	leveldb_comparator_t *comparator = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z",
			&name, &name_len, &options_zv) == FAILURE) {
		return;
	}

	LEVELDB_CHECK_OPEN_BASEDIR(name);

	options = php_leveldb_get_open_options(options_zv, &comparator, &callable_name TSRMLS_CC);

	if (!options) {
		return;
	}

	leveldb_destroy_db(options, name, &err);

	if (comparator) {
		leveldb_comparator_destroy(comparator);
		efree(callable_name);
	}

	leveldb_options_destroy(options);

	LEVELDB_CHECK_ERROR(err);

	RETURN_TRUE;
}
/*	}}} */

/*	{{{ proto bool LevelDB::repair(string $name [, array $options])
	Repairs the given database */
PHP_METHOD(LevelDB, repair)
{
	char *name;
	int name_len;
	zval *options_zv;

	char *err = NULL, *callable_name = NULL;
	leveldb_options_t *options;
	leveldb_comparator_t *comparator = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z",
			&name, &name_len, &options_zv) == FAILURE) {
		return;
	}

	LEVELDB_CHECK_OPEN_BASEDIR(name);

	options = php_leveldb_get_open_options(options_zv, &comparator, &callable_name TSRMLS_CC);

	if (!options) {
		return;
	}

	leveldb_repair_db(options, name, &err);

	if (comparator) {
		leveldb_comparator_destroy(comparator);
		efree(callable_name);
	}

	leveldb_options_destroy(options);

	LEVELDB_CHECK_ERROR(err);

	RETURN_TRUE;
}
/*	}}} */

/*  {{{ proto LevelDBWriteBatch LevelDBWriteBatch::__construct()
	Instantiates a LevelDBWriteBatch object */
PHP_METHOD(LevelDBWriteBatch, __construct)
{
	leveldb_write_batch_object *intern;
	leveldb_writebatch_t *batch;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = (leveldb_write_batch_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	batch = leveldb_writebatch_create();

	intern->batch = batch;
}
/* }}} */

/*  {{{ proto bool LevelDBWriteBatch::set(string $key, string $value)
	An alias for LevelDBWriteBatch::put() */
/*  proto bool LevelDBWriteBatch::put(string $key, string $value)
	Adds a put operation for the given key and value to the write batch. */
PHP_METHOD(LevelDBWriteBatch, set)
{
	char *key, *value;
	int key_len, value_len;

	leveldb_write_batch_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss",
			&key, &key_len, &value, &value_len) == FAILURE) {
		return;
	}

	intern = (leveldb_write_batch_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	leveldb_writebatch_put(intern->batch, key, key_len, value, value_len);

	RETURN_TRUE;
}
/* }}} */

/*  {{{ proto bool LevelDBWriteBatch::delete(string $key)
	Adds a deletion operation for the given key to the write batch */
PHP_METHOD(LevelDBWriteBatch, delete)
{
	char *key;
	int key_len;

	leveldb_write_batch_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
		return;
	}

	intern = (leveldb_write_batch_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	leveldb_writebatch_delete(intern->batch, key, key_len);

	RETURN_TRUE;
}
/* }}} */

/*  {{{ proto bool LevelDBWriteBatch::clear()
	Clears all of operations in the write batch */
PHP_METHOD(LevelDBWriteBatch, clear)
{
	leveldb_write_batch_object *intern;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = (leveldb_write_batch_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	leveldb_writebatch_clear(intern->batch);

	RETURN_TRUE;

}
/* }}} */

/* {{{ forward declarations to the iterator handlers */
static void leveldb_iterator_dtor(zend_object_iterator *iter TSRMLS_DC);
static int leveldb_iterator_valid(zend_object_iterator *iter TSRMLS_DC);
static void leveldb_iterator_current_data(zend_object_iterator *iter, zval ***data TSRMLS_DC);

#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION > 4)
static void leveldb_iterator_current_key(zend_object_iterator *iter, zval *key TSRMLS_DC);
#else
static int leveldb_iterator_current_key(zend_object_iterator *iter, char **str_key, uint *str_key_len, ulong *int_key TSRMLS_DC);
#endif
static void leveldb_iterator_move_forward(zend_object_iterator *iter TSRMLS_DC);
static void leveldb_iterator_rewind(zend_object_iterator *iter TSRMLS_DC);

/* iterator handler table */
zend_object_iterator_funcs leveldb_iterator_funcs = {
	leveldb_iterator_dtor,
	leveldb_iterator_valid,
	leveldb_iterator_current_data,
	leveldb_iterator_current_key,
	leveldb_iterator_move_forward,
	leveldb_iterator_rewind
};
/* }}} */

/* {{{ leveldb_iterator_get_iterator */
zend_object_iterator *leveldb_iterator_get_iterator(zend_class_entry *ce, zval *object, int by_ref TSRMLS_DC)
{
	leveldb_iterator_iterator *iterator;
	leveldb_iterator_object *it_object;

	if (by_ref) {
		zend_error(E_ERROR, "An iterator cannot be used with foreach by reference");
	}

	Z_ADDREF_P(object);

	it_object = (leveldb_iterator_object *)zend_object_store_get_object(object TSRMLS_CC);
	iterator = emalloc(sizeof(leveldb_iterator_iterator));

	iterator->intern.funcs = &leveldb_iterator_funcs;
	iterator->intern.data = (void *)object;
	iterator->iterator = it_object->iterator;
	iterator->current = NULL;

	return (zend_object_iterator*)iterator;
}
/* }}} */

/* {{{ leveldb_iterator_dtor */
static void leveldb_iterator_dtor(zend_object_iterator *iter TSRMLS_DC)
{
	leveldb_iterator_iterator *iterator = (leveldb_iterator_iterator *)iter;

	if (iterator->intern.data) {
		zval *object =  iterator->intern.data;
		zval_ptr_dtor(&object);
	}

	if (iterator->current) {
		zval_ptr_dtor(iterator->current);
		efree(iterator->current);
	}

	efree(iterator);
}
/* }}} */

/* {{{ leveldb_iterator_valid */
static int leveldb_iterator_valid(zend_object_iterator *iter TSRMLS_DC)
{
	leveldb_iterator_t *iterator = ((leveldb_iterator_iterator *)iter)->iterator;
	return leveldb_iter_valid(iterator) ? SUCCESS : FAILURE;
}
/* }}} */

/* {{{ leveldb_iterator_current_data */
static void leveldb_iterator_current_data(zend_object_iterator *iter, zval ***data TSRMLS_DC)
{
	char *value;
	size_t value_len;
	leveldb_iterator_iterator *iterator = (leveldb_iterator_iterator *)iter;

	if (iterator->current) {
		zval_ptr_dtor(iterator->current);
		efree(iterator->current);
	}

	*data = (zval **) emalloc(sizeof(zval *));
	value = (char *)leveldb_iter_value(iterator->iterator, &value_len);

	MAKE_STD_ZVAL(**data);
	ZVAL_STRINGL(**data, value, value_len, 1);

	iterator->current = *data;
}
/* }}} */

/* {{{ leveldb_iterator_current_key */
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION > 4)
static void leveldb_iterator_current_key(zend_object_iterator *iter, zval *key TSRMLS_DC)
#else
static int leveldb_iterator_current_key(zend_object_iterator *iter, char **str_key, uint *str_key_len, ulong *int_key TSRMLS_DC)
#endif
{
	char *cur_key;
	size_t cur_key_len = 0;
	leveldb_iterator_t *iterator = ((leveldb_iterator_iterator *)iter)->iterator;

	cur_key = (char *)leveldb_iter_key(iterator, &cur_key_len);
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION > 4)
	ZVAL_STRINGL(key, cur_key, cur_key_len, 1);
#else
	*str_key = estrndup(cur_key, cur_key_len);
	*str_key_len = cur_key_len + 1; /* adding the \0 like HashTable does */
	return HASH_KEY_IS_STRING;
#endif
}
/* }}} */

/* {{{ leveldb_iterator_move_forward */
static void leveldb_iterator_move_forward(zend_object_iterator *iter TSRMLS_DC)
{
	leveldb_iterator_t *iterator = ((leveldb_iterator_iterator *)iter)->iterator;
	leveldb_iter_next(iterator);
}
/* }}} */

/* {{{ leveldb_iterator_rewind */
static void leveldb_iterator_rewind(zend_object_iterator *iter TSRMLS_DC)
{
	leveldb_iterator_t *iterator = ((leveldb_iterator_iterator *)iter)->iterator;
	leveldb_iter_seek_to_first(iterator);
}
/* }}} */

/*  {{{ proto LevelDBIterator LevelDBIterator::__construct(LevelDB $db [, array $read_options])
	Instantiates a LevelDBIterator object. */
PHP_METHOD(LevelDBIterator, __construct)
{
	zval *db_zv = NULL;
	leveldb_object *db_obj;
	zval *readoptions_zv = NULL;

	leveldb_iterator_object *intern;
	leveldb_readoptions_t *readoptions;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|z",
			&db_zv, php_leveldb_class_entry, &readoptions_zv) == FAILURE) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	db_obj = (leveldb_object *)zend_object_store_get_object(db_zv TSRMLS_CC);
	LEVELDB_CHECK_DB_NOT_CLOSED(db_obj);

	readoptions = php_leveldb_get_readoptions(db_obj, readoptions_zv TSRMLS_CC);

	if (!readoptions) {
		return;
	}

	intern->iterator = leveldb_create_iterator(db_obj->db, readoptions);

	leveldb_readoptions_destroy(readoptions);

	Z_ADDREF_P(db_zv);
	intern->db = db_zv;

	leveldb_iter_seek_to_first(intern->iterator);
}
/*	}}} */

#define LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern) \
	if (intern->iterator == NULL) { \
		zend_throw_exception(php_leveldb_ce_LevelDBException, "Iterator has been destroyed", PHP_LEVELDB_ERROR_ITERATOR_CLOSED TSRMLS_CC); \
		return; \
	} \
	if (((leveldb_object *)zend_object_store_get_object((intern)->db TSRMLS_CC))->db == NULL) { \
		(intern)->iterator = NULL; \
		zend_throw_exception(php_leveldb_ce_LevelDBException, "Can not iterate on closed db", PHP_LEVELDB_ERROR_DB_CLOSED TSRMLS_CC); \
		return; \
	}

/*	{{{ proto bool LevelDBIterator::destroy()
	Destroy the iterator */
PHP_METHOD(LevelDBIterator, destroy)
{
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	if (intern->iterator) {
		leveldb_iter_destroy(intern->iterator);
		intern->iterator = NULL;
	}

	RETURN_TRUE;
}
/*	}}} */

/*	{{{ proto string LevelDBIterator::getError()
	Return last iterator error */
PHP_METHOD(LevelDBIterator, getError)
{
	char *err = NULL;
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	leveldb_iter_get_error(intern->iterator, &err);

	if (err == NULL) {
		RETURN_FALSE;
	}

	RETVAL_STRING(err, 1);
	free(err);
}
/*	}}} */

/*	{{{ proto string LevelDBIterator::current()
	Return current element */
PHP_METHOD(LevelDBIterator, current)
{
	char *value = NULL;
	size_t value_len;
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	if (!leveldb_iter_valid(intern->iterator) ||
			!(value = (char *)leveldb_iter_value(intern->iterator, &value_len))) {
		RETURN_FALSE;
	}

	RETURN_STRINGL(value, value_len, 1);
}
/*	}}} */

/*	{{{ proto string LevelDBIterator::key()
	Returns the key of current element */
PHP_METHOD(LevelDBIterator, key)
{
	char *key = NULL;
	size_t key_len;
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	if (!leveldb_iter_valid(intern->iterator) ||
			!(key = (char *)leveldb_iter_key(intern->iterator, &key_len))) {
		RETURN_FALSE;
	}

	RETURN_STRINGL(key, key_len, 1);
}
/*	}}} */

/*	{{{ proto void LevelDBIterator::next()
	Moves forward to the next element */
PHP_METHOD(LevelDBIterator, next)
{
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	if (leveldb_iter_valid(intern->iterator)) {
		leveldb_iter_next(intern->iterator);
	}
}
/*	}}} */

/*	{{{ proto void LevelDBIterator::prev()
	Moves backward to the previous element */
PHP_METHOD(LevelDBIterator, prev)
{
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	if (leveldb_iter_valid(intern->iterator)) {
		leveldb_iter_prev(intern->iterator);
	}
}
/*	}}} */

/*	{{{ proto void LevelDBIterator::rewind()
	Rewinds back to the first element of the Iterator */
PHP_METHOD(LevelDBIterator, rewind)
{
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	leveldb_iter_seek_to_first(intern->iterator);
}
/*	}}} */

/*	{{{ proto void LevelDBIterator::last()
	Moves to the last element of the Iterator */
PHP_METHOD(LevelDBIterator, last)
{
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	leveldb_iter_seek_to_last(intern->iterator);
}
/*	}}} */

/*	{{{ proto void LevelDBIterator::seek(string $key)
	Seeks to a given key in the Iterator if no such key it will point to nearest key */
PHP_METHOD(LevelDBIterator, seek)
{
	char *key;
	int key_len;
	leveldb_iterator_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len ) == FAILURE) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	leveldb_iter_seek(intern->iterator, key, (size_t)key_len);
}
/*	}}} */

/*	{{{ proto bool LevelDBIterator::valid()
	Checks if current position is valid */
PHP_METHOD(LevelDBIterator, valid)
{
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	RETURN_BOOL(leveldb_iter_valid(intern->iterator));
}
/*	}}} */

/*  {{{ proto LevelDBSnapshot LevelDBSnapshot::__construct(LevelDB $db)
	Instantiates a LevelDBSnapshot object */
PHP_METHOD(LevelDBSnapshot, __construct)
{
	zval *db_zv = NULL;
	leveldb_object *db_obj;
	leveldb_snapshot_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O",
			&db_zv, php_leveldb_class_entry) == FAILURE) {
		return;
	}

	intern = (leveldb_snapshot_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	db_obj = (leveldb_object *)zend_object_store_get_object(db_zv TSRMLS_CC);
	LEVELDB_CHECK_DB_NOT_CLOSED(db_obj);

	intern->snapshot = (leveldb_snapshot_t *)leveldb_create_snapshot(db_obj->db);

	Z_ADDREF_P(db_zv);
	intern->db = db_zv;
}
/*	}}} */

/*  {{{ proto void LevelDBSnapshot::release()
	Release the LevelDBSnapshot object */
PHP_METHOD(LevelDBSnapshot, release)
{
	leveldb_snapshot_object *intern;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = (leveldb_snapshot_object *)zend_object_store_get_object(getThis() TSRMLS_CC);

	if (!intern->db) {
		return;
	}

	{
		leveldb_object *db = (leveldb_object *)zend_object_store_get_object(intern->db TSRMLS_CC);
		leveldb_release_snapshot(db->db, intern->snapshot);
		zval_ptr_dtor(&intern->db);
		intern->snapshot = NULL;
		intern->db = NULL;
	}
}
/*	}}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(leveldb)
{
	zend_class_entry ce;
	zend_class_entry *exception_ce = zend_exception_get_default(TSRMLS_C);

	memcpy(&leveldb_default_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	memcpy(&leveldb_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	memcpy(&leveldb_iterator_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	/* Register LevelDB Class */
	INIT_CLASS_ENTRY(ce, "LevelDB", php_leveldb_class_methods);
	ce.create_object = php_leveldb_object_new;
	php_leveldb_class_entry = zend_register_internal_class(&ce TSRMLS_CC);

	/* Register LevelDBWriteBatch Class */
	INIT_CLASS_ENTRY(ce, "LevelDBWriteBatch", php_leveldb_write_batch_class_methods);
	ce.create_object = php_leveldb_write_batch_object_new;
	php_leveldb_write_batch_class_entry = zend_register_internal_class(&ce TSRMLS_CC);

	/* Register LevelDBIterator Class */
	INIT_CLASS_ENTRY(ce, "LevelDBIterator", php_leveldb_iterator_class_methods);
	ce.create_object = php_leveldb_iterator_object_new;
	php_leveldb_iterator_class_entry = zend_register_internal_class(&ce TSRMLS_CC);
	php_leveldb_iterator_class_entry->get_iterator = leveldb_iterator_get_iterator;


	/* Register LevelDBSnapshot Class */
	INIT_CLASS_ENTRY(ce, "LevelDBSnapshot", php_leveldb_snapshot_class_methods);
	ce.create_object = php_leveldb_snapshot_object_new;
	php_leveldb_snapshot_class_entry = zend_register_internal_class(&ce TSRMLS_CC);

	/* Register LevelDBException class */
	INIT_CLASS_ENTRY(ce, "LevelDBException", NULL);
	ce.create_object = exception_ce->create_object;
	php_leveldb_ce_LevelDBException = zend_register_internal_class_ex(&ce, exception_ce, NULL TSRMLS_CC);

	/* Register constants */
	REGISTER_LONG_CONSTANT("LEVELDB_NO_COMPRESSION", leveldb_no_compression, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("LEVELDB_SNAPPY_COMPRESSION", leveldb_snappy_compression, CONST_CS | CONST_PERSISTENT);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(leveldb)
{
       return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(leveldb)
{
	char tmp[32];
	snprintf(tmp, 32, "%d.%d", leveldb_major_version(), leveldb_minor_version());

	php_info_print_table_start();
	php_info_print_table_header(2, "leveldb support", "enabled");
	php_info_print_table_row(2, "leveldb extension version", PHP_LEVELDB_VERSION);
	php_info_print_table_row(2, "leveldb library version", tmp);
	php_info_print_table_end();
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
