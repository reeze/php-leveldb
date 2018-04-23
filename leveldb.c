/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2016 The PHP Group                                |
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

#define LEVELDB_CHECK_OPEN_BASEDIR(file) \
	if (php_check_open_basedir((file))){ \
		RETURN_FALSE; \
	}

#define PHP_LEVELDB_ERROR_DB_CLOSED 1
#define PHP_LEVELDB_ERROR_ITERATOR_CLOSED 2

#define LEVELDB_CHECK_DB_NOT_CLOSED(db_object) \
	if ((db_object)->db == NULL) { \
		zend_throw_exception(php_leveldb_ce_LevelDBException, "Can not operate on closed db", PHP_LEVELDB_ERROR_DB_CLOSED); \
		return; \
	}

/* PHP-LevelDB MAGIC identifier don't change this */
#define PHP_LEVELDB_CUSTOM_COMPARATOR_NAME "php_leveldb.custom_comparator"

#define php_leveldb_obj_new(class_type, ce) \
  class_type *intern; \
                      \
  intern = (class_type *)ecalloc(1, sizeof(class_type) + zend_object_properties_size(ce)); \
                                                                                           \
  zend_object_std_init(&intern->std, ce); \
  object_properties_init(&intern->std, ce); \
                                      \
  intern->std.handlers = & class_type##_handlers; \
                                                  \
  return &intern->std;

