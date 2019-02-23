--TEST--
overloading internal function with user function
--ENV--
ZEND_OVERLOADS=tests/002-overloads.ini
--FILE--
<?php
function microtimeOverload($microtime, $asFloat = false) {
    printf("Entering Microtime\n");
    $retval = $microtime($asFloat);
    printf("Leaving Microtime\n");
    return $retval;
}

var_dump(microtime(true));
?>
--EXPECTF--
Entering Microtime
Leaving Microtime
float(%f)

