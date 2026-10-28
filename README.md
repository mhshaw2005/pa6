# READ ME 
### PA6 for comp sci - Multi Threaded DNS resolver

## Array.c
array_init - Initialize the array with 0 at all positions. Also have conditions to ensure that the array is successfully initialized otherwise safely destroys it.

array_destroy - destroys the arrays mutex locks and conditional statments.

array_put - Starts by creating a mutex lock. Makes a thread wait if there is not any space in the array currently. Then copy the strings into the buffer to then remove it from the array, and signal that there is an empty spot now.

array_get - Starts by creating a mutex lock. Makes a thread wait if there is nothing in the array. Then copies out from the buffer putting it into the array, and sending its corresponding signals.

array_close - Safely closes the array.

## Multilookup.c
elapsed - initializes the time count for the program.

log_err - Logs an error to the console, uses a mutex lock.

requester - Starts by creating a mutex lock and fetching the next unserviced file. Opens file with error handling. Once it has a new line we ensure it is the correct format and the correct length before working with them. We then put them into the array using locks to ensure that nothing is accessed by multiple threads.

resolver - enters array and parses through. Attempts to lookup the service using dnslookups. Uses locks to properly output the correct print statements. Once the thread has outputted all of the services in the array it will output its total runtime and unlock all relevant locks.  

parse_count - Ensures that all of the counts match and are correct. otherwise outputs what was not matching. 

main - Runs the actual program. Starts by validating the file and ensuring all of the programs inputs are correct. Attempts to open and init the array with the files, outputting errors as needed. init all mutex conditions after that init start values. Starts by creating resolver threads to ensure there is data before the requester starts to request. once the resolvers are done it starts its requester. Check for any errors. Then we join the requester threads then we join the resolver threads. Then give total runtime output as well as freeing all memory properly and handling any final errors. 
