# Makefile
-pthread needs to be included in the flags

# Safety
Do not share variables!
The program needs to be architectured in a way that ensures we avoid race condition
A Mutex (mutual exclusion) can be used to ensures that only one thread at a time can access a variable/file


# Models
Dynamic Work Queue: Each task is assigned to the first available thread
Static Assignment thread: Each thread has a pre-assigned list of tasks

A Dynamic Work Queue sounds better as long as we can ensure Safety

It's apparently possible to do something like:
    -When a worker is available, it checks if their is no one else currently listening, if not it checks if any message has been received and handle them
    -If someone is already listening or if nothing was received, it just continues to the queue