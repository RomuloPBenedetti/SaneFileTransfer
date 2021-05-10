# SaneFileTransfere

This repository contains some Linux's file transference algorithms that explore 
Linux Dirty page fragility, at the moment it is mostly focused on transference to 
slow external devices and big single files.

## The Problem


Most linux distros out there allow programs to allocate huge amounts of dirty pages
before the kernel start writing them to disk and even bigger amounts until
it start forcing writing synchronously. If the system have 32 GB of RAM, Kernel 
start to force synchronous writes only after you have 6.4 GB of dirty pages, 
holy saint Mary Jane, Batman! :) 

Most of the times this runs really fine in modern days, the user setup is composed 
of relatively homogeneous speed devices. The problem start when you mix external,
unreliable and slow devices... (samba, pendrives, slow external HDDs, etc)

Most programs out there in Linux ecosystem (`rsync`, `cp`, `nautilus`, `dolphin`, etc) don't
take this in consideration and just throw everything to Kernel pagging to manage. Lets
see what happens in this scenario:

* A 32 GB RAM setup; 
* Transferring a 6 GB file;
* to a 40 MBps speed external device;

some problems you may face are:

* bigger than needed CPU usage.
* High pressure in source or and destiny device IO;
* A big cache drop to store 6GB of dirty data for some minutes;
* programs freeze, system may start trashing and in the worst case, goes down if you decide to issue an `sync` command;
* The transference reaching 100% only some seconds after it start, generating confusion;
* The transference "ending" before data really reach the external device, generating confusion when you try to umount;
* Longer total time to transfer file, due do Kernel dirty page scheduling overhead (generally when dirty page gets to 
  1+ GB sizes);

This is not a new problem, There is some discussion around external devices and dirty page limits problems from times
to times, an example is:
[The pernicious USB-stick stall problem](https://lwn.net/Articles/572911/)

## Possible Solutions

As seen in `lwn.net`, there are some ideas being discussed at kernel side that may or may not be implemented in the 
future, like separating dirty page limits per device, dividing the dirty page caching into some sort of writethrough and 
writeback caching system, one for normal partitions and other for external devices

### Adjusting Dirty Limits
  
It is possible to reduce dirty page limits tuning `vm.dirty_background_bytes` and `vm.dirty_bytes`. The downside is that
it affects system globally and so, affects all disks, including ones that may benefit from big dirty pages limits. In 
some cases it may affect sensitive programs that have tight IO demands like streaming/recording programs.

Tunning `BDI` (/sys/class/bdi/<bdi>/) devices `min_ratio` and `max_ratio` may help a bit but unfortunately, as this
setting only takes effect after we have more than (dirty_background_ratio+dirty_ratio)/2 dirty data on RAM, the effect
happens to be small if you are only writing to this device.

### Limiting And Controlling Transference In Userspace

The program operating the transference itself can avoid abusing dirty page caching by throttling the transference 
dynamically, it add a bit of context switch overhead that gets to be compensated by reduced page cache overhead in
kernel side. It makes transference progress more precise, reduce the amount of dirty pages awaiting to be written back 
to device and reduce system IO/CPU pressure, and avoids extra delays when unmounting the device. For optimal performance 
`sendfile()` syscall is used.

The transference start with a predefined buffer chunks size of 30MB and loops until transference is done. the buffer 
is dynamically adjusted acording to transference speed:

    buffer_size =speed * 2;

After each chunk, `fdsync()` is issued to request that data be writen back to device until transference is completed.

## Limnitations And Improvements For The Future

* Buffer size starting value and dynamic adjustment may get better (deice profile detection).
* Folders and multiple files transference.
* Network device transference.
* Other optimizations around copy_file_range, sparse files, parallel copying, etc.
* detection of best scenarios where to use or not `fdsync()`.

## How To Build And Use

    mkdir build
    cd build
    cmake ..
    make

Now you can use the executable generated in `build`:

    $ time ./sane_file_transfer --alg1_sync /home/rbenedetti/test.iso /run/media/rbenedetti/PortableHDD_NTFS/test.iso
    Witen: 3383MB | 100% | 47.65MBps | 71.00sec
    File copied successfully.
    
    real    1m11,069s
    user    0m0,000s
    sys     0m5,322s
    
    $ time ./sane_file_transfer --alg1_sync /home/rbenedetti/test.iso /run/media/rbenedetti/PortableHDD_NTFS/test.iso
    Witen: 3383MB | 100% | 241.64MBps | 14.00sec
    File copied successfully.
    
    real    0m15,303s
    user    0m0,000s
    sys     0m5,468s
    
    $ time ( ./sane_file_transfer --alg1_sync /home/rbenedetti/test.iso /run/media/rbenedetti/PortableHDD_NTFS/test.iso && sync )
    Witen: 3383MB | 100% | 225.53MBps | 15.00sec
    File copied successfully.
    
    real    1m16,286s
    user    0m0,002s
    sys     0m6,213s

rsync, for comparison:

    $ time rsync -avh --info=progress2 '/home/rbenedetti/test.iso' /run/media/rbenedetti/PortableHDD_NTFS/test.iso
    sending incremental file list
    test.iso
    3.55G 100%  162.03MB/s    0:00:20 (xfr#1, to-chk=0/1)
    
    sent 3.55G bytes  received 35 bytes  173.08M bytes/sec
    total size is 3.55G  speedup is 1.00
    
    real    0m20,925s
    user    0m11,743s
    sys     0m5,021s

    $ time ( rsync -avh --info=progress2 '/home/rbenedetti/test.iso' /run/media/rbenedetti/PortableHDD_NTFS/test.iso && sync )
    sending incremental file list
    test.iso
    3.55G 100%  152.21MB/s    0:00:22 (xfr#1, to-chk=0/1)
    
    sent 3.55G bytes  received 35 bytes  157.70M bytes/sec
    total size is 3.55G  speedup is 1.00
    
    real    1m16,316s
    user    0m11,570s
    sys     0m5,859s
    
