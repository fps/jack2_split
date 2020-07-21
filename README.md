# jack2_split

A program that facilitates parallelism in serial jack graphs by introducing latency

# What?

If you have a jack processsing graph that looks like this:

<pre>
capture -> A -> B -> playback
</pre>

where A and B are jack clients, then A and B must be scheduled in series due to the linear dependency.

Running A and B serially might produce xruns (i.e. violate the scheduling deadline imposed by jackd) while running only A or only B might not.

<code>jack2_split</code> can be used to remedy the situation by using the following graph:

<pre>
capture -> A -> jack2_split -> B playback
</pre>

<code>jack2_split</code> breaks the serial dependency by registering two jack clients which respectively only have terminal input and output ports and copying the buffer from its inputs to its outputs _after_ the current process cycle. This introduces one additional period of latency into the graph, but allows jack2/jackdmp to schedule A and B in parallel (e.g. on two cores).
