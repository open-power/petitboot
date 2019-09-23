#ifndef _PB_ELF_H
#define _PB_ELF_H

#include <elfutils/libdw.h>
#include <libelf.h>

/*
 * The PowerPC namespace in an ELF Note of the kernel binary is used to store
 * capabilities and information which can be used by a bootloader or userland
 *
 * docs: Documentation/powerpc/elfnote.rst
 */
#define POWERPC_ELFNOTE_NAMESPACE "PowerPC"

/*
 * The capabilities supported/required by the kernel
 * This type uses a bitmap as "desc" field.
 */
#define PPC_ELFNOTE_CAPABILITIES 0x1

/* bitmap fields: */
#define PPCCAP_ULTRAVISOR_BIT 0x1

Elf *elf_open_image(const char *image);
void *elf_getnote_desc(Elf *elf,
		const char *namespace,
		uint32_t type);

#endif /* _PB_ELF_H */