#define LEVELDB_CHECK_ERROR(err) \
	if ((err) != NULL) { \
		zend_throw_exception(php_leveldb_ce_LevelDBException, err, 0); \
		leveldb_free(err); \
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
	STANDARD_MODULE_HEADER,
	"leveldb",
	leveldb_functions,
	PHP_MINIT(leveldb),
	PHP_MSHUTDOWN(leveldb),
	NULL,
	NULL,
	PHP_MINFO(leveldb),
	PHP_LEVELDB_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_LEVELDB
ZEND_GET_MODULE(leveldb)
#endif

/* Handlers */
static zend_object_handlers leveldb_object_handlers;
static zend_object_handlers leveldb_snapshot_object_handlers;
static zend_object_handlers leveldb_write_batch_object_handlers;
static zend_object_handlers leveldb_iterator_object_handlers;

/* Class entries */
zend_class_entry *php_leveldb_ce_LevelDBException;
zend_class_entry *php_leveldb_class_entry;
zend_class_entry *php_leveldb_write_batch_class_entry;
zend_class_entry *php_leveldb_iterator_class_entry;
zend_class_entry *php_leveldb_snapshot_class_entry;

/* Objects */
typedef struct {
	leveldb_t *db;
	/* default read options */
	unsigned char verify_check_sum;
	unsigned char fill_cache;
	/* default write options */
	unsigned char sync;

	/* custom comparator */
	leveldb_comparator_t *comparator;
	zend_string *callable_name;
	zend_object std;
} leveldb_object;

typedef struct {
	leveldb_writebatch_t *batch;
	zend_object std;
} leveldb_write_batch_object;

typedef struct {
	leveldb_iterator_t *iterator;
	leveldb_object *db;
	zend_object std;
} leveldb_iterator_object;

/* define an overloaded iterator structure */
typedef struct {
	zend_object_iterator  intern;
	leveldb_iterator_object *iterator_obj;
	zval *current;
} leveldb_iterator_iterator;


typedef struct {
	leveldb_object *db;
	leveldb_snapshot_t *snapshot;
	zend_object std;
} leveldb_snapshot_object;

#define DECLARE_FETCH_OBJECT(class_name) \
	static inline class_name * class_name##_fetch_object(zend_object *obj) \
	{ \
		  return (class_name *)((char *)obj - XtOffsetOf(class_name, std)); \
	}

DECLARE_FETCH_OBJECT(leveldb_object)
DECLARE_FETCH_OBJECT(leveldb_snapshot_object)
DECLARE_FETCH_OBJECT(leveldb_write_batch_object)
DECLARE_FETCH_OBJECT(leveldb_iterator_object)

#define LEVELDB_FETCH_OBJ(obj_name, zv) obj_name##_fetch_object(Z_OBJ_P(zv))
#define LEVELDB_FETCH_ZOBJ(obj_name, obj) obj_name##_fetch_object(obj)

#define FETCH_LEVELDB_Z_OBJ(zv) LEVELDB_FETCH_OBJ(leveldb_object, zv)
#define FETCH_LEVELDB_Z_SNAPSHOT_OBJ(zv) LEVELDB_FETCH_OBJ(leveldb_snapshot_object, zv)
#define FETCH_LEVELDB_Z_WRITE_BATCH_OBJ(zv) LEVELDB_FETCH_OBJ(leveldb_write_batch_object, zv)
#define FETCH_LEVELDB_Z_ITERATOR_OBJ(zv) LEVELDB_FETCH_OBJ(leveldb_iterator_object, zv)

#define FETCH_LEVELDB_OBJ(obj) LEVELDB_FETCH_ZOBJ(leveldb_object, obj)
#define FETCH_LEVELDB_SNAPSHOT_OBJ(obj) LEVELDB_FETCH_ZOBJ(leveldb_snapshot_object, obj)
#define FETCH_LEVELDB_WRITE_BATCH_OBJ(obj) LEVELDB_FETCH_ZOBJ(leveldb_write_batch_object, obj)
#define FETCH_LEVELDB_ITERATOR_OBJ(obj) LEVELDB_FETCH_ZOBJ(leveldb_iterator_object, obj)

void php_leveldb_object_free(zend_object *std)
{
	leveldb_object *obj = FETCH_LEVELDB_OBJ(std);

	if (obj->db) {
		leveldb_close(obj->db);
	}

	if (obj->comparator) {
		leveldb_comparator_destroy(obj->comparator);
		zend_string_free(obj->callable_name);
	}

	zend_object_std_dtor(std);
}

static zend_object* php_leveldb_object_new(zend_class_entry *class_type)
{
	php_leveldb_obj_new(leveldb_object, class_type);
}

/* Write batch object operate */
void php_leveldb_write_batch_object_free(zend_object *std)
{
	leveldb_write_batch_object *obj = FETCH_LEVELDB_WRITE_BATCH_OBJ(std);

	if (obj->batch) {
		leveldb_writebatch_destroy(obj->batch);
	}

	zend_object_std_dtor(std);
}

static zend_object* php_leveldb_write_batch_object_new(zend_class_entry *class_type)
{
	php_leveldb_obj_new(leveldb_write_batch_object, class_type);
}



/* Iterator object operate */
void php_leveldb_iterator_object_free(zend_object *std)
{
	leveldb_iterator_object *obj = FETCH_LEVELDB_ITERATOR_OBJ(std);

	if (obj->db && obj->db->db && obj->iterator) {
		leveldb_iter_destroy(obj->iterator);
	}

	zend_object_std_dtor(std);
}

static zend_object* php_leveldb_iterator_object_new(zend_class_entry *class_type)
{
	php_leveldb_obj_new(leveldb_iterator_object, class_type);
}

/* Snapshot object operate */
void php_leveldb_snapshot_object_free(zend_object *std)
{
	leveldb_t *db;
	leveldb_snapshot_object *obj = FETCH_LEVELDB_SNAPSHOT_OBJ(std);

	if (obj->snapshot && obj->db) {
		if((db = obj->db->db) != NULL) {
			leveldb_release_snapshot(db, obj->snapshot);
		}
	}

	zend_object_std_dtor(std);
}

static zend_object* php_leveldb_snapshot_object_new(zend_class_entry *class_type)
{
	php_leveldb_obj_new(leveldb_snapshot_object, class_type);
}

/* arg info */
ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_construct, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
	ZEND_ARG_ARRAY_INFO(0, options, 0)
	ZEND_ARG_ARRAY_INFO(0, read_options, 0)
	ZEND_ARG_ARRAY_INFO(0, write_options, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_get, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
	ZEND_ARG_ARRAY_INFO(0, read_options, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_set, 0, 0, 2)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
	ZEND_ARG_ARRAY_INFO(0, write_options, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_delete, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
	ZEND_ARG_ARRAY_INFO(0, write_options, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_write, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, batch, LevelDBWriteBatch, 0)
	ZEND_ARG_ARRAY_INFO(0, write_options, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_getProperty, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_getApproximateSizes, 0, 0, 2)
	ZEND_ARG_ARRAY_INFO(0, start, 0)
	ZEND_ARG_ARRAY_INFO(0, limit, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_compactRange, 0, 0, 2)
	ZEND_ARG_TYPE_INFO(0, start, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, limit, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_get_iterator, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, options, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_destroy, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
	ZEND_ARG_ARRAY_INFO(0, options, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_repair, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
	ZEND_ARG_ARRAY_INFO(0, options, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_iterator_construct, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, db, LevelDB, 0)
	ZEND_ARG_ARRAY_INFO(0, read_options, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_iterator_seek, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_leveldb_snapshot_construct, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, db, LevelDB, 0)
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
	PHP_ME(LevelDBWriteBatch, __construct, arginfo_leveldb_void, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
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
	zval_ptr_dtor(callable);
	efree(callable);
}

static int leveldb_custom_comparator_compare(void *stat, const char *a, size_t alen, const char *b, size_t blen)
{
	zval *callable = (zval *)stat;
	zval params[2], result;
	int ret;

	ZVAL_STRINGL(&params[0], (char *)a, alen);
	ZVAL_STRINGL(&params[1], (char *)b, blen);
	ZVAL_NULL(&result);

	assert(!ZVAL_IS_NULL(callable));

	if (call_user_function(EG(function_table), NULL, callable, &result, 2, params) == SUCCESS && !Z_ISUNDEF(result)) {
		convert_to_long(&result);
		ret = Z_LVAL(result);
	}else{
		ret = 0;
	}

	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&result);

	return ret;
}

static const char* leveldb_custom_comparator_name(void *stat)
{
	return PHP_LEVELDB_CUSTOM_COMPARATOR_NAME;
}

static inline leveldb_options_t* php_leveldb_get_open_options(zval *options_zv, leveldb_comparator_t **out_comparator, zend_string **callable_name)
{
	zval *value;
	HashTable *ht;
	leveldb_options_t *options = leveldb_options_create();

	/* default true */
	leveldb_options_set_create_if_missing(options, 1);

	if (options_zv == NULL) {
		return options;
	}

	ht = Z_ARRVAL_P(options_zv);

	if ((value = zend_hash_str_find(ht, ZEND_STRL("create_if_missing"))) != NULL) {
		leveldb_options_set_create_if_missing(options, zend_is_true(value));
	}

	if ((value = zend_hash_str_find(ht, ZEND_STRL("error_if_exists"))) != NULL) {
		leveldb_options_set_error_if_exists(options, zend_is_true(value));
	}

	if ((value = zend_hash_str_find(ht, ZEND_STRL("paranoid_checks"))) != NULL) {
		leveldb_options_set_paranoid_checks(options, zend_is_true(value));
	}

	if ((value = zend_hash_str_find(ht, ZEND_STRL("write_buffer_size"))) != NULL) {
		convert_to_long(value);
		leveldb_options_set_write_buffer_size(options, Z_LVAL_P(value));
	}

	if ((value = zend_hash_str_find(ht, ZEND_STRL("max_open_files"))) != NULL) {
		convert_to_long(value);
		leveldb_options_set_max_open_files(options, Z_LVAL_P(value));
	}

	if ((value = zend_hash_str_find(ht, ZEND_STRL("block_size"))) != NULL) {
		convert_to_long(value);
		leveldb_options_set_block_size(options, Z_LVAL_P(value));
	}

	if ((value = zend_hash_str_find(ht, ZEND_STRL("block_cache_size"))) != NULL) {
		convert_to_long(value);
		leveldb_options_set_cache(options, leveldb_cache_create_lru(Z_LVAL_P(value)));
	}

	if ((value = zend_hash_str_find(ht, ZEND_STRL("block_restart_interval"))) != NULL) {
		convert_to_long(value);
		leveldb_options_set_block_restart_interval(options, Z_LVAL_P(value));
	}

	if ((value = zend_hash_str_find(ht, ZEND_STRL("compression"))) != NULL) {
		convert_to_long(value);

		switch (Z_LVAL_P(value))
		{
		case leveldb_no_compression:
		case leveldb_snappy_compression:
#ifdef MOJANG_LEVELDB
		case leveldb_zlib_compression:
		case leveldb_zlib_raw_compression:
#endif
			leveldb_options_set_compression(options, Z_LVAL_P(value));
			break;
		default:
			php_error_docref(NULL, E_WARNING, "Unsupported compression type");
			break;
		}
	}

	if ((value = zend_hash_str_find(ht, ZEND_STRL("comparator"))) != NULL
		&& !ZVAL_IS_NULL(value)) {
		leveldb_comparator_t *comparator;
		if (!zend_is_callable(value, 0, callable_name)) {
			zend_throw_exception_ex(php_leveldb_ce_LevelDBException, 0,
				"Invalid open option: comparator, %s() is not callable", ZSTR_VAL(*callable_name));

			zend_string_release(*callable_name);
			*callable_name = NULL;

			leveldb_options_destroy(options);

			return NULL;
		}

		zval *z2 = (zval *)emalloc(sizeof(zval));
		ZVAL_COPY(z2, value);

		comparator = leveldb_comparator_create((void *)(z2),
			leveldb_custom_comparator_destructor, leveldb_custom_comparator_compare,
			leveldb_custom_comparator_name);

		if (comparator) {
			*out_comparator = comparator;
		}

		leveldb_options_set_comparator(options, comparator);
	}

	return options;
}

static inline leveldb_readoptions_t *php_leveldb_get_readoptions(leveldb_object *intern, zval *options_zv)
{
	zval *value;
	HashTable *ht;
	leveldb_readoptions_t *readoptions = leveldb_readoptions_create();

	if (options_zv == NULL) {
		return readoptions;
	}

	ht = Z_ARRVAL_P(options_zv);

	if ((value = zend_hash_str_find(ht, ZEND_STRL("verify_check_sum"))) != NULL) {
		leveldb_readoptions_set_verify_checksums(readoptions, zend_is_true(value));
	} else {
		leveldb_readoptions_set_verify_checksums(readoptions, intern->verify_check_sum);
	}

	if ((value = zend_hash_str_find(ht, ZEND_STRL("fill_cache"))) != NULL) {
		leveldb_readoptions_set_fill_cache(readoptions, zend_is_true(value));
	} else {
		leveldb_readoptions_set_fill_cache(readoptions, intern->fill_cache);
	}

	if ((value = zend_hash_str_find(ht, ZEND_STRL("snapshot"))) != NULL
		&& !ZVAL_IS_NULL(value)) {
		if (Z_TYPE_P(value) == IS_OBJECT && Z_OBJCE_P(value) == php_leveldb_snapshot_class_entry) {
			leveldb_snapshot_object *obj = FETCH_LEVELDB_Z_SNAPSHOT_OBJ(value);
			if (obj->snapshot == NULL) {
				zend_throw_exception_ex(php_leveldb_ce_LevelDBException, 0,
					"Invalid snapshot parameter, it has been released");

				leveldb_readoptions_destroy(readoptions);
				return NULL;
			}

			leveldb_readoptions_set_snapshot(readoptions, obj->snapshot);
		} else {
			zend_throw_exception_ex(php_leveldb_ce_LevelDBException, 0,
				"Invalid snapshot parameter, it must be an instance of LevelDBSnapshot");

			leveldb_readoptions_destroy(readoptions);
			return NULL;
		}
	}

	return readoptions;
}

static inline void php_leveldb_set_readoptions(leveldb_object *intern, zval *options_zv)
{
	zval *value;
	HashTable *ht;

	if (options_zv == NULL) {
		return;
	}

	ht = Z_ARRVAL_P(options_zv);

	if ((value = zend_hash_str_find(ht, ZEND_STRL("verify_check_sum"))) != NULL) {
		intern->verify_check_sum = zend_is_true(value);
	}

	if ((value = zend_hash_str_find(ht, ZEND_STRL("fill_cache"))) != NULL) {
		intern->fill_cache = zend_is_true(value);
	}
}

static inline leveldb_writeoptions_t *php_leveldb_get_writeoptions(leveldb_object *intern, zval *options_zv)
{
	zval *value;
	HashTable *ht;
	leveldb_writeoptions_t *writeoptions = leveldb_writeoptions_create();

	if (options_zv == NULL) {
		return writeoptions;
	}

	ht = Z_ARRVAL_P(options_zv);

	if ((value = zend_hash_str_find(ht, ZEND_STRL("sync"))) != NULL) {
		leveldb_writeoptions_set_sync(writeoptions, zend_is_true(value));
	} else {
		leveldb_writeoptions_set_sync(writeoptions, intern->sync);
	}

	return writeoptions;
}

static inline void php_leveldb_set_writeoptions(leveldb_object *intern, zval *options_zv)
{
	zval *value;

	if (options_zv == NULL) {
		return;
	}

	if ((value = zend_hash_str_find(Z_ARRVAL_P(options_zv), ZEND_STRL("sync"))) != NULL) {
		intern->sync = zend_is_true(value);
	}
}

/* {{{ proto LevelDB LevelDB::__construct(string $name [, array $options [, array $readoptions [, array $writeoptions]]])
   Instantiates a LevelDB object and opens the give database */
PHP_METHOD(LevelDB, __construct)
{
	char *name;
	size_t name_len;
	zval *options_zv = NULL, *readoptions_zv = NULL, *writeoptions_zv = NULL;

	char *err = NULL;
	leveldb_t *db = NULL;
	leveldb_options_t *openoptions;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|a!a!a!",
			&name, &name_len, &options_zv, &readoptions_zv, &writeoptions_zv) == FAILURE) {
		return;
	}

	LEVELDB_CHECK_OPEN_BASEDIR(name);

	intern = FETCH_LEVELDB_Z_OBJ(getThis());

	if (intern->db) {
		leveldb_close(intern->db);
	}

	openoptions = php_leveldb_get_open_options(options_zv, &intern->comparator, &intern->callable_name);

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
	size_t key_len;
	size_t value_len;
	zval *readoptions_zv = NULL;

	char *err = NULL;
	leveldb_readoptions_t *readoptions;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z",
			&key, &key_len, &readoptions_zv) == FAILURE) {
		return;
	}

	intern = FETCH_LEVELDB_Z_OBJ(getThis());
	LEVELDB_CHECK_DB_NOT_CLOSED(intern);

	readoptions = php_leveldb_get_readoptions(intern, readoptions_zv);
	value = leveldb_get(intern->db, readoptions, key, key_len, &value_len, &err);
	leveldb_readoptions_destroy(readoptions);

	LEVELDB_CHECK_ERROR(err);

	if (value == NULL) {
		RETURN_FALSE;
	}

	RETVAL_STRINGL(value, value_len);
	leveldb_free(value);
}
/* }}} */

