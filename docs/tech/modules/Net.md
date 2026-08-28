---
tags: [module]
---

# Net

VirtIO network: probe MAC, send one UDP datagram, stop.

## Capabilities

- PCI `0x1AF4` / `0x1000` or virtio 1.0 `0x1041`.
- Print `net 52:54:00:12:34:56` (QEMU default in README).
- One Ethernet+IPv4+UDP `hi` to `10.0.2.2:5555`, dest MAC `52:55:0a:00:02:02`, then `udp sent`.

## How it is implemented

- [[kernel.virtio.c]] — `virtio_net_init` beside virtio-blk.
- QEMU: `-netdev user`, `-device virtio-net-pci`, `-object filter-dump` → `build/virtio-net.pcap`.

No DHCP, TCP, ARP stack, RX queue, or sockets. Those are parked in [plan.md](../../plan.md).

## See also

- [[architecture/Boundaries]]
- [[architecture/Boot Path]]
