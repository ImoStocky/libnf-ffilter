/*

 Copyright (c) 2015-2017, Tomas Podermanski, Lukas Hutak, Imrich Stoffa

 This file is part of libnf.net project.

 Libnf is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Libnf is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with libnf.  If not, see <http://www.gnu.org/licenses/>.

*/

/**
 * \file ffilter.h
 * \brief netflow fiter implementation - C interface
 */
#ifndef _FLOW_FILTER_H_
#define _FLOW_FILTER_H_//

#if defined _WIN32 || defined __CYGWIN__
#ifdef BUILDING_DLL
    #ifdef __GNUC__
      #define DLL_PUBLIC __attribute__ ((dllexport))
    #else
      #define DLL_PUBLIC __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #else
    #ifdef __GNUC__
      #define DLL_PUBLIC __attribute__ ((dllimport))
    #else
      #define DLL_PUBLIC __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #endif
  #define DLL_LOCAL
#else
#if __GNUC__ >= 4
#define DLL_PUBLIC __attribute__ ((visibility ("default")))
#define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#else
#define DLL_PUBLIC
    #define DLL_LOCAL
#endif
#endif

#include <stdint.h>
#include <stddef.h>

#define FF_MAX_STRING  1024
#define FF_SCALING_FACTOR  1000LL
#define FF_MULTINODE_MAX 4

#ifndef HAVE_HTONLL
#ifdef WORDS_BIGENDIAN
#   define ntohll(n)    (n)
#   define htonll(n)    (n)
#else
#   define ntohll(n)    ((((uint64_t)ntohl(n)) << 32) | ntohl(((uint64_t)(n)) >> 32))
#   define htonll(n)    ((((uint64_t)htonl(n)) << 32) | htonl(((uint64_t)(n)) >> 32))
#endif
#define HAVE_HTONLL 1
#endif


/*! \brief Supported data types */
typedef enum {
	FF_TYPE_UNSUPPORTED = 0x0,	// for unsupported data types

	FF_TYPE_UNSIGNED,
	FF_TYPE_UNSIGNED_BIG,
	FF_TYPE_SIGNED,
	FF_TYPE_SIGNED_BIG,
	FF_TYPE_UINT8,			/* 1Byte unsigned - fixed size */
	FF_TYPE_UINT16,
	FF_TYPE_UINT32,
	FF_TYPE_UINT64,
	FF_TYPE_INT8,			/* 1Byte unsigned - fixed size */
	FF_TYPE_INT16,
	FF_TYPE_INT32,
	FF_TYPE_INT64,

	FF_TYPE_DOUBLE,			/* muzeme si byt jisti, ze se bude pouzivat format IEEE 754. */
	FF_TYPE_ADDR,
	FF_TYPE_MAC,
	FF_TYPE_STRING,
	FF_TYPE_MPLS,
	FF_TYPE_TIMESTAMP,      /* uint64_t bit timestamp eval as unsigned, milliseconds from 1-1-1970 00:00:00 */
	FF_TYPE_TIMESTAMP_BIG,  /* uint64_t bit timestamp eval as unsigned, to host byte order conversion required */
} ff_type_t;


//Some of the types here are useless - why define another fixed size types ?
typedef void ff_unsup_t;
typedef char* ff_uint_t;
typedef char* ff_int_t;
typedef uint8_t ff_uint8_t;
typedef uint16_t ff_uint16_t;
typedef uint32_t ff_uint32_t;
typedef uint64_t ff_uint64_t;
typedef int8_t ff_int8_t;
typedef int16_t ff_int16_t;
typedef int32_t ff_int32_t;
typedef int64_t ff_int64_t;
typedef double ff_double_t;
typedef char ff_mac_t[8];
typedef uint64_t ff_timestamp_t;

typedef struct ff_ip_s { uint32_t data[4]; } ff_ip_t; /*!< IPv4/IPv6 address */
typedef struct ff_net_s { ff_ip_t ip; ff_ip_t mask; int ver; } ff_net_t;