/*	{{{ proto bool LevelDB::set(string $key, string $value [, array $write_options])
	An alias method for LevelDB::put() */
/*	proto bool LevelDB::put(string $key, string $value [, array $write_options])
	Puts the value for the given key */
PHP_METHOD(LevelDB, set)
{
	char *key, *value;
	size_t key_len, value_len;
	zval *writeoptions_zv = NULL;

	char *err = NULL;
	leveldb_writeoptions_t *writeoptions;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss|z",
			&key, &key_len, &value, &value_len, &writeoptions_zv) == FAILURE) {
		return;
	}

	intern = FETCH_LEVELDB_Z_OBJ(getThis());
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
	size_t key_len;
	zval *writeoptions_zv = NULL;

	char *err = NULL;
	leveldb_writeoptions_t *writeoptions;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z",
			&key, &key_len, &writeoptions_zv) == FAILURE) {
		return;
	}

	intern = FETCH_LEVELDB_Z_OBJ(getThis());
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

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "O|z",
			&write_batch, php_leveldb_write_batch_class_entry, &writeoptions_zv) == FAILURE) {
		return;
	}

	intern = FETCH_LEVELDB_Z_OBJ(getThis());
	LEVELDB_CHECK_DB_NOT_CLOSED(intern);

	writeoptions = php_leveldb_get_writeoptions(intern, writeoptions_zv);
	write_batch_object = FETCH_LEVELDB_Z_WRITE_BATCH_OBJ(write_batch);

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
	size_t name_len;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &name, &name_len) == FAILURE) {
		return;
	}

	intern = FETCH_LEVELDB_Z_OBJ(getThis());
	LEVELDB_CHECK_DB_NOT_CLOSED(intern);

	property = leveldb_property_value(intern->db, (const char *)name);

	if (property == NULL) {
		RETURN_FALSE;
	}

	RETVAL_STRING(property);
	leveldb_free(property);
}
/* }}} */

