# Navek Fiber (Enterprise Service Bus)

NAVEK FIBER is a high-performance enterprise integration bus. The main components of the NAVEK FIBER CORE are:
- FiberBUS - integration bus core
- FiberQUEUE - external modules that implement the functionality of message queues or specific functions
- FiberSAPI - connectors to programming languages, data warehouses etc.

Main features:
- fast deployment
- simple and clear endpoints description mechanism
- various types of message queues (synchronous, asynchronous)
- ability to implement endpoint as a message handler in high-level programming languages (Python, PHP, LUA, C++)
- flexibility and extensibility of the functional due to the modular architecture
- ability to use internal mechanisms for intermediate storage and data caching (InMemory key-value storage, Tarantool)
- ability to set the message processing chain
- interaction via the HTTP (RESTful) protocol
- queues and processes monitoring (RESTful API)
- through registration of each message, the ability to track the processing
- ability to control throughput for the enterprise systems, you can set different kind of limits by enterprise systems - for requests, calls, number of simultaneous requests and others