typedef struct ff_mpls_s { uint32_t val; uint32_t label; } ff_mpls_t; /*!< In node type for mpls */

typedef union {
	//TODO: Test on big-endian machine
	struct {
		unsigned eos : 1;
		unsigned exp : 3;
		unsigned label : 20;
		unsigned none : 8;
	};
	uint32_t data;
} ff_mpls_label_t;

typedef struct ff_mpls_stack_s { ff_mpls_label_t id[10]; } ff_mpls_stack_t;

/**
 * \typedef ffilter interface return codes
 */
typedef enum {
	FF_OK = 0x1,				/**< No error occuried */
	FF_ERR_NOMEM = -0x1,
	FF_ERR_UNKN  = -0x2,
	FF_ERR_UNSUP  = -0x3,
	FF_ERR_OTHER  = -0xE,
	FF_ERR_OTHER_MSG  = -0xF, 	/**< Closer description of fault can be received from ff_error */
} ff_error_t;

/**
 * \typedef ffilter lvalue options
 *
 */
typedef enum {
	FF_OPTS_NONE = 0,
	FF_OPTS_FLAGS = 0x01,		/**< Item is of flag type, this change behaviour when no operator is set to bit compare */
	FF_OPTS_MPLS_LABEL = 0x08,
	FF_OPTS_MPLS_EOS = 0x04,
	FF_OPTS_MPLS_EXP = 0x02,
	FF_OPTS_CONST = 0x10,

} ff_opts_t;


/* supported operations */
typedef enum {
	FF_OP_UNDEF = -1,
	FF_OP_NOT = 1,
	FF_OP_OR,
	FF_OP_AND,
	FF_OP_IN,
	FF_OP_YES,

	FF_OP_NOOP,
	FF_OP_EQ,
	FF_OP_LT,
	FF_OP_GT,
	FF_OP_ISSET,
	FF_OP_EXIST
} ff_oper_t;

extern const char* ff_oper_str[FF_OP_EXIST+1];
extern const char* ff_type_str[FF_TYPE_TIMESTAMP_BIG+1];

/** \brief External identification of value */
typedef union {
	uint64_t index;       /**< Index mapping      */
	const void *ptr;      /**< Direct mapping     */
} ff_extern_id_t;

/** \brief Identification of left value */
typedef struct ff_lvalue_s {
	/** Type of left value */
	ff_type_t type;
	/** External identification of data field */
	ff_extern_id_t id[FF_MULTINODE_MAX];
	/** Extra options that modiflies evaluation of data */
	//TODO: Clarify purpose, maybe create getters and setters
	int options;
	/** 0 for not set */
	int n;
	const char * literal;

} ff_lvalue_t;

/* node of syntax tree */
typedef struct ff_node_s {
	ff_extern_id_t field;         /* field ID */
	//TODO: in future use only pointers - do not copy data from wrapper
	char *value;                  /* buffer allocated for data */
	size_t vsize;                 /* size of data in value */
	//TODO: could be ommited in future if pointer to function to evaluate is used instead
	int type;                     /* data type for value */
	ff_oper_t oper;               /* operation */
	//TODO: get rid of it mpls can have multiple operators, transcoding can be done via lvalue
	int opts;                     /* mpls stack data selector label, exp or eos */

	//TODO: transform to heap data structure - no pointers
	struct ff_node_s *left;
	struct ff_node_s *right;

} ff_node_t;

//typedef struct ff_s ff_t;
struct ff_s;

/**{@ \section ff_options_t
 *	Clarify purpose of options object in filter
 */

/**
 * \typedef Lookup callback signature.
 * \brief Lookup the field name found in filter expresson and identify its type one of and associated data elements
 * Callback fills in information about field into ff_lvalue_t sturcture. Required information are external
 * identification of field as understood by data function, data type of filed as one of ff_type_t enum
 * \param[in] filter Filter object
 * \param[in] fieldstr Name of element to identify
 * \param[out] lvalue identification representing field
 * \return FF_OK on success
 */