/*	{{{ proto void LevelDB::compactRange(string $start, string $limit)
	Compact the range between the specified */
PHP_METHOD(LevelDB, compactRange)
{
	char *start, *limit;
	size_t start_len, limit_len;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss", &start, &start_len, &limit, &limit_len) == FAILURE) {
		return;
	}

	intern = FETCH_LEVELDB_Z_OBJ(getThis());
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
	zval *start, *limit, *start_val, *limit_val;
	char **start_key, **limit_key;
	size_t *start_len, *limit_len;
	uint num_ranges, i = 0;
	HashPosition pos_start, pos_limit;
	uint64_t *sizes;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "aa", &start, &limit) == FAILURE) {
		return;
	}

	intern = FETCH_LEVELDB_Z_OBJ(getThis());
	LEVELDB_CHECK_DB_NOT_CLOSED(intern);

	num_ranges = zend_hash_num_elements(Z_ARRVAL_P(start));

	if (num_ranges != zend_hash_num_elements(Z_ARRVAL_P(limit))) {
		php_error_docref(NULL, E_WARNING, "The num of start keys and limit keys didn't match");
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

	while ((start_val = zend_hash_get_current_data_ex(Z_ARRVAL_P(start), &pos_start)) != NULL &&
		(limit_val = zend_hash_get_current_data_ex(Z_ARRVAL_P(limit), &pos_limit)) != NULL) {

		start_key[i] = Z_STRVAL_P(start_val);
		start_len[i] = Z_STRLEN_P(start_val);
		limit_key[i] = Z_STRVAL_P(limit_val);
		limit_len[i] = Z_STRLEN_P(limit_val);

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

	intern = FETCH_LEVELDB_Z_OBJ(getThis());
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

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|a", &readoptions_zv) == FAILURE) {
		return;
	}

	object_init_ex(return_value, php_leveldb_iterator_class_entry);

	zend_call_method(return_value, php_leveldb_iterator_class_entry,
		&php_leveldb_iterator_class_entry->constructor, "__construct", sizeof("__construct") - 1,
		NULL, (readoptions_zv == NULL ? 1 : 2), getThis(), readoptions_zv);
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

	zend_call_method_with_1_params(return_value, php_leveldb_snapshot_class_entry,
		&php_leveldb_snapshot_class_entry->constructor, "__construct", NULL, getThis());
}
/*	}}} */

