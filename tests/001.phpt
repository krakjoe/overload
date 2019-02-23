--TEST--
overloading user function with user function
--ENV--
ZEND_OVERLOADS=tests/001-overloads.ini
--FILE--
<?php
function fooOverload($foo) {
    printf("Entering Foo\n");
    $retval = $foo();
    printf("Leaving Foo\n");
    return $retval;
}

function foo() {
    return false;
}

var_dump(foo());
?>
--EXPECT--
Entering Foo
Leaving Foo
bool(false)