typedef ff_error_t (*ff_lookup_func_t) (struct ff_s *filter, const char *fieldstr, ff_lvalue_t *lvalue);

/**
 * \typedef Data Callback signature
 * \brief Select requested data from record.
 * Callback copies data associated with external identification, as set by lookup callback, from evaluated record
 * to buffer and marks length of these data. Structure of record must be known to data function.
 * \param ff_s Filter object
 * \param[in] record General data pointer to record
 * \param[in] id Indentfication of data field in recrod
 * \param[out] buf Buffer to store retrieved data
 * \param[out] vsize Length of retrieved data
 */
typedef ff_error_t (*ff_data_func_t) (struct ff_s*, void *, ff_extern_id_t, char*, size_t *);

/**
 * \typedef Rval_map Callback signature
 * \brief Translate constant values unresolved by filter convertors.
 * Callback is used to transform literal value to given ff_type_t when internal conversion function fails.
 * \param ff_s Filter object
 * \param[in] valstr String representation of value
 * \param[in] type Required ffilter internal type
 * \param[in] id External identification of field (form transforming exceptions like flags)
 * \param[out] buf Buffer to copy data
 * \param[out] size Length of valid data in buffer
 */
typedef ff_error_t (*ff_rval_map_func_t) (struct ff_s *, const char *, ff_type_t, ff_extern_id_t, char*, size_t* );

/** \typedef Filter options callbacks  */
typedef struct ff_options_s {
	/** Element lookup function */
	ff_lookup_func_t ff_lookup_func;
	/** Value comparation function */
	ff_data_func_t ff_data_func;
	/** Literal constants translation function eg. TCP->6 */
	ff_rval_map_func_t ff_rval_map_func;
} ff_options_t;

/**@}*/

/** \brief Filter object instance */
typedef struct ff_s {

	ff_options_t    options;	/**< Callback functions */
	void            *root;		/**< Internal representation of filter expression */
	char            error_str[FF_MAX_STRING];	/**< Last error set */

} ff_t;

/**
 * \brief Options constructor
 * allocates options structure
 * \param[in] ff_options
 * \return FF_OK on success
 */
ff_error_t ff_options_init(ff_options_t **ff_options);

/**
 * \brief Options destructor
 * frees options structure
 * \param[out] ff_options Address of pointer to options
 * \return FF_OK on success
 */
ff_error_t ff_options_free(ff_options_t *ff_options);

/**
 * \brief Create filter structure and compile filter expression using callbacks in options
 * First filter object is created then expr is compiled to internal representation.
 * Options callbacks provides following:
 * Lookup identifies the valid lvalue field names and associated filed data types.
 * Data callback sellects associated data for each identificator during evaluation
 * Rval_map callback provides translations to literal constants in value fileds eg.: "SSH"->22 etc.
 * \param[out] ff_filter Address of pointer to filter object
 * \param[in] expr Filter expression
 * \param[in] ff_options Associated options containig callbacks
 * \return FF_OK on success
 */
ff_error_t ff_init(ff_t **filter, const char *expr, ff_options_t *ff_options);

/**
 * \brief Evaluate filter on data
 * \param[in] ff_filter Compiled filter object
 * \param[in] rec Data record in form readable to data callback
 * \return Nonzero on match
 */
int ff_eval(ff_t *filter, void *rec);

/**
 * \brief Release memory allocated for filter object and destroy it
 * \param[out] filter Compiled filter object
 * \return FF_OK on success
 */
ff_error_t ff_free(ff_t *filter);

/**
 * \brief Set error string to filter object
 * \param[in] filter Compiled filter object
 * \param[in] format Format string as used in printf
 * \param[in] ...
 */
void ff_set_error(ff_t *filter, char *format, ...);

/**
 * \brief Retrive last error set form filter object
 * \param[in] filter Compiled filter object
 * \param[out] buf Place where to copy error string
 * \param[in] buflen Length of buffer available for error string
 * \return Pointer to copied error string
 */
const char* ff_error(ff_t *filter, const char *buf, int buflen);


#endif /* _LNF_FILTER_H */