/*	{{{ proto bool LevelDB::destroy(string $name [, array $options])
	Destroy the contents of the specified database */
PHP_METHOD(LevelDB, destroy)
{
	char *name;
	size_t name_len;
	zval *options_zv = NULL;

	char *err = NULL;
	zend_string *callable_name = NULL;
	leveldb_options_t *options;
	leveldb_comparator_t *comparator = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z",
			&name, &name_len, &options_zv) == FAILURE) {
		return;
	}

	LEVELDB_CHECK_OPEN_BASEDIR(name);

	options = php_leveldb_get_open_options(options_zv, &comparator, &callable_name);

	if (!options) {
		return;
	}

	leveldb_destroy_db(options, name, &err);

	if (comparator) {
		leveldb_comparator_destroy(comparator);
		zend_string_free(callable_name);
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
	size_t name_len;
	zval *options_zv;

	char *err = NULL;
	zend_string *callable_name = NULL;
	leveldb_options_t *options;
	leveldb_comparator_t *comparator = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z",
			&name, &name_len, &options_zv) == FAILURE) {
		return;
	}

	LEVELDB_CHECK_OPEN_BASEDIR(name);

	options = php_leveldb_get_open_options(options_zv, &comparator, &callable_name);

	if (!options) {
		return;
	}

	leveldb_repair_db(options, name, &err);

	if (comparator) {
		leveldb_comparator_destroy(comparator);
		zend_string_free(callable_name);
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

	intern = FETCH_LEVELDB_Z_WRITE_BATCH_OBJ(getThis());
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
	size_t key_len, value_len;

	leveldb_write_batch_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
			&key, &key_len, &value, &value_len) == FAILURE) {
		return;
	}

	intern = FETCH_LEVELDB_Z_WRITE_BATCH_OBJ(getThis());
	leveldb_writebatch_put(intern->batch, key, key_len, value, value_len);

	RETURN_TRUE;
}
/* }}} */

