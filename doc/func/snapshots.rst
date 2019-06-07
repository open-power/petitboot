.. _snapshots:

Snapshots
=========

By default Petitboot does not directly mount any block devices. Instead it uses the device-mapper snapshot_ device to mount an in-memory representation of the device. Any writes Petitboot or another part of the system may make to the device are written to memory rather than the physical device, providing a "real" read-only guarantee beyond just that provided by the filesystem.

In normal operation this is completely transparent but there are two scenarios where actual writes are desired:

.. _snapshot: https://www.kernel.org/doc/Documentation/device-mapper/snapshot.txt

Saving bootloader data
----------------------

Some configuration formats include directives to save some data before boot - like GRUB's ability to record the last option booted. Snapshots will prevent this but Petitboot can be configured to merge these writes back to the source device by enabling the ""Allow bootloader scripts to modify disks" option in the System Configuration screen.

Manual interaction
------------------

Any writes to a block device first mounted by Petitboot will by default be thrown away. If a user makes changes to a disk or USB device for example that they want to preserve they can tell Petitboot to merge these writes to the source device with the "sync" event. For example if the user had written something to the sda2 partition, in the shell they can run:

.. code-block:: none

   pb-event sync@sda2

Petitboot will handle the merging itself and remount the device read-only.

If desired snapshots can also be disabled via the "petitboot,snapshots?" parameter. For example:

.. code-block:: none

   nvram --update-config petitboot,snapshots?=false

As of the next boot Petitboot will mount block devices directly.
