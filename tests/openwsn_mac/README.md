# OpenWSN MAC on RIOT
This test demonstrates how to run the
[OpenWSN](https://github.com/openwsn-berkeley/openwsn-fw)  MAC layer
(IEEE802.15.4E TSCH) isolated from the other components.

Please note that this port is still in experimental status. For further information about the port,
currently supported platforms as well as issues, TODOs and debugging mechanisms, please refer to
the documentation](../../pkg/openwsn/doc.txt).

## Experimental setups
The following experiments act as a starting point for testing and debugging. Either
build and flash local nodes or incorporate the [FIT IoT-LAB](https://www.iot-lab.info/)
Testbed. Please check the ports [documentation](../../pkg/openwsn/doc.txt) for information
about supported hardware platforms.

A TSCH network requires at least one node running as PAN Coordinator, and one or
more nodes running as Coordinator (parent nodes than can advertise themselves so
othe nodes can join the TSCH network) or Leaf (end devices that can only join the
network, usually low power constrained devices).

## Start a TSCH network

1. Use the `cell` command to add an advertisement cell in the PAN coordinator:
```
Usage: cell <add|rem> <slot_offset> <channel_offset> <adv|tx|rx> [neighbour_address]
```

E.g to add an advertisement cell in `slot_offset` 0 and `channel_offset` 0:
```
cell add 0 0 adv
```
2. Set the role of the PAN coordinator to `pancoord`:
```
role pancoord
```

3. Wait for some seconds. Nodes nearby should print "`Synchronized`" when they
joined the TSCH network.

## Send messages between nodes.

1. Read the 64 bit Link Layer address from the receiver:
```
pa
```

2. Send a message to that address using `txtsnd` command.
```
txtsnd <address> <message>
```
E.g
```
txtsnd 796518402211c74e "Hello World!"
```

If the address is not specified, the message will be multicasted to all nodes
in the line of sight of the sender

## Expand the network
To add more nodes outside of the boundaries of the PAN coordinator, simply add
coordinators in the line of sight of the PAN coordinator or regular coordinators.

To declare a node as a coordinator, use the `role` command
```
role coord
```

A node can be set to leaf again using the same command:
```
role leaf
```

## Cell management
Use the `cell` command to manage the cells

E.g to create a link in slot offset 50 and channel offset 20 that receives
data from address 79:65:18:40:22:11:c7:4e
```
cell add 50 20 rx 796518402211c74e
```

E.g to create a link in slot offset 50 and channel offset 20 that sends
data to address 79:65:18:40:22:11:aa:bb

```
cell add 50 20 tx 796518402211aabb
```

With this configuration when 79:65:18:40:22:11:c7:4e sends a packet to
79:65:18:40:22:11:aa:bb, it will be sent either in the advertisement slot
(anycast) or in the (50,20) slot.

It's also possible to add more advertisement cells. E.g
```
cell add 40 0 adv
```

To remove the recently created cell:
```
cell rem 40 0 adv
```

## Change slotframe length
Before starting the pan coordinator, set the frame length with the `slotframe`
command:
```
slotframe <number_of_slots>
```

By default, `number_of_slots` is set to 101. A lower number will decrease the
latency but reduce the number of available slots.
