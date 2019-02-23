Zend Overload
=============

This is a __work in progress__ extension that is the result of researching instrumenting functions in the JIT coming to PHP.

Implementation
==============

The overloader will parse an ini file on startup, set in environment `ZEND_OVERLOADS`, the ini file instructs overloader how to overload target functions at runtime.

Supported

  - Overloading internal function with user function
  - Overloading user function with user function
  - Overloading internal function with internal function
  - Overloading user function with an internal one
  
So that the instrument (overload function) may invoke the target function, the target function is passed as a closure as the first argument to the instrument:

```php
function fooOverload($foo, $a, $b) {
    /* may invoke $foo */
}

function foo($a, $b) {
    /* ... */
}
```

This is unfinished, methods overloading is not fully implemented but the mechanism will work for that too.

I'm not really happy with the INI approach either, but didn't want to introduce a user API, because it may complicate matters ... I think the INI approach can be made good, but I've never used to built in INI parser ... possibly yaml is a better format for configuration ...

I don't like the name, it probably shouldn't have the word Zend in it ...

This has been tested with the JIT as it currently stands, and works with it when jit is enabled and disabled, it has not been tested extensively with any other build of PHP ... so expect it to break anywhere else ...

Notes
-----
Optimizer does several things that make this difficult, it ignores compiler flags and makes optimizations based on assumptions that may not be true. As a aresult of this, we must disable passes that are incompatible, although I hope inconsistencies between Zend and Opcache is fixed.

The default JIT level also makes the same assumptions, although I hope inconsistencies between VM and JIT can also be fixed.

__**THIS IS RESEARCH, DO NOT DEPLOY THIS EXTENSION**__
