Edmond Poon \
ejpoon \
Spring 2022 CSE130 asgn3

# Programming Assignment 3
This assignment aims to implement a server that accepts get, append, and put requests while logging all completed responses to a specified logfile and allowing for multiple clients at a time (multi-threaded).

## Building 

The server can be built at once via either of the commands below:
```
$ make all
```
Or 
```
$ make 
```
Or 
```
$ make httpserver
```

## Running

To run the server after compiling it, you can run the command:
```
$ ./httpserver [-t threads] [-l logfile] <port> 
```

## Files:
|File         |Description                                                                       | 
|:-----------:| -------------------------------------------------------------------------------- |
|httpserver.c |A source file that contains the main logic that comprises the server.             |
|node.c     |A source file that contains the functions that represent a hash table entry.             |
|node.h   |A header file that contains the declarations that are used to represent a hash table entry.             |
|bst.c     |A source file that contains the functions that implement the bst.             |
|bst.h   |A header file that contains the declarations that are used implement a bst.             |
|ht.c     |A source file that contains the functions that implement a hash table.             |
|ht.h   |A header file that contains the declarations that are used to implement a hash table.             |
|parser.c     |A source file that contains the functions that parse the request.             |
|parser.h   |A header file that contains the declarations that are used to parse the request.             |
|queue.c     |A source file that contains the functions that implement a bounded buffer.             |
|queue.h   |A header file that contains the declarations that are used to implement a bounded buffer.             |
|response.c |A source file that contains the functions used to send responses back to the client.             |
|response.h |A header file that contains the declarations that are used to send responses back to the client.             |
|Makefile     |A file that offers the ability to generate executables and clean generated files. |
|README.md    |A file that contains the descriptions of each file and its' role in this project. |


# Design Decisions
This sections describes the numerous design decisions I made while working on this assignment.

## Design 
To implement the server, I decided to first parse through the headers and request line to determine whether it may be a valid request. Splitting the request line into the three main sections (Method, URI, Version). For each section, I ensured they were valid (get, append, or put for method; 
a valid file path for URI; HTTP/1.1 for the version). Once the request line has been verified, I turn to the headers to ensure they follow the correct format (value: key). If I encounter the Content-Length header, which indicates a body, I save the length and read that number of bytes from the response
to represent the body of the response. Similarly, If I encounter the Request-Id header, I store the id until I log the request. If any errors were encountered in the process, I would send the corresponding error response back to the client. Otherwise, I send the correct response back to the client indicating success.

## Responses
|Status-Code  |Status-Phrase | 
|:-----------:| ------------ |
|200 |OK                     |
|201 |Created                |
|404 |Not Found              |
|500 |Internal Server Error  |

## Data Structures and Algorithms
I used a list as a buffer to speed up parsing through the response and a linked list to keep track of incoming requests that can't be processed yet. Additionally, I use a bounded buffer as a queue to help "queue" up work for each worker thread to take out from. Similar to the producer-consumer problem, the 
dispather thread pauses when the queue is full and same for the worker threads when the queue is empty. Lastly, I used a hash table paired with a binary search tree to associate files with their own read/write locks to prevent one writer for a file from blocking another writer to a different file.

## Coherency and Atomicity
To implement coherency and atomicity for the files, I simply give each file descriptor a lock (shared or exclusive). Put or Append requests get exclusive locks to prevent any readers from reading old data while Get requests get shared locks to allow multiple readers at a time. Additionally, I use a mutex when writing
to the logfile to prevent atomicity issues. To allow for multiple writers (each to different files), I associate a lock to each file that has been used before to differentiate between the different files. In this case, wanting to write to both files A and B would be allowed to run in parallel rather than having 
one write go before the other.


## Audit Logging
To implement the audit logging, I simply logged the valid entry every time I would send a response to the client. To ensure the logging would still be functional when ecnountering a sigterm, I flushed the buffer whenever I attempted to write to the log file
to ensure the entry is written before anything else can happen.

## Multi-Threading
To implement a multi-threaded server, I first created the desired number of threads which all slept while waiting for the main thread to notify them of an incoming request. Once a thread is notified, it updates the corresponding variables to keep track of every thread, including the total number of threads
currently running, and handles the incoming request. Once it begins handling the request, it notifies the main thread that it is now doing some thread-safe operation, in which the main thread will either set another thread with another incoming request or wait for a new request. Once a thread is finished
handling a request, it simply sleeps until it gets notified again by the main thread about a new request. 

If a connection doesn't send any data after a few seconds, the connection will be put on hold to allow another connection to start running. This prevents the possibility of starving other connections because a certain connection isn't nice. Note that to pause and resume the connections, I keep enough state to ensure
I can resume, including the headers I've read, the request line data, and more. Similarly, if a file has been locked by another thread, I push the waiting threads onto the queue to allow other requests that don't rely on that file to run.

Additionally, to account for more requests than available threads, I implemented a queue to store the connection file descriptor of the extra requests to be processed once threads become available. This way, all clients will eventually have their requests processed.

## Errors
To identify the specific errors that were required for this assignment, I thought through the issues that might break the program, such as invalid requests. Specifically, once I had completed the parsing and responses for valid requests, I spend a lot of time messing with the given resource binary in an 
attempt to match the invalid requests and how I should handle them. For example, URI's that use a file name as a directory results in an internal server error while URI's that don't exist yet are created. I found a tough time figuring out the different ways to "break" the resource binary, but when I did,
I would add them to a document to allow me to test all my future commits to ensure my server will still pass the separate issues that I encountered while developing.

## Efficiency
As for the efficiency note, I tested my program by timing the server's response with a variety of inputs and compared the time it took to parse the input with the given resource binary. I tried the server with binary files, extremely large files, and small files and compared it with the resources to get 
an  upper bound for efficiency. To improve on efficiency, I attempted to find ways to remove computations when possible and try to find better methods to parse the requests. Additionally, I looked for ways to parse the requests in an efficient manner to ensure large content lengths or files will be sent
or written in a timely manner without affecting the client.
