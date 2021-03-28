FALCON
======

This repo contains the implementation of Falcon (<u>F</u>ast and B<u>al</u>anced <u>Co</u>ntainer <u>N</u>etworking) as presented in the paper
"Parallelizing Packet Processing in Container Overlay Networks" accepted in
[EuroSys 2021](//2021.eurosys.org/).

This implementation is based on the original Linux kernel v5.4.64. Most of the files are unchanged. We've modified some parts of the networking subsystem to implement the ideas of Falcon.

The main modifications are as follows:

- `drivers/net/ethernet/mellanox/mlx5/core/en_rx.c`<br>
  Packet type identification and GROsplit decision. For now, we only implement Falcon for the network driver we used (`mlx5`).

- `fs/proc/stat.c`<br>
  Adds some new proc files to configure Falcon dynamically from userspace.

- `include/linux/kernel_stat.h`<br>
  Adding some extra per-CPU variables, usef for load balancing.

- `include/linux/skbuff.h`<br>
  Adds few fields to the `struct sk_buff` structure to store packet type.

- `kernel/time/timekeeping.c`<br>
  Implements the function to measure and update CPU load statistics periodically.

- `net/core/dev.c`<br>
  Implements the Falcon softirq splitting and balancing algorithm.

- `net/core/gro_cells.c`<br>
  Enables Falcon for the generic `gro_cell` driver that is used by software bridges.

## Experimental Results

We are working on preparaing the instructions and scripts to perform the experiments presented in the paper. Once completed, we will add them here.

## Authors

- Jiaxin Lei \<jlei23@binghamton.edu>
- Manish Munikar \<manish.munikar@uta.edu>
- Kun Suo \<ksuo@kennesaw.edu>
- Hui Lu \<huilu@binghamton.edu>
- Jia Rao \<jia.rao@uta.edu>