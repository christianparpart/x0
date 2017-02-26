
# raft::Server

raft::Server allows you to implement replicated finite state machine.

## Execution Model

The Raft consensus module is multi threaded. 

* Follower:
  - peer: one thread per incoming connection; this can be the leader, or a candidate
* Candidate:
  - peer: one thread for each peer to talk to
* Leader:
  - apply-logs: thread to locally apply logs that are safe to apply
  - replicator: one worker thread for each follower to replicate logs to

## Design

The intend of this API is to provide an easy mechanism to implement
highly available replicated finite state machines, so that you can implement
your app on top of it.

This is why I left almost everything adaptable.

It is recommended to run your distributed system in a 5-node cluster,
so that up to 2 servers can fail.

## Durability

Every log entry is assigned a monotonically increasing sequenced number
with an ID space of `2^64` - so how long does that last?

`2^64` equals `18.446.744.073.709.551.616`, in short 1.8e19.

Assuming you consume sustained 100k IDs per second, your software still lasts
as long as 5.849.424 years - hm, let's think more big-data style,..

Let's assume you consume a sustained *100mio* IDs per second, your
software still lasts as long as 5.849 years.

My conclusion though will be, a unsigned 64 bit integer for identifying
each and every little historical change within a database surely looks
sufficient.

## Algorithm

The core of this little library is the Raft algorithm.

## Service Discovery

- StaticDiscovery: manually configure peer locations.
- DnsDiscovery: perform peer discovery based on DNS SRV and A records.

## Peer Communication

- LocalTransport: performs communication to in-process reachable peers. Used for testing and debugging only.
- InetTransport: performs communication over the network, via TCP/IP.

## Storage

- InMemoryStorage: implements in-process memory based storage. Used for testing and debugging only.
- OnDiskStorage: implements storage for your local file system.

## Implementation

In your distributed application, you need to instanciate `raft::Server` and
pass it your customized behaviors (storage, discovery, ...) as well as your
finite state machine that this `raft::Server` has to apply the commands on.

### Log Replication

- A log entry is "committed" when it is on stable storage on the majority of servers.
- Leader propagates committed entries to be applied to the cluster's FSMs
  by pushing its own commitIndex to the peers.

## Unit Test Cases

* [ ] If one server’s current term is smaller than the other’s, then it updates its current term to the larger value
* [ ] If a candidate or leader discovers that its term is out of date, it immediately reverts to follower state.
* [ ] If a server receives a request with a stale term number, it rejects the request.

...

## Leader-to-Follower I/O

- detect timeouts in message writes/reads
- constantly send AppendEntries RPC, and empty heartbeats in idle times
- until we're not leader anymore.
- on success: update `Server.followers[id].lastActivity` timestamp
- `Leader::Peer`

### Leader
- keeps leader-state and maintain follower communication

### Leader::Peer

```
LeaderReplicationThread::run() {
  while (server->isLeader()) {
    replicate();
  }
}

LeaderReplicationThread::replicate() {
  waitForEvents(); // commands or heartbeat timeout
  replicateOne();
}

LeaderReplicationThread::replicateOne() {
  transport_->send(peerId_, appendMessagesReq);
}
```

### Local File Storage

```
/cluster_id               cluster ID
/server_id                represents server ID

/voted_for                if exists and non-empty, contains the server that
                          this server voted for

/log.{BaseIndex}          log[] starting at given base index
/log.latest               -> symlink
/snapshot.{LatestIndex}   state machine snapshot up to given index
```