/*  {{{ proto bool LevelDBWriteBatch::delete(string $key)
	Adds a deletion operation for the given key to the write batch */
PHP_METHOD(LevelDBWriteBatch, delete)
{
	char *key;
	size_t key_len;

	leveldb_write_batch_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &key, &key_len) == FAILURE) {
		return;
	}

	intern = FETCH_LEVELDB_Z_WRITE_BATCH_OBJ(getThis());
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

	intern = FETCH_LEVELDB_Z_WRITE_BATCH_OBJ(getThis());
	leveldb_writebatch_clear(intern->batch);

	RETURN_TRUE;

}
/* }}} */

/* {{{ forward declarations to the iterator handlers */
static void leveldb_iterator_dtor(zend_object_iterator *iter);
static int leveldb_iterator_valid(zend_object_iterator *iter);
static zval* leveldb_iterator_current_data(zend_object_iterator *iter);

static void leveldb_iterator_current_key(zend_object_iterator *iter, zval *key);
static void leveldb_iterator_move_forward(zend_object_iterator *iter);
static void leveldb_iterator_rewind(zend_object_iterator *iter);

static int php_leveldb_check_iter_db_not_closed(leveldb_iterator_object *intern) {
	if (intern->iterator == NULL) {
		zend_throw_exception(php_leveldb_ce_LevelDBException, "Iterator has been destroyed", PHP_LEVELDB_ERROR_ITERATOR_CLOSED);
		return FAILURE;
	}
	if (intern->db->db == NULL) {
		intern->iterator = NULL;
		zend_throw_exception(php_leveldb_ce_LevelDBException, "Can not iterate on closed db", PHP_LEVELDB_ERROR_DB_CLOSED);
		return FAILURE;
	}

	return SUCCESS;
}

#define LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern) \
	if(php_leveldb_check_iter_db_not_closed(intern) != SUCCESS){ \
		return; \
	}

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
zend_object_iterator *leveldb_iterator_get_iterator(zend_class_entry *ce, zval *object, int by_ref)
{
	leveldb_iterator_iterator *iterator;
	leveldb_iterator_object *it_object;

	if (by_ref) {
		zend_error(E_ERROR, "An iterator cannot be used with foreach by reference");
	}

	it_object = FETCH_LEVELDB_Z_ITERATOR_OBJ(object);
	iterator = emalloc(sizeof(leveldb_iterator_iterator));

	zend_iterator_init((zend_object_iterator*)iterator);

	iterator->intern.funcs = &leveldb_iterator_funcs;
	ZVAL_COPY(&iterator->intern.data, object);
	iterator->iterator_obj = it_object;
	iterator->current = NULL;

	return (zend_object_iterator*)iterator;
}
/* }}} */

/* {{{ leveldb_iterator_dtor */
static void leveldb_iterator_dtor(zend_object_iterator *iter)
{
	leveldb_iterator_iterator *iterator = (leveldb_iterator_iterator *)iter;

	zval *object = &iterator->intern.data;
	zval_ptr_dtor(object);

	if (iterator->current) {
		zval_ptr_dtor(iterator->current);
		efree(iterator->current);
	}

	iterator->iterator_obj = NULL;
}
/* }}} */

