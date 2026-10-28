# READ ME 
### PA6 for comp sci - Multi Threaded DNS resolver

## Array.c
array_init - Initialize the array with 0 at all positions. Also have conditions to ensure that the array is successfully initialized otherwise safely destroys it.

array_destroy - destroys the arrays mutex locks and conditional statments.

array_put - Starts by creating a mutex lock. Makes a thread wait if there is not any space in the array currently. Then copy the strings into the buffer to then remove it from the array, and signal that there is an empty spot now.

array_get - Starts by creating a mutex lock. Makes a thread wait if there is nothing in the array. Then copies out from the buffer putting it into the array, and sending its corresponding signals.

array_close - Safely closes the array.

## Multilookup.c
elapsed - 

log_err -

requester - 

resolver - 

parse_count - 

main - 
