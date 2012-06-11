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
# define PHP_LEVELDB_SAFE_MODE_CHECK(file) || (PG(safe_mode) && !php_checkuid((file), "rb+", CHECKUID_CHECK_MODE_PARAM))
#else
# define PHP_LEVELDB_SAFE_MODE_CHECK
#endif


#define PHP_LEVELDB_CHECK_OPEN_BASEDIR(file) \
	if (php_check_open_basedir((file) TSRMLS_CC) PHP_LEVELDB_SAFE_MODE_CHECK((file))){ \
		RETURN_FALSE; \
	}

#if ZEND_MODULE_API_NO >= 20100525
#define init_properties(intern) object_properties_init(&intern->std, class_type)
#else
#define init_properties(intern) zend_hash_copy(intern->std.properties, \
    &class_type->default_properties, (copy_ctor_func_t) zval_add_ref,  \
    (void *) &tmp, sizeof(zval *))
#endif

#define php_leveldb_obj_new(obj, class_type)					\
  zend_object_value retval;										\
  obj *intern;													\
  zval *tmp;                                            		\
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
		char *err_msg = estrdup((err)); \
		free((err)); \
		zend_throw_exception(zend_exception_get_default(TSRMLS_C), err_msg, 0 TSRMLS_CC); \
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
zend_class_entry *php_leveldb_class_entry;
zend_class_entry *php_leveldb_write_batch_class_entry;
zend_class_entry *php_leveldb_iterator_class_entry;

/* Objects */
typedef struct {
	zend_object std;
	leveldb_t *db;
} leveldb_object;

typedef struct {
	zend_object std;
	leveldb_writebatch_t *batch;
} leveldb_write_batch_object;

void php_leveldb_object_free(void *object TSRMLS_DC)
{
	leveldb_object *obj = (leveldb_object *)object;

	if (obj->db) {
		leveldb_close(obj->db);
	}

	zend_objects_free_object_storage((zend_object *)object TSRMLS_CC);
}

static zend_object_value php_leveldb_object_new(zend_class_entry *class_type TSRMLS_CC)
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

static zend_object_value php_leveldb_write_batch_object_new(zend_class_entry *class_type TSRMLS_CC)
{
	php_leveldb_obj_new(leveldb_write_batch_object, class_type);
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

/* {{{ proto LevelDB LevelDB::__construct(string $name [, array $options [, array $readoptions [, array $writeoptions]]])
   Instantiates an LevelDB object and opens then give database. */
PHP_METHOD(LevelDB, __construct)
{
	char *name;
	int name_len;
	zval *options = NULL, *read_options = NULL, *write_options = NULL;

	char *err = NULL;
	leveldb_t *db = NULL;
	leveldb_options_t *open_options;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|zzz",
			&name, &name_len, &options, &read_options, &write_options) == FAILURE) {
		return;
	}

	PHP_LEVELDB_CHECK_OPEN_BASEDIR(name);

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);

	/* leveldb create_if_missing default is false, make it true as default */
	open_options = leveldb_options_create();
	leveldb_options_set_create_if_missing(open_options, 1);

	db = leveldb_open(open_options, (const char *)name, &err);

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
	zval *read_options_zv = NULL;

	char *err = NULL;
	leveldb_readoptions_t *read_options;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z",
			&key, &key_len, &read_options_zv) == FAILURE) {
		return;
	}

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	read_options = leveldb_readoptions_create();

	value = leveldb_get(intern->db, read_options, key, key_len, (size_t *)&value_len, &err);

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
	zval *write_options_zv = NULL;

	char *err = NULL;
	leveldb_writeoptions_t *write_options;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|z",
			&key, &key_len, &value, &value_len, &write_options_zv) == FAILURE) {
		return;
	}

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	write_options = leveldb_writeoptions_create();

	leveldb_put(intern->db, write_options, key, key_len, value, value_len, &err);

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
	zval *write_options_zv = NULL;

	char *err = NULL;
	leveldb_writeoptions_t *write_options;
	leveldb_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z",
			&key, &key_len, &write_options_zv) == FAILURE) {
		return;
	}

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	write_options = leveldb_writeoptions_create();

	leveldb_delete(intern->db, write_options, key, key_len, &err);

	LEVELDB_CHECK_ERROR(err);

	if (err != NULL) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/*	{{{ proto bool LevelDB::destroy(string $name [, array $options])
	Destroy the contents of the specified database. */
PHP_METHOD(LevelDB, destroy)
{
	char *name;
	int name_len;
	zval *options_zv;

	char *err = NULL;
	leveldb_options_t *options;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_DC, "s|z", 
			&name, &name_len, &options_zv) == FAILURE) {
		return;
	}

	PHP_LEVELDB_CHECK_OPEN_BASEDIR(name);

	options = leveldb_options_create();

	leveldb_destroy_db(options, name, &err);

	LEVELDB_CHECK_ERROR(err);

	RETURN_TRUE;
}
/*	}}} */

/*	{{{ proto bool LevelDB::write(LevelDBWriteBatch $batch [, array $write_options])
	Executes all of the operations added in the write batch. */
PHP_METHOD(LevelDB, write)
{
	zval *write_options_zv = NULL;
	zval *write_batch;

	char *err = NULL;
	leveldb_object *intern;
	leveldb_writeoptions_t *write_options;
	leveldb_write_batch_object *write_batch_object;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_DC, "O|z",
			&write_batch, php_leveldb_write_batch_class_entry, &write_options_zv) == FAILURE) {
		return;
	}

	intern = (leveldb_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
	write_batch_object = (leveldb_write_batch_object *)zend_object_store_get_object(write_batch TSRMLS_CC);
	write_options = leveldb_writeoptions_create();

	leveldb_write(intern->db, write_options, write_batch_object->batch, &err);

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

	char *err = NULL;
	leveldb_options_t *options;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_DC, "s|z", 
			&name, &name_len, &options_zv) == FAILURE) {
		return;
	}

	PHP_LEVELDB_CHECK_OPEN_BASEDIR(name);

	options = leveldb_options_create();

	leveldb_repair_db(options, name, &err);

	LEVELDB_CHECK_ERROR(err);

	RETURN_TRUE;
}
/*	}}} */


/*  {{{ proto bool LevelDBWriteBatch::__construct()
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

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(leveldb)
{
	zend_class_entry ce;

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
