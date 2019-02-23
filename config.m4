dnl config.m4 for extension overload

PHP_ARG_ENABLE(overload, whether to enable overload support,
[  --enable-overload          Enable overload support], yes)

if test "$PHP_OVERLOAD" != "no"; then
  PHP_NEW_EXTENSION(overload, 
    php_overload.c, 
    shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1,, yes)
fi
