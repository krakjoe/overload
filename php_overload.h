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
#ifndef PHP_OVERLOAD_H
# define PHP_OVERLOAD_H

# define ZEND_OVERLOAD_EXTNAME   "Zend Overload"
# define ZEND_OVERLOAD_AUTHORS   "Joe Watkins"
# define ZEND_OVERLOAD_VERSION   "0.0.1"
# define ZEND_OVERLOAD_HELPERS   "http://github.com/krakjoe/overload"
# define ZEND_OVERLOAD_COPYRIGHT "The PHP Group"

# ifndef ZEND_OVERLOAD_EXPORT
#  ifdef ZEND_WIN32
#   define ZEND_OVERLOAD_EXPORT __declspec(dllexport)
#  elif defined(__GNUC__) && __GNUC__ >= 4
#   define ZEND_OVERLOAD_EXPORT __attribute__ ((visibility("default")))
#  else
#   define ZEND_OVERLOAD_EXPORT
#  endif
# endif

# if defined(ZTS)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

#endif	/* PHP_OVERLOAD_H */
