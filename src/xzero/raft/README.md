
# raft::Server

raft::Server allows you to implement replicated finite state machine.

## Design

The intend of this API is to provide an easy mechanism to implement
highly available replicated finite state machines, so that you can implement
your app on top of it.

This is why I left almost everything adaptable.

It is recommended to run your distributed system in a 5-node cluster,
so that up to 2 servers can fail.

## Algorithm

The core of this little library is the Raft algorithm.

## Service Discovery

- StaticDiscovery: manually configure peer locations.
- DnsDiscovery: perform peer discovery based on DNS SRV and A records.

## Peer Communication

- LocalChannel: performs communication to in-process reachable peers. Used for testing and debugging only.
- TcpChannel: performs communication over the network, via TCP/IP.

## Storage

- InMemoryStorage: implements in-process memory based storage. Used for testing and debugging only.
- OnDiskStorage: implements storage for your local file system.

## Implementation

In your distributed application, you need to instanciate `raft::Server` and
pass it your customized behaviors (storage, discovery, ...) as well as your
finite state machine that this `raft::Server` has to apply the commands on.

## Unit Test Cases

* [ ] If one server’s current term is smaller than the other’s, then it updates its current term to the larger value
* [ ] If a candidate or leader discovers that its term is out of date, it immediately reverts to follower state.
* [ ] If a server receives a request with a stale term number, it rejects the request.

...

## NOTES

```
s1.send(VoteRequest)            -- sends request to each peer
  for (Peer& peer: peers())
    peer.send(VoteRequest);

peer.received(VoteRequest)
  remote.send(VoteResponse)

s1.send(VoteRequest)
  s2.receive(VoteRequest)
    s2.send(VoteResponse)
      s1.receive(VoteResponse)
  s3.receive(VoteRequest)
    s3.send(VoteResponse)
      s1.receive(VoteResponse)
  s4.receive(VoteRequest)
    s4.send(VoteResponse)
      s1.receive(VoteResponse)
  s5.receive(VoteRequest)
    s5.send(VoteResponse)
      s1.receive(VoteResponse)


```
