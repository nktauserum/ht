# ht

[EN](https://github.com/nktauserum/ht) / [RU](https://github.com/nktauserum/ht/blob/main/README_RU.md)

Literally **a hash table & rw-mutex over HTTP**. This is an educational project, created to better understand low-level processes below abstraction levels. So, not perfect. It uses only the necessary dependencies: stdlib, posix threads and linux-specific libraries.

Hashing is performed by the **djb2 algorithm**, collisions are handled with linear probing in the not beautiful way. Server is built according to the scheme producer-consumer: the main thread receives incoming requests from **epoll** in a non-blocking way and puts them into a **ring buffer**.

These requests are distributed between **worker threads** of a configurable count. Requests are parsed by them and interact with hash-table. A separate thread is responsible for tracking the expiration of records. There is a read-write mutex, of course.

All buffers are preallocated, there's no dynamic resizing - only compile-time configuration. 

## Building and running

*This project has been tested only on Linux.*

1. Clone this repository.
2. `make run`


## To-do

- More concize logs
- Some persistence
- Basic authorization 
- SSL
