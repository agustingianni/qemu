# Qemu virtio-net

Virtio Network Device

## virtio

- Communication from guest to host via queues

### VirtIODevice

- Describes the device.

### VirtIONet

- Contains `VirtIODevice`

## What does the device do?

- Register some interface so the `OS` can send data to the virtual network device
  - How do we initialize this device from our `PoC`?
    - What is the mechanism of communication
      - There are two methods active at the same time
        - Modern
        - Legacy
      - They define PCI regions
        - virtio_pci_modern_regions_init(proxy, vdev->name);
          - memory_region_init_io
            - memory_region_init
        - There are a couple of them
      - The address seems to be set by `pci_register_bar`
      - IO Port?
        - Number?
      - DMA?
  - How do we send data?

## Potential bugs

### virtio_net_handle_ctrl

#### Linux implementation

https://elixir.bootlin.com/linux/latest/source/drivers/net/virtio_net.c#L1777

1. Add the control header to the scatter gather list.
2. Add the payload to the scatter gather list.
3. Add the scatter gather list to the control queue.
4. Kick.
    1. This sends the data via an ioport.
5. Read the result.

#### Trigger `virtio_net_handle_ctrl`

We need a way to send data to `ctrl_vq`.

```cpp
n->ctrl_vq = virtio_add_queue(vdev, 64, virtio_net_handle_ctrl);
```

1	virtio_net_handle_ctrl 	        virtio-net.c:1419:13
2	virtio_net_handle_mq 	        virtio-net.c:1374:12
3	virtio_net_set_status 	        virtio-net.c:363:13
4	virtio_net_drop_tx_queue_data 	virtio-net.c:355:13
5	virtqueue_drop_all 	            virtio.c:1775:14
6	virtqueue_split_drop_all 	    virtio.c:1741:21
7	vring_set_avail_event 	        virtio.c:407:20
8	vring_get_region_caches 	    virtio.c:277:33