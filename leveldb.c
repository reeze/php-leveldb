/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2012 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Reeze Xia <reeze.xia@gmail.com>                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "Zend/zend_exceptions.h"
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

#define LEVELDB_CHECK_NOT_CLOSED(db_object) \
	if ((db_object)->db == NULL) { \
		zend_throw_exception(leveldb_ce_LevelDBException, "Can not operate on closed db", 0 TSRMLS_CC); \
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
		zend_throw_exception(leveldb_ce_LevelDBException, err, 0 TSRMLS_CC); \
		free(err); \
		return; \
	}

zend_class_entry *leveldb_ce_LevelDBException;


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
zend_class_entry *php_leveldb_class_entry;
zend_class_entry *php_leveldb_write_batch_class_entry;
zend_class_entry *php_leveldb_iterator_class_entry;

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

	zval_dtor(obj->db);

	zend_objects_free_object_storage((zend_object *)object TSRMLS_CC);
}

static zend_object_value php_leveldb_iterator_object_new(zend_class_entry *class_type TSRMLS_DC)
{
	php_leveldb_obj_new(leveldb_iterator_object, class_type);
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
	PHP_ME(LevelDB, close, arginfo_leveldb_void, ZEND_ACC_PUBLIC)
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
	PHP_FE_END
};
/* }}} */

static void leveldb_custom_comparator_destructor(void *stat)
{
	zval *callable = (zval *)stat;
	zval_dtor(callable);
}

static int leveldb_custom_comparator_compare(void *stat, const char *a, size_t alen, const char *b, size_t blen)
{
	zval *callable = (zval *)stat;
	zval *result = NULL;
	zval *params[2];

	MAKE_STD_ZVAL(params[0]);
	MAKE_STD_ZVAL(params[1]);

	ZVAL_STRINGL(params[0], (char *)a, alen, 1);
	ZVAL_STRINGL(params[1], (char *)b, blen, 1);

	MAKE_STD_ZVAL(result);
	if (call_user_function(EG(function_table), NULL, callable, result, 2, params) == SUCCESS) {
		convert_to_long(result);
	}

	zval_dtor(params[0]);
	zval_dtor(params[1]);

	return Z_LVAL_P(result);
}

static const char* leveldb_custom_comparator_name(void *stat)
{
	return PHP_LEVELDB_CUSTOM_COMPARATOR_NAME;
}

static inline leveldb_options_t* php_leveldb_get_open_options(zval *options_zv, leveldb_comparator_t **out_comparator, char **callable_name)
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

	if (zend_hash_find(ht, "block_restart_interval", sizeof("block_restart_interval"), (void **)&value) == SUCCESS) {
		convert_to_long(*value);
		leveldb_options_set_block_restart_interval(options, Z_LVAL_PP(value));
	}

	if (zend_hash_find(ht, "comparator", sizeof("comparator"), (void **)&value) == SUCCESS) {
		leveldb_comparator_t *comparator;
		if (!zend_is_callable(*value, 0, callable_name)) {
			zend_throw_exception_ex(leveldb_ce_LevelDBException, 0 TSRMLS_CC,
				"Invalid open option: comparator, %s() is not callable", *callable_name);

			efree(*callable_name);
			*callable_name = NULL;

			return NULL;
		}

		Z_ADDREF_PP(value);
		comparator = leveldb_comparator_create((void *)(*value),
			leveldb_custom_comparator_destructor, leveldb_custom_comparator_compare, leveldb_custom_comparator_name);

		if (comparator) {
			*out_comparator = comparator;
		}

		leveldb_options_set_comparator(options, comparator);
	}

	/* TODO
		- Add compression
	*/

	return options;
}

static inline leveldb_readoptions_t *php_leveldb_get_readoptions(leveldb_object *intern, zval *options_zv)
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
   Instantiates a LevelDB object and opens the give database. */
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
	Returns the value for the given key or false. */
PHP_METHOD(LevelDB, get)
{
	char *key, *value;
	int key_len, value_len;
	zval *readoptions_zv = NULL;

	char *err = NULL;
	leveldb_readoptions_t *readoptions;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z",
			&key, &key_len, &readoptions_zv) == FAILURE) {
		return;
	}

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_NOT_CLOSED(intern);

	readoptions = php_leveldb_get_readoptions(intern, readoptions_zv);
	value = leveldb_get(intern->db, readoptions, key, key_len, (size_t *)&value_len, &err);
	leveldb_readoptions_destroy(readoptions);

	LEVELDB_CHECK_ERROR(err);

	if (value == NULL) {
		RETURN_FALSE;
	}

	RETURN_STRINGL(value, value_len, 1);
}
/* }}} */

/*	{{{ proto bool LevelDB::set(string $key, string $value [, array $write_options])
	Sets the value for the given key. */
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
	LEVELDB_CHECK_NOT_CLOSED(intern);

	writeoptions = php_leveldb_get_writeoptions(intern, writeoptions_zv);
	leveldb_put(intern->db, writeoptions, key, key_len, value, value_len, &err);
	leveldb_writeoptions_destroy(writeoptions);

	LEVELDB_CHECK_ERROR(err);

	RETURN_TRUE;
}
/* }}} */

/*	{{{ proto bool LevelDB::delete(string $key [, array $write_options])
	Deletes the given key. */
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
	LEVELDB_CHECK_NOT_CLOSED(intern);

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
	Executes all of the operations added in the write batch. */
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
	LEVELDB_CHECK_NOT_CLOSED(intern);

	writeoptions = php_leveldb_get_writeoptions(intern, writeoptions_zv);
	write_batch_object = (leveldb_write_batch_object *)zend_object_store_get_object(write_batch TSRMLS_CC);

	leveldb_write(intern->db, writeoptions, write_batch_object->batch, &err);
	leveldb_writeoptions_destroy(writeoptions);

	LEVELDB_CHECK_ERROR(err);

	RETURN_TRUE;
}
/*	}}} */

