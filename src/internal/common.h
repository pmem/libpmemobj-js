#ifndef COMMON_H
#define COMMON_H

#include <libpmemobj.h>
#include <stdarg.h>
#include <stdio.h>

typedef PMEMoid PPtr;

// TODO: find proper way to define type num
#define NONE_TYPE_NUM 0
#define ELEMENTS_BASE_TYPE_NUM 10
#define POBJ_TYPE_NUM 20
#define ARRAY_ITEMS_TYPE_NUM 30
#define PDICTKEYSOBJECT_TYPE_NUM 40
#define PNUMDICTKEYSOBJECT_TYPE_NUM 50
#define INTERNAL_ABORT_ERRNO 99999

enum TYPE_CODE {
  // Non container type
  TYPE_CODE_NULL,
  TYPE_CODE_STRING,
  TYPE_CODE_ARRAYBUFFER,
  // Pointer
  TYPE_CODE_SINGLETON,
  TYPE_CODE_NUMBER,
  // Container type
  TYPE_CODE_OBJECT,
  TYPE_CODE_DICT,
  TYPE_CODE_ARRAY,
  TYPE_CODE_NUMDICT,
  TYPE_CODE_INTERNAL_MAX,
};

enum SINGLETON_OFFSET {
  SINGLETON_OFFSET_NULL,
  SINGLETON_OFFSET_TRUE,
  SINGLETON_OFFSET_FALSE,
  SINGLETON_OFFSET_UNDEFINED,
  SINGLETON_OFFSET_JS_NULL,
  SINGLETON_OFFSET_EMPTY,
  SINGLETON_OFFSET_EMPTY_STRING
};

struct PObject {
  uint64_t ob_type;
};

struct PVarObject {
  PObject ob_base;
  uint64_t ob_size;
};

struct PRoot {
  PPtr root_object;
};

struct PDoubleObject {
  PObject ob_base;
  double dval;
};

struct PStringObject {
  PObject ob_base;
};

struct PArrayBufferObject {
  PObject ob_base;
  uint32_t ob_length;
};

struct PObjectObject {
  PObject ob_base;
  PPtr elements;
  PPtr extra_props;
  uint64_t is_array;
};

struct PDictObject {
  PObject ob_base;
  uint64_t ma_used;
  PPtr ma_keys; /* PDictKeysObject */
};

struct PDictKeyEntry {
  uint64_t me_hash;
  PPtr me_key;
  PPtr me_value;
};

struct PDictKeysObject {
  uint64_t dk_size;
  int64_t dk_usable;
  PDictKeyEntry dk_entries[1];
};

struct PArrayObject{
  PVarObject ob_base;
  PPtr ob_items;
  uint64_t allocated;
};

struct PNumDictObject {
  PVarObject ob_base;
  uint64_t ma_used;
  PPtr ma_keys;
};

struct PNumDictKeyEntry {
  uint64_t me_hash;
  // me_state could be EMPTY, DUMMY, or FULL
  uint32_t me_state;
  uint32_t me_key;
  PPtr me_value;
};

struct PNumDictKeysObject {
  uint64_t dk_size;
  int64_t dk_usable;
  PNumDictKeyEntry dk_entries[1];
};

#define PPTR_EQUALS(lhs, rhs) \
  ((lhs).off == (rhs).off && (lhs).pool_uuid_lo == (rhs).pool_uuid_lo)

#define PPTR_IS_NUMBER(pptr) (pptr.pool_uuid_lo == TYPE_CODE_NUMBER)

#define TYPE_CODE_IS_CONTAINER(type_code) \
  (type_code > TYPE_CODE_NUMBER && type_code < TYPE_CODE_INTERNAL_MAX)

// TODO: validate that pool_uuid_lo must not be equal to TYPE_CODE_SINGLETON and
// TYPE_CODE_NUMBER
const PPtr PPTR_NULL = {pool_uuid_lo : 0, off : 0};
const PPtr PPTR_DUMMY = {pool_uuid_lo : 0, off : 1};
const PPtr PPTR_TRUE = {
  pool_uuid_lo : TYPE_CODE_SINGLETON,
  off : SINGLETON_OFFSET_TRUE
};
const PPtr PPTR_FALSE = {
  pool_uuid_lo : TYPE_CODE_SINGLETON,
  off : SINGLETON_OFFSET_FALSE
};
const PPtr PPTR_JS_NULL = {
  pool_uuid_lo : TYPE_CODE_SINGLETON,
  off : SINGLETON_OFFSET_JS_NULL
};
const PPtr PPTR_UNDEFINED = {
  pool_uuid_lo : TYPE_CODE_SINGLETON,
  off : SINGLETON_OFFSET_UNDEFINED
};
const PPtr PPTR_EMPTY = {
  pool_uuid_lo : TYPE_CODE_SINGLETON,
  off : SINGLETON_OFFSET_EMPTY
};
const PPtr PPTR_EMPTY_STRING = {
  pool_uuid_lo : TYPE_CODE_SINGLETON,
  off : SINGLETON_OFFSET_EMPTY_STRING
};
const PPtr PPTR_ZERO = {pool_uuid_lo : TYPE_CODE_NUMBER, off : 0};

const uint32_t SMI_MAX = 2147483647;

class Logger {
 public:
  static void Log(const char *format, ...) {
    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
  };

  static void Debug(const char *format, ...) {
#ifdef DEBUG
    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#endif
  };
};

#endif