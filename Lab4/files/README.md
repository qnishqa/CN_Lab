# Lab 4 — Concurrent Network Information Service

## Files
- `server.c` — TCP lookup server (one thread per client) + UDP status server (own thread), ports 9090/9091
- `client.c` — TCP client for hostname/type lookups
- `udp_status_client.c` — UDP client that sends `STATUS`
- `Makefile` — builds all three

## Build
```
make
```
or manually:
```
gcc -Wall -pthread -o server server.c
gcc -Wall -o client client.c
gcc -Wall -o udp_status_client udp_status_client.c
```

## Run
Terminal 1:
```
./server
```
Terminal 2, 3, ... (as many as you like, simultaneously):
```
./client 127.0.0.1
```
Enter a domain, then a record type (`A`, `CNAME`, `MX`, `NS`). Enter `EXIT` to disconnect.

Any terminal, at any time:
```
./udp_status_client 127.0.0.1
```

## Verified behavior
- Multiple clients can stay connected and query concurrently (each gets its own thread); tested with two simultaneous connections plus a mid-session STATUS query — server correctly reported "Connected clients: 2".
- Each client can run several queries before sending `EXIT`.
- Unmatched hostname/type returns `Record not found`.
- Server console logs each client's IP:port on connect/disconnect and each query it makes.

## Notes on the design
- TCP is a byte stream, not message-oriented, so `read_line()` reads byte-by-byte until `\n` — don't assume one `recv()` = one query, that's a classic bug in these labs.
- `client_count` is shared between the TCP-handling threads and the UDP thread, so it's protected by a mutex (`count_mutex`).
- Threads are `pthread_detach()`-ed so their resources are reclaimed automatically when a client disconnects — no need to `pthread_join()` from `main()`.

---

## Extension Homework — Wireshark (no submission required)

General setup: start `./server`, open Wireshark, select the loopback interface (`lo`) if testing on one machine, or the real NIC if client/server are on different hosts. Filter to `tcp.port == 9090` or `udp.port == 9091` to cut noise.

1. **Three-way handshake.** Start the capture, then run `./client`. You should see three packets: `SYN` (client → server, seq=x), `SYN, ACK` (server → client, seq=y, ack=x+1), `ACK` (client → server, ack=y+1). Note the client's ephemeral source port (assigned by the OS, typically 32768+) versus the server's fixed port 9090.

2. **Multiple concurrent clients.** Start 3+ `./client` instances before closing any of them. Filter `tcp.port == 9090` and look at the four-tuple (src IP, src port, dst IP, dst port) of each stream (Wireshark's *Statistics → Conversations* is useful here). All connections share the same destination IP/port (server:9090), but each has a distinct client-side ephemeral port — that four-tuple is exactly how the OS/server tells the connections apart even though they land on one listening socket.

3. **Application data over TCP.** Run a single query, e.g. `www.example.com` / `A`. Right-click a packet in that TCP stream → *Follow → TCP Stream*. You'll see the raw text `www.example.com A` sent by the client and `192.168.1.10` sent back by the server, with no separate "protocol" framing — the lookup protocol here is just plain text carried as TCP payload.

4. **TCP vs UDP.** Run `./udp_status_client`. Filter `udp.port == 9091`. You'll see exactly two packets: the `STATUS` request and the reply — no SYN/SYN-ACK/ACK before it, because UDP is connectionless.

5. **Two differences to note (example answers, confirm with your own capture):**
   - TCP shows connection-management packets (handshake, and FIN/ACK teardown on disconnect); UDP shows only the request and reply datagrams — no setup or teardown packets at all.
   - The TCP stream can carry multiple queries back-to-back on one connection (you can see several request/response exchanges inside one stream), while each UDP exchange is a single, independent request/response pair with no persistent connection state.
