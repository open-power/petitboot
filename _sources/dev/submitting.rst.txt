Development and Submitting Patches
==================================

Petitboot is largely written in C and follows the `Linux kernel coding style <https://github.com/torvalds/linux/blob/master/Documentation/process/coding-style.rst>`_.

Development occurs on the `Petitboot mailing list <https://lists.ozlabs.org/listinfo/petitboot>`_.

Petitboot also has a `Patchwork instance <http://patchwork.ozlabs.org/project/petitboot/list/>`_ that watches the list.

Patch Guidelines
----------------

Patches should be sent to the mailing list generally following what you would see in `submitting-patches <https://github.com/torvalds/linux/blob/master/Documentation/process/submitting-patches.rst>`_

In general if you generate the patch with ``git format-patch`` or ``git send-email`` you should be fine.

Patches should have an obvious title and where necessary a clear commit message describing the changes.
Avoid lumping unrelated changes together, instead putting them in separate patches in a logical order.
If a patch is generally contained to one area (and it should be), it should generally be prefixed with the path of what it is changing, for example "discover/grub:" or "ui/ncurses:".

If sending a new revision of a patch update the title to mention the verson (hint: ``git format-patch -v 2``) and include a short changelog under the ``---`` describing what changed between versions. Check out the list archives for `some examples <https://lists.ozlabs.org/pipermail/petitboot/2018-November/001188.html>`_.

Stable Patches
--------------

Patches or upstream commits that need to be applied to a stable branch should be restricted to small, self-contained fixes as much as possible. Avoid backporting new features or invasive changes.