/* {{{ leveldb_iterator_valid */
static int leveldb_iterator_valid(zend_object_iterator *iter)
{
	leveldb_iterator_object *iterator_obj = ((leveldb_iterator_iterator *)iter)->iterator_obj;

	if (php_leveldb_check_iter_db_not_closed(iterator_obj) != SUCCESS) {
		return FAILURE;
	}

	return leveldb_iter_valid(iterator_obj->iterator) ? SUCCESS : FAILURE;
}
/* }}} */

/* {{{ leveldb_iterator_current_data */
static zval* leveldb_iterator_current_data(zend_object_iterator *iter)
{
	char *value;
	size_t value_len;
	zval *current;

	leveldb_iterator_iterator *iterator = (leveldb_iterator_iterator *)iter;

	leveldb_iterator_object *iterator_obj = iterator->iterator_obj;

	current = (zval *)emalloc(sizeof(zval));

	if (php_leveldb_check_iter_db_not_closed(iterator_obj) != SUCCESS) {
		ZVAL_NULL(current);
	} else {
		if (iterator->current) {
			zval_ptr_dtor(iterator->current);
			efree(iterator->current);
		}

		value = (char *)leveldb_iter_value(iterator_obj->iterator, &value_len);

		ZVAL_STRINGL(current, value, value_len);
	}

	iterator->current = current;

	return current;
}
/* }}} */

/* {{{ leveldb_iterator_current_key */
static void leveldb_iterator_current_key(zend_object_iterator *iter, zval *key)
{
	char *cur_key;
	size_t cur_key_len = 0;
	leveldb_iterator_object *iterator_obj = ((leveldb_iterator_iterator *)iter)->iterator_obj;
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(iterator_obj);

	cur_key = (char *)leveldb_iter_key(iterator_obj->iterator, &cur_key_len);
	ZVAL_STRINGL(key, cur_key, cur_key_len);
}
/* }}} */

/* {{{ leveldb_iterator_move_forward */
static void leveldb_iterator_move_forward(zend_object_iterator *iter)
{
	leveldb_iterator_object *iterator_obj = ((leveldb_iterator_iterator *)iter)->iterator_obj;
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(iterator_obj);

	leveldb_iter_next(iterator_obj->iterator);
}
/* }}} */

/* {{{ leveldb_iterator_rewind */
static void leveldb_iterator_rewind(zend_object_iterator *iter)
{
	leveldb_iterator_object *iterator_obj = ((leveldb_iterator_iterator *)iter)->iterator_obj;
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(iterator_obj);

	leveldb_iter_seek_to_first(iterator_obj->iterator);
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

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "O|z",
			&db_zv, php_leveldb_class_entry, &readoptions_zv) == FAILURE) {
		return;
	}

	intern = FETCH_LEVELDB_Z_ITERATOR_OBJ(getThis());
	db_obj = FETCH_LEVELDB_Z_OBJ(db_zv);
	LEVELDB_CHECK_DB_NOT_CLOSED(db_obj);

	readoptions = php_leveldb_get_readoptions(db_obj, readoptions_zv);

	if (!readoptions) {
		return;
	}

	intern->iterator = leveldb_create_iterator(db_obj->db, readoptions);

	leveldb_readoptions_destroy(readoptions);

	intern->db = db_obj;

	leveldb_iter_seek_to_first(intern->iterator);
}
/*	}}} */

/*	{{{ proto bool LevelDBIterator::destroy()
	Destroy the iterator */
PHP_METHOD(LevelDBIterator, destroy)
{
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	intern = FETCH_LEVELDB_Z_ITERATOR_OBJ(getThis());
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

	intern = FETCH_LEVELDB_Z_ITERATOR_OBJ(getThis());
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	leveldb_iter_get_error(intern->iterator, &err);

	if (err == NULL) {
		RETURN_FALSE;
	}

	RETVAL_STRING(err);
	leveldb_free(err);
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

	intern = FETCH_LEVELDB_Z_ITERATOR_OBJ(getThis());
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	if (!leveldb_iter_valid(intern->iterator) ||
			!(value = (char *)leveldb_iter_value(intern->iterator, &value_len))) {
		RETURN_FALSE;
	}

	RETURN_STRINGL(value, value_len);
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

	intern = FETCH_LEVELDB_Z_ITERATOR_OBJ(getThis());
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	if (!leveldb_iter_valid(intern->iterator) ||
			!(key = (char *)leveldb_iter_key(intern->iterator, &key_len))) {
		RETURN_FALSE;
	}

	RETURN_STRINGL(key, key_len);
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

	intern = FETCH_LEVELDB_Z_ITERATOR_OBJ(getThis());
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

	intern = FETCH_LEVELDB_Z_ITERATOR_OBJ(getThis());
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

	intern = FETCH_LEVELDB_Z_ITERATOR_OBJ(getThis());
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

	intern = FETCH_LEVELDB_Z_ITERATOR_OBJ(getThis());
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	leveldb_iter_seek_to_last(intern->iterator);
}
/*	}}} */

