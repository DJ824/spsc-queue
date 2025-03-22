this is a single producer single consumer lock free queue 

implementation is based on a ring buffer with certain performance optimizations: 

- keep the head and tail in different cache lines, without it, we know that only 1 core can have exlusive ownership of the entire cache line, this causes ping ponging back and forth between the 2 cores, as they would keep having to transfer ownership, invalidate the old state, update new state, and so forth 

- cache head and tail indices to reduce cache trafficking. in a standard enqueue op, we first need to check if the queue full, we do this be checking if the tail + 1 slot is the same as the head. when the producer core accesses the head, the state is stored in the shared cache. however, when a consumer tries to dequeue, it needs to hold the state of the head in its exclusive cache, as it means to write to that location. so, when this happens, one core holds an exclusive state, and the other holds a shared state, when the exclusive state is modified, the cpu has to update this state to all cores that also have access, whether it is shared or exclusive, which takes many clock cycles. to fix this, we store the state of the head and tail in temporary caches, and only update them when the queue is full/empty

example) 
head = 0, tail = 4 
consumer dequeues -> tail_cache = 4, so we can keep dequeing until the head == tail_cache, and then we can check to see if there are more items to dequeue by updating the tail cache, without this, we would be doing tail.load() every deqeue operation, 

- another optimization technique is to add padding at the front and end of the queue to avoid false sharing with any adjacent memory allocations. the padding size is determined by how many queue slots fit into a single cache line, this avoids the head and tail from being in the same cache line as any other allocation your program might do, based on the original location the queue was allocated



![image](https://github.com/user-attachments/assets/81062fe4-fb20-4291-ae5f-f96b354d1701)


ref: https://rigtorp.se/ringbuffer/