/*	{{{ proto bool LevelDB::close()
	Close the opened db */
PHP_METHOD(LevelDB, close)
{
	leveldb_object *intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	if (intern->db) {
		leveldb_close(intern->db);
		intern->db = NULL;
	}

	RETURN_TRUE;
}
/*	}}} */

/*	{{{ proto bool LevelDB::destroy(string $name [, array $options])
	Destroy the contents of the specified database. */
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

	options = php_leveldb_get_open_options(options_zv, &comparator, &callable_name);

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
	Repairs the given database. */
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

	options = php_leveldb_get_open_options(options_zv, &comparator, &callable_name);

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
	Instantiates a LevelDBWriteBatch object. */
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
	Adds a set operation for the given key and value to the write batch. */
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
	Adds a deletion operation for the given key to the write batch. */
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
	Clears all of operations in the write batch. */
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
static int leveldb_iterator_current_key(zend_object_iterator *iter, char **str_key, uint *str_key_len, ulong *int_key TSRMLS_DC);
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
		Z_DELREF_PP(iterator->current);
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
	int value_len;
	leveldb_iterator_iterator *iterator = (leveldb_iterator_iterator *)iter;

	if (iterator->current) {
		Z_DELREF_PP(iterator->current);
		efree(iterator->current);
	}

	*data = (zval **) emalloc(sizeof(zval *));
	value = (char *)leveldb_iter_value(iterator->iterator, (size_t *)&value_len);

	MAKE_STD_ZVAL(**data);
	ZVAL_STRINGL(**data, value, value_len, 1);

	iterator->current = *data;
}
/* }}} */

/* {{{ leveldb_iterator_current_key */
static int leveldb_iterator_current_key(zend_object_iterator *iter, char **str_key, uint *str_key_len, ulong *int_key TSRMLS_DC)
{
	char *key;
	leveldb_iterator_t *iterator = ((leveldb_iterator_iterator *)iter)->iterator;

	key = (char *)leveldb_iter_key(iterator, (size_t *)str_key_len);
	*str_key = estrndup(key, *str_key_len);
	*str_key_len = *str_key_len + 1; /* adding the \0 like HashTable does */

	return HASH_KEY_IS_STRING;
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
	LEVELDB_CHECK_NOT_CLOSED(db_obj);

	readoptions = php_leveldb_get_readoptions(db_obj, readoptions_zv);
	intern->iterator = leveldb_create_iterator(db_obj->db, readoptions);

	leveldb_readoptions_destroy(readoptions);

	Z_ADDREF_P(db_zv);
	intern->db = db_zv;

	leveldb_iter_seek_to_first(intern->iterator);
}
/*	}}} */

#define LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern) \
	if (((leveldb_object *)zend_object_store_get_object((intern)->db TSRMLS_CC))->db == NULL) { \
		(intern)->iterator = NULL; \
		zend_throw_exception(leveldb_ce_LevelDBException, "Can not iterate on closed db", 0 TSRMLS_CC); \
		return; \
	}

/*	{{{ proto string LevelDBIterator::current()
	Return current element */
PHP_METHOD(LevelDBIterator, current)
{
	char *value = NULL;
	int value_len;
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE ) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	if (!leveldb_iter_valid(intern->iterator) ||
			!(value = (char *)leveldb_iter_value(intern->iterator, (size_t *)&value_len))) {
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
	int key_len;
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE ) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	if (!leveldb_iter_valid(intern->iterator) ||
			!(key = (char *)leveldb_iter_key(intern->iterator, (size_t *)&key_len))) {
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

	if (zend_parse_parameters_none() == FAILURE ) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	leveldb_iter_next(intern->iterator);
}
/*	}}} */

/*	{{{ proto void LevelDBIterator::prev()
	Moves backward to the previous element */
PHP_METHOD(LevelDBIterator, prev)
{
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE ) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	leveldb_iter_prev(intern->iterator);
}
/*	}}} */

/*	{{{ proto void LevelDBIterator::rewind()
	Rewinds back to the first element of the Iterator */
PHP_METHOD(LevelDBIterator, rewind)
{
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE ) {
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

	if (zend_parse_parameters_none() == FAILURE ) {
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

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len ) == FAILURE ) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	leveldb_iter_seek(intern->iterator, key, key_len);
}
/*	}}} */

/*	{{{ proto bool LevelDBIterator::valid()
	Checks if current position is valid */
PHP_METHOD(LevelDBIterator, valid)
{
	leveldb_iterator_object *intern;

	if (zend_parse_parameters_none() == FAILURE ) {
		return;
	}

	intern = (leveldb_iterator_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	LEVELDB_CHECK_ITER_DB_NOT_CLOSED(intern);

	RETURN_BOOL(leveldb_iter_valid(intern->iterator));
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

	/* Register LevelDBException class */
	INIT_CLASS_ENTRY(ce, "LevelDBException", NULL);
	ce.create_object = exception_ce->create_object;
	leveldb_ce_LevelDBException = zend_register_internal_class_ex(&ce, exception_ce, NULL TSRMLS_CC);

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
	php_info_print_table_start();
	php_info_print_table_header(2, "leveldb support", "enabled");
	php_info_print_table_row(2, "leveldb version", PHP_LEVELDB_VERSION);
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