/*	{{{ proto void LevelDBIterator::seek(string $key)
	Seeks to a given key in the Iterator if no such key it will point to nearest key */
PHP_METHOD(LevelDBIterator, seek)
{
	char *key;
	size_t key_len;
	leveldb_iterator_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &key, &key_len ) == FAILURE) {
		return;
	}

	intern = FETCH_LEVELDB_Z_ITERATOR_OBJ(getThis());
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

	intern = FETCH_LEVELDB_Z_ITERATOR_OBJ(getThis());
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

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "O",
			&db_zv, php_leveldb_class_entry) == FAILURE) {
		return;
	}

	intern = FETCH_LEVELDB_Z_SNAPSHOT_OBJ(getThis());
	db_obj = FETCH_LEVELDB_Z_OBJ(db_zv);
	LEVELDB_CHECK_DB_NOT_CLOSED(db_obj);

	intern->snapshot = (leveldb_snapshot_t *)leveldb_create_snapshot(db_obj->db);

	intern->db = db_obj;
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

	intern = FETCH_LEVELDB_Z_SNAPSHOT_OBJ(getThis());

	if (intern->db == NULL || intern->snapshot == NULL) {
		return;
	}

	leveldb_object *db = intern->db;
	leveldb_release_snapshot(db->db, intern->snapshot);
	intern->snapshot = NULL;
	intern->db = NULL;
}
/*	}}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(leveldb)
{
	zend_class_entry ce;
	zend_class_entry *exception_ce = zend_exception_get_default();

#define DECLARE_OBJ_HANDLERS(class_type) \
	memcpy(& class_type##_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers)); \
	class_type##_handlers.offset = XtOffsetOf(class_type, std); \
	class_type##_handlers.dtor_obj = zend_objects_destroy_object;\
	class_type##_handlers.free_obj = php_##class_type##_free;

	DECLARE_OBJ_HANDLERS(leveldb_object);
	DECLARE_OBJ_HANDLERS(leveldb_snapshot_object);
	DECLARE_OBJ_HANDLERS(leveldb_write_batch_object);
	DECLARE_OBJ_HANDLERS(leveldb_iterator_object);

	/* Register LevelDB Class */
	INIT_CLASS_ENTRY(ce, "LevelDB", php_leveldb_class_methods);
	ce.create_object = php_leveldb_object_new;
	php_leveldb_class_entry = zend_register_internal_class(&ce);

	/* Register LevelDBWriteBatch Class */
	INIT_CLASS_ENTRY(ce, "LevelDBWriteBatch", php_leveldb_write_batch_class_methods);
	ce.create_object = php_leveldb_write_batch_object_new;
	php_leveldb_write_batch_class_entry = zend_register_internal_class(&ce);

	/* Register LevelDBIterator Class */
	INIT_CLASS_ENTRY(ce, "LevelDBIterator", php_leveldb_iterator_class_methods);
	ce.create_object = php_leveldb_iterator_object_new;
	php_leveldb_iterator_class_entry = zend_register_internal_class(&ce);
	php_leveldb_iterator_class_entry->get_iterator = leveldb_iterator_get_iterator;


	/* Register LevelDBSnapshot Class */
	INIT_CLASS_ENTRY(ce, "LevelDBSnapshot", php_leveldb_snapshot_class_methods);
	ce.create_object = php_leveldb_snapshot_object_new;
	php_leveldb_snapshot_class_entry = zend_register_internal_class(&ce);

	/* Register LevelDBException class */
	INIT_CLASS_ENTRY(ce, "LevelDBException", NULL);
	ce.create_object = exception_ce->create_object;
	php_leveldb_ce_LevelDBException = zend_register_internal_class_ex(&ce, exception_ce);

	/* Register constants */
	REGISTER_LONG_CONSTANT("LEVELDB_NO_COMPRESSION", leveldb_no_compression, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("LEVELDB_SNAPPY_COMPRESSION", leveldb_snappy_compression, CONST_CS | CONST_PERSISTENT);

#ifdef MOJANG_LEVELDB
	REGISTER_LONG_CONSTANT("LEVELDB_ZLIB_COMPRESSION", leveldb_zlib_compression, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("LEVELDB_ZLIB_RAW_COMPRESSION", leveldb_zlib_raw_compression, CONST_CS | CONST_PERSISTENT);
#endif // MOJANG_LEVELDB

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
